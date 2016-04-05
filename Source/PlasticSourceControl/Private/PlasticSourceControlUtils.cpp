// Copyright (c) 2016 Codice Software - Sebastien Rombauts (sebastien.rombauts@gmail.com)

#include "PlasticSourceControlPrivatePCH.h"
#include "PlasticSourceControlUtils.h"
#include "PlasticSourceControlState.h"
#include "PlasticSourceControlModule.h"
#include "PlasticSourceControlCommand.h"

#if PLATFORM_LINUX
#include <sys/ioctl.h>
#endif


namespace PlasticSourceControlConstants
{
	/** The maximum number of files we submit in a single Plastic command */
	const int32 MaxFilesPerBatch = 50;
}

FScopedTempFile::FScopedTempFile(const FText& InText)
{
	Filename = FPaths::CreateTempFilename(*FPaths::GameLogDir(), TEXT("Plastic-Temp"), TEXT(".txt"));
	if(!FFileHelper::SaveStringToFile(InText.ToString(), *Filename, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogSourceControl, Error, TEXT("Failed to write to temp file: %s"), *Filename);
	}
}

FScopedTempFile::~FScopedTempFile()
{
	if(FPaths::FileExists(Filename))
	{
		if(!FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*Filename))
		{
			UE_LOG(LogSourceControl, Error, TEXT("Failed to delete temp file: %s"), *Filename);
		}
	}
}

const FString& FScopedTempFile::GetFilename() const
{
	return Filename;
}


namespace PlasticSourceControlUtils
{

// Launch the Plastic command line process and extract its results & errors
static bool RunCommandInternalRaw(const FString& InCommand, const FString& InPathToPlasticBinary, const FString& InRepositoryRoot, const TArray<FString>& InParameters, const TArray<FString>& InFiles, FString& OutResults, FString& OutErrors)
{
	int32 ReturnCode = 0;
	// start with the Plastic command itself ("status", "log", "chekin"...)
	FString FullCommand = InCommand;

	// Platic only needs the workspace path if no list of files provided
	if(!InRepositoryRoot.IsEmpty() && (0 == InFiles.Num()))
	{
		// Specify the Plastic workspace (the root)
		FullCommand += TEXT(" \"");
		FullCommand += InRepositoryRoot;
		FullCommand += TEXT("\" ");
	}

	// Append to the command all parameters, and then finally the files
	for(const auto& Parameter : InParameters)
	{
		FullCommand += TEXT(" ");
		FullCommand += Parameter;
	}
	for(const auto& File : InFiles)
	{
		FullCommand += TEXT(" \"");
		FullCommand += File;
		FullCommand += TEXT("\"");
	}
	// TODO does Plastic have a "--non-interactive" option, or detects when there are no connected standard input/output streams

// @todo: temporary debug logs
	UE_LOG(LogSourceControl, Warning, TEXT("RunCommandInternalRaw: '%s %s'"), *InPathToPlasticBinary, *FullCommand);
	FPlatformProcess::ExecProcess(*InPathToPlasticBinary, *FullCommand, &ReturnCode, &OutResults, &OutErrors);
	UE_LOG(LogSourceControl, Log, TEXT("RunCommandInternalRaw: ExecProcess ReturnCode=%d OutResults='%s'"), ReturnCode, *OutResults);
	if (!OutErrors.IsEmpty())
	{
		UE_LOG(LogSourceControl, Error, TEXT("RunCommandInternalRaw: ExecProcess ReturnCode=%d OutErrors='%s'"), ReturnCode, *OutErrors);
	}

	return ReturnCode == 0;
}

// Basic parsing or results & errors from the Plastic command line process
static bool RunCommandInternal(const FString& InCommand, const FString& InPathToPlasticBinary, const FString& InRepositoryRoot, const TArray<FString>& InParameters, const TArray<FString>& InFiles, TArray<FString>& OutResults, TArray<FString>& OutErrorMessages)
{
	bool bResult;
	FString Results;
	FString Errors;

	bResult = RunCommandInternalRaw(InCommand, InPathToPlasticBinary, InRepositoryRoot, InParameters, InFiles, Results, Errors);

#if PLATFORM_WINDOWS
	const TCHAR* pchDelim = TEXT("\r\n");
#else
	const TCHAR* pchDelim = TEXT("\n");
#endif
	Results.ParseIntoArray(OutResults, pchDelim, true);
	Errors.ParseIntoArray(OutErrorMessages, pchDelim, true);

	return bResult;
}

FString FindPlasticBinaryPath()
{
	return FString(TEXT("cm"));
}

bool CheckPlasticAvailability(const FString& InPathToPlasticBinary)
{
	bool bPlasticAvailable = false;

	FString InfoMessages;
	FString ErrorMessages;
	bPlasticAvailable = RunCommandInternalRaw(TEXT("version"), InPathToPlasticBinary, FString(), TArray<FString>(), TArray<FString>(), InfoMessages, ErrorMessages);

	return bPlasticAvailable;
}

// Find the root of the Plastic repository, looking from the GameDir and upward in its parent directories.
bool FindRootDirectory(const FString& InPathToGameDir, FString& OutRepositoryRoot)
{
	bool bFound = false;
	FString PathToPlasticSubdirectory;
	OutRepositoryRoot = InPathToGameDir;

	while(!bFound && !OutRepositoryRoot.IsEmpty())
	{
		PathToPlasticSubdirectory = OutRepositoryRoot;
		PathToPlasticSubdirectory += TEXT(".plastic"); // Look for the ".plastic" subdirectory present at the root of every Plastic repository
		bFound = IFileManager::Get().DirectoryExists(*PathToPlasticSubdirectory);
		if(!bFound)
		{
			int32 LastSlashIndex;
			OutRepositoryRoot = OutRepositoryRoot.LeftChop(5);
			if(OutRepositoryRoot.FindLastChar('/', LastSlashIndex))
			{
				OutRepositoryRoot = OutRepositoryRoot.Left(LastSlashIndex + 1);
			}
			else
			{
				OutRepositoryRoot.Empty();
			}
		}
	}
	if (!bFound)
	{
		OutRepositoryRoot = InPathToGameDir; // If not found, return the GameDir as best possible root.
	}
	return bFound;
}


void GetBranchName(const FString& InPathToPlasticBinary, const FString& InRepositoryRoot, FString& OutBranchName)
{
	bool bResults;
	TArray<FString> InfoMessages;
	TArray<FString> ErrorMessages;
	TArray<FString> Parameters;
	Parameters.Add(TEXT("--wkconfig"));
	Parameters.Add(TEXT("--nochanges"));
	Parameters.Add(TEXT("--nostatus"));
	bResults = RunCommandInternal(TEXT("status"), InPathToPlasticBinary, InRepositoryRoot, Parameters, TArray<FString>(), InfoMessages, ErrorMessages);
	if(bResults && InfoMessages.Num() > 0)
	{
		OutBranchName = InfoMessages[0];
	}
}

bool RunCommand(const FString& InCommand, const FString& InPathToPlasticBinary, const FString& InRepositoryRoot, const TArray<FString>& InParameters, const TArray<FString>& InFiles, TArray<FString>& OutResults, TArray<FString>& OutErrorMessages)
{
	bool bResult = true;

	if(InFiles.Num() > PlasticSourceControlConstants::MaxFilesPerBatch)
	{
		// Batch files up so we dont exceed command-line limits
		int32 FileCount = 0;
		while(FileCount < InFiles.Num())
		{
			TArray<FString> FilesInBatch;
			for(int32 FileIndex = 0; FileCount < InFiles.Num() && FileIndex < PlasticSourceControlConstants::MaxFilesPerBatch; FileIndex++, FileCount++)
			{
				FilesInBatch.Add(InFiles[FileCount]);
			}

			TArray<FString> BatchResults;
			TArray<FString> BatchErrors;
			bResult &= RunCommandInternal(InCommand, InPathToPlasticBinary, InRepositoryRoot, InParameters, FilesInBatch, BatchResults, BatchErrors);
			OutResults += BatchResults;
			OutErrorMessages += BatchErrors;
		}
	}
	else
	{
		bResult &= RunCommandInternal(InCommand, InPathToPlasticBinary, InRepositoryRoot, InParameters, InFiles, OutResults, OutErrorMessages);
	}

	return bResult;
}



/**
 * Extract and interpret the file state from the given Plastic "status" result.
 * empty string = unmodified or hidden changes
 CH Content\Changed_BP.uasset
 CO Content\CheckedOut_BP.uasset
 CP Content\Copied_BP.uasset
 RP Content\Replaced_BP.uasset
 AD Content\Added_BP.uasset
 PR Content\Private_BP.uasset
 IG Content\Ignored_BP.uasset
 DE Content\Deleted_BP.uasset
 LD Content\Deleted2_BP.uasset
 MV 100% Content\ToMove_BP.uasset -> Content\Moved_BP.uasset
 LM 100% Content\ToMove2_BP.uasset -> Content\Moved2_BP.uasset
*/
class FPlasticStatusParser
{
public:
	FPlasticStatusParser(const FString& InResult)
	{
		if (0 == InResult.Len()) // Empty result for "Controled" file (ie. Unchanged / Pristine / Clean)
		{
			State = EWorkingCopyState::Controled;
		}
		else
		{
			const FString FileStatus = InResult.Mid(1, 2);

			UE_LOG(LogSourceControl, Log, TEXT("FPlasticStatusParser('%s'[%d]): '%s'"), *InResult, InResult.Len(), *FileStatus);

			if (FileStatus == "CH") // Modified but not Checked-Out
			{
				State = EWorkingCopyState::Changed;
			}
			else if (FileStatus == "CO") // Checked-Out for modification
			{
				State = EWorkingCopyState::CheckedOut;
			}
			else if (FileStatus == "CP")
			{
				State = EWorkingCopyState::Copied;
			}
			else if (FileStatus == "RP")
			{
				State = EWorkingCopyState::Replaced;
			}
			else if (FileStatus == "AD")
			{
				State = EWorkingCopyState::Added;
			}
			else if (FileStatus == "PR")
			{
				State = EWorkingCopyState::NotControlled;
			}
			else if (FileStatus == "IG")
			{
				State = EWorkingCopyState::Ignored;
			}
			else if ((FileStatus == "DE") || (FileStatus == "LD"))
			{
				State = EWorkingCopyState::Deleted;
			}
			else if ((FileStatus == "MV") || (FileStatus == "LM")) // Renamed
			{
				State = EWorkingCopyState::Moved;
			}
			else if (FileStatus == "conflited") // TODO
			{
				// "Unmerged" conflict cases are generally marked with a "U",
				// but there are also the special cases of both "A"dded, or both "D"eleted
				State = EWorkingCopyState::Conflicted;
			}
			else
			{
				UE_LOG(LogSourceControl, Warning, TEXT("Unknown"));
				State = EWorkingCopyState::Unknown;
			}
		}
	}

	EWorkingCopyState::Type State;
};

/** Parse the array of strings results of a 'cm status --nostatus --noheaders --all --ignore' command
 *
 * Example cm fileinfo results:
 CH Content\Changed_BP.uasset
 CO Content\CheckedOut_BP.uasset
 CP Content\Copied_BP.uasset
 RP Content\Replaced_BP.uasset
 AD Content\Added_BP.uasset
 PR Content\Private_BP.uasset
 IG Content\Ignored_BP.uasset
 DE Content\Deleted_BP.uasset
 LD Content\Deleted2_BP.uasset
 MV 100% Content\ToMove_BP.uasset -> Content\Moved_BP.uasset
 LM 100% Content\ToMove2_BP.uasset -> Content\Moved2_BP.uasset
 */
static void ParseStatusResult(const FString& InPathToPlasticBinary, const FString& InRepositoryRoot, const FString& InFile, const TArray<FString>& InResults, TArray<FPlasticSourceControlState>& OutStates)
{
	// Assuming one line of results for one file
	const FString& Status = InResults[0];
	const FPlasticStatusParser StatusParser(Status);
	FPlasticSourceControlState FileState(InFile);
	FileState.WorkingCopyState = StatusParser.State;

	UE_LOG(LogSourceControl, Log, TEXT("%s: %s = %d"), *InFile, *Status, static_cast<uint32>(StatusParser.State));

	if(FileState.IsConflicted())
	{
		// In case of a conflict (unmerged file) get the base revision to merge
// TODO				RunGetConflictStatus(InPathToPlasticBinary, InRepositoryRoot, File, FileState);
	}
	FileState.TimeStamp.Now();
	OutStates.Add(FileState);
}

// Run a Plastic "status" command to update status of given files.
bool RunUpdateStatus(const FString& InPathToPlasticBinary, const FString& InRepositoryRoot, const TArray<FString>& InFiles, TArray<FString>& OutErrorMessages, TArray<FPlasticSourceControlState>& OutStates)
{
	TArray<FString> Results;
	TArray<FString> Parameters;
	Parameters.Add(TEXT("--nostatus --noheaders --all --ignored"));

	bool bResult = true;
	for(const FString& File : InFiles)
	{
		TArray<FString> ErrorMessages;
		TArray<FString> OneFile;
		OneFile.Add(File);
		const bool bStatusOk = RunCommand(TEXT("status"), InPathToPlasticBinary, InRepositoryRoot, Parameters, OneFile, Results, ErrorMessages);
		if (bStatusOk && (0 < Results.Num()))
		{
			ParseStatusResult(InPathToPlasticBinary, InRepositoryRoot, File, Results, OutStates);
		}
		else
		{
			OutErrorMessages.Append(ErrorMessages);
		}
	}

	return bResult;
}

bool UpdateCachedStates(const TArray<FPlasticSourceControlState>& InStates)
{
	FPlasticSourceControlModule& PlasticSourceControl = FModuleManager::LoadModuleChecked<FPlasticSourceControlModule>( "PlasticSourceControl" );
	FPlasticSourceControlProvider& Provider = PlasticSourceControl.GetProvider();
	int NbStatesUpdated = 0;

	for(const auto& InState : InStates)
	{
		TSharedRef<FPlasticSourceControlState, ESPMode::ThreadSafe> State = Provider.GetStateInternal(InState.LocalFilename);
		if(State->WorkingCopyState != InState.WorkingCopyState)
		{
			State->WorkingCopyState = InState.WorkingCopyState;
			State->PendingMergeBaseFileHash = InState.PendingMergeBaseFileHash;
		//	State->TimeStamp = InState.TimeStamp; // @todo Bug report: Workaround a bug with the Source Control Module not updating file state after a "Save"
			NbStatesUpdated++;
		}
	}

	return (NbStatesUpdated > 0);
}

/**
 * Helper struct for RemoveRedundantErrors()
 */
struct FRemoveRedundantErrors
{
	FRemoveRedundantErrors(const FString& InFilter)
		: Filter(InFilter)
	{
	}

	bool operator()(const FString& String) const
	{
		if(String.Contains(Filter))
		{
			return true;
		}

		return false;
	}

	/** The filter string we try to identify in the reported error */
	FString Filter;
};

void RemoveRedundantErrors(FPlasticSourceControlCommand& InCommand, const FString& InFilter)
{
	bool bFoundRedundantError = false;
	for(auto Iter(InCommand.ErrorMessages.CreateConstIterator()); Iter; Iter++)
	{
		if(Iter->Contains(InFilter))
		{
			InCommand.InfoMessages.Add(*Iter);
			bFoundRedundantError = true;
		}
	}

	InCommand.ErrorMessages.RemoveAll( FRemoveRedundantErrors(InFilter) );

	// if we have no error messages now, assume success!
	if(bFoundRedundantError && InCommand.ErrorMessages.Num() == 0 && !InCommand.bCommandSuccessful)
	{
		InCommand.bCommandSuccessful = true;
	}
}

}
