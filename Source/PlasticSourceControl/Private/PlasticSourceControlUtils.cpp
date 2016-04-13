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
#if PLATFORM_WINDOWS
	const TCHAR* pchDelim = TEXT("\r\n");
#else
	const TCHAR* pchDelim = TEXT("\n");
#endif
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

// Needed to SetHandleInformation() on WritePipe for input (opposite of ReadPipe, for output) (idem FInteractiveProcess)
static FORCEINLINE bool CreatePipeWrite(void*& ReadPipe, void*& WritePipe)
{
#if PLATFORM_WINDOWS
	SECURITY_ATTRIBUTES Attr = { sizeof(SECURITY_ATTRIBUTES), NULL, true };

	if (!::CreatePipe(&ReadPipe, &WritePipe, &Attr, 0))
	{
		return false;
	}

	if (!::SetHandleInformation(WritePipe, HANDLE_FLAG_INHERIT, 0))
	{
		return false;
	}

	return true;
#else
	return FPlatformProcess::CreatePipe(ReadPipe, WritePipe);
#endif // PLATFORM_WINDOWS
}

namespace PlasticSourceControlUtils
{
static void*		OutputPipeRead = nullptr;
static void*		OutputPipeWrite = nullptr;
static void*		InputPipeRead = nullptr;
static void*		InputPipeWrite = nullptr;
static FProcHandle	ProcessHandle;

// Launch the Plastic command line shell process in background for optimized successive commands
static bool LaunchBackgroundCommandLineShell(const FString& InPathToPlasticBinary)
{
	// only if shell not already running
	if (!ProcessHandle.IsValid())
	{
		const FString FullCommand(TEXT("shell"));

		const bool bLaunchDetached = false;				// the new process will NOT have its own window
		const bool bLaunchHidden = true;				// the new process will be minimized in the task bar
		const bool bLaunchReallyHidden = bLaunchHidden; // the new process will not have a window or be in the task bar

		verify(FPlatformProcess::CreatePipe(OutputPipeRead, OutputPipeWrite));	// For reading from child process
		verify(CreatePipeWrite(InputPipeRead, InputPipeWrite));	// For writing to child process

		UE_LOG(LogSourceControl, Warning, TEXT("LaunchBackgroundCommandLineShell: '%s %s'"), *InPathToPlasticBinary, *FullCommand);
		ProcessHandle = FPlatformProcess::CreateProc(*InPathToPlasticBinary, *FullCommand, bLaunchDetached, bLaunchHidden, bLaunchReallyHidden, nullptr, 0, nullptr, OutputPipeWrite, InputPipeRead);
		if (!ProcessHandle.IsValid())
		{
			UE_LOG(LogSourceControl, Warning, TEXT("Failed to launch 'cm shell'")); // not a bug, just no Plastic SCM cli found
			FPlatformProcess::CloseProc(ProcessHandle);
			FPlatformProcess::ClosePipe(InputPipeRead, InputPipeWrite);
			FPlatformProcess::ClosePipe(OutputPipeRead, OutputPipeWrite);
		}
	}

	return ProcessHandle.IsValid();
}

bool RunCommandInternalShell(const FString& InCommand, const TArray<FString>& InParameters, const TArray<FString>& InFiles, FString& OutResults, FString& OutErrors)
{
	bool bResult = false;

	if (ProcessHandle.IsValid())
	{
		// Start with the Plastic command itself ("status", "log", "chekin"...)
		FString FullCommand = InCommand;
		// Append to the command all parameters, and then finally the files
		for (const auto& Parameter : InParameters)
		{
			FullCommand += TEXT(" ");
			FullCommand += Parameter;
		}
		for (const auto& File : InFiles)
		{
			FullCommand += TEXT(" \"");
			FullCommand += File;
			FullCommand += TEXT("\"");
		}
		// TODO: temporary debug logs (before end of line)
		UE_LOG(LogSourceControl, Warning, TEXT("RunCommandInternalShell: '%s'"), *FullCommand);
		FullCommand += TEXT('\n'); // Finalize the command line

		// Send command to 'cm shell' process
		const bool bWriteOk = FPlatformProcess::WritePipe(InputPipeWrite, FullCommand);

		// And wait up to ten seconds for its termination
		const double Timeout = 10.0;
		const double StartTime = FPlatformTime::Seconds();
		while (FPlatformProcess::IsProcRunning(ProcessHandle) && (FPlatformTime::Seconds() - StartTime < Timeout))
		{
			FString Output = FPlatformProcess::ReadPipe(OutputPipeRead);
			if (0 < Output.Len())
			{
				OutResults.Append(MoveTemp(Output));
				// Search the ouptput for the line containing the result code, also indicating the end of the command
				const uint32 IndexCommandResult = OutResults.Find(TEXT("CommandResult "), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
				if (INDEX_NONE != IndexCommandResult)
				{
					const uint32 IndexEndResult = OutResults.Find(PlasticSourceControlConstants::pchDelim, ESearchCase::CaseSensitive, ESearchDir::FromStart, IndexCommandResult + 14);
					if (INDEX_NONE != IndexEndResult)
					{
						const FString Result = OutResults.Mid(IndexCommandResult + 14, IndexEndResult - IndexCommandResult - 14);
						int32 ResultCode = FCString::Atoi(*Result);
						bResult = (0 == ResultCode);
						// remove the CommandResult line from the OutResults
						OutResults.RemoveAt(IndexCommandResult, OutResults.Len() - IndexCommandResult);
						break;
					}
				}
			}
			FPlatformProcess::Sleep(0.0f); // 0.0 means release the current time slice to let other threads get some attention
		}
		UE_LOG(LogSourceControl, Log, TEXT("OutResults: '%s' bResult=%d Elapsed=%lf"), *OutResults, bResult, FPlatformTime::Seconds() - StartTime);
		
		// Return output as error if result code is an error
		if (!bResult)
		{
			OutErrors = MoveTemp(OutResults);
		}
	}
	else
	{
		UE_LOG(LogSourceControl, Error, TEXT("RunCommandInternalShell(%s): cm shell not running"), *InCommand);
		OutErrors = InCommand + ": Plastic SCM shell not running!";
	}

	return bResult;
}

bool CheckPlasticAvailability(const FString& InPathToPlasticBinary)
{
	// Launch the background 'cm shell' process if possible (and not already running)
	return LaunchBackgroundCommandLineShell(InPathToPlasticBinary);
}

// Terminate the background 'cm shell' process and associated pipes
void Terminate()
{
	if (ProcessHandle.IsValid())
	{
		// Tell the 'cm shell' to exit
		FString Results, Errors;
		RunCommandInternalShell(TEXT("exit"), TArray<FString>(), TArray<FString>(), Results, Errors);
		// And wait up to one seconde for its termination
		int timeout = 100;
		while (FPlatformProcess::IsProcRunning(ProcessHandle) && (0 < timeout--))
		{
			FPlatformProcess::Sleep(0.01f);
		}
		FPlatformProcess::CloseProc(ProcessHandle);
		FPlatformProcess::ClosePipe(InputPipeRead, InputPipeWrite);
		FPlatformProcess::ClosePipe(OutputPipeRead, OutputPipeWrite);
		OutputPipeRead = OutputPipeWrite = nullptr;
		InputPipeRead = InputPipeWrite = nullptr;
	}
}

// Basic parsing or results & errors from the Plastic command line process
static bool RunCommandInternal(const FString& InCommand, const TArray<FString>& InParameters, const TArray<FString>& InFiles, TArray<FString>& OutResults, TArray<FString>& OutErrorMessages)
{
	bool bResult;
	FString Results;
	FString Errors;

	bResult = RunCommandInternalShell(InCommand, InParameters, InFiles, Results, Errors);

	Results.ParseIntoArray(OutResults, PlasticSourceControlConstants::pchDelim, true);
	Errors.ParseIntoArray(OutErrorMessages, PlasticSourceControlConstants::pchDelim, true);

	return bResult;
}

FString FindPlasticBinaryPath()
{
	return FString(TEXT("cm"));
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

void GetBranchName(FString& OutBranchName)
{
	bool bResults;
	TArray<FString> InfoMessages;
	TArray<FString> ErrorMessages;
	TArray<FString> Parameters;
	Parameters.Add(TEXT("--wkconfig"));
	Parameters.Add(TEXT("--nochanges"));
	Parameters.Add(TEXT("--nostatus"));
	bResults = RunCommandInternal(TEXT("status"), Parameters, TArray<FString>(), InfoMessages, ErrorMessages);
	if(bResults && InfoMessages.Num() > 0)
	{
		OutBranchName = InfoMessages[0];
	}
}

bool RunCommand(const FString& InCommand, const TArray<FString>& InParameters, const TArray<FString>& InFiles, TArray<FString>& OutResults, TArray<FString>& OutErrorMessages)
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
			bResult &= RunCommandInternal(InCommand, InParameters, FilesInBatch, BatchResults, BatchErrors);
			OutResults += BatchResults;
			OutErrorMessages += BatchErrors;
		}
	}
	else
	{
		bResult &= RunCommandInternal(InCommand, InParameters, InFiles, OutResults, OutErrorMessages);
	}

	return bResult;
}

// debug log utility
static const TCHAR* ToString(EWorkspaceState::Type InWorkspaceState)
{
	const TCHAR* pLabel = nullptr;
	switch (InWorkspaceState)
	{
		case EWorkspaceState::Unknown: pLabel = TEXT("Unknown"); break;
		case EWorkspaceState::Ignored: pLabel = TEXT("Ignored"); break;
		case EWorkspaceState::Controled: pLabel = TEXT("Controled"); break;
		case EWorkspaceState::CheckedOut: pLabel = TEXT("CheckedOut"); break;
		case EWorkspaceState::Added: pLabel = TEXT("Added"); break;
		case EWorkspaceState::Moved: pLabel = TEXT("Moved"); break;
		case EWorkspaceState::Copied: pLabel = TEXT("Copied"); break;
		case EWorkspaceState::Replaced: pLabel = TEXT("Replaced"); break;
		case EWorkspaceState::Deleted: pLabel = TEXT("Deleted"); break;
		case EWorkspaceState::Changed: pLabel = TEXT("Changed"); break;
		case EWorkspaceState::Conflicted: pLabel = TEXT("Conflicted"); break;
		case EWorkspaceState::Private: pLabel = TEXT("Private"); break;
		default: pLabel = TEXT("???"); break;
	}
	return pLabel;
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

 TODO: Conflicted files?
*/
class FPlasticStatusParser
{
public:
	FPlasticStatusParser(const FString& InResult)
	{
		const FString FileStatus = InResult.Mid(1, 2);

		if (FileStatus == "CH") // Modified but not Checked-Out
		{
			State = EWorkspaceState::Changed;
		}
		else if (FileStatus == "CO") // Checked-Out for modification
		{
			State = EWorkspaceState::CheckedOut;
		}
		else if (FileStatus == "CP")
		{
			State = EWorkspaceState::Copied;
		}
		else if (FileStatus == "RP")
		{
			State = EWorkspaceState::Replaced;
		}
		else if (FileStatus == "AD")
		{
			State = EWorkspaceState::Added;
		}
		else if (FileStatus == "PR") // Not Controled/Not in Depot/Untraked
		{
			State = EWorkspaceState::Private;
		}
		else if (FileStatus == "IG")
		{
			State = EWorkspaceState::Ignored;
		}
		else if ((FileStatus == "DE") || (FileStatus == "LD")) // TODO: need to differentiate for CanEdit/CanCheckout/CanCheckIn?
		{
			State = EWorkspaceState::Deleted; // Deleted or Locally Deleleted (ie. missing)
		}
		else if ((FileStatus == "MV") || (FileStatus == "LM")) // Renamed TODO: need to differentiate for CanEdit/CanCheckout/CanCheckIn?
		{
			State = EWorkspaceState::Moved; // Moved/Renamed or Locally Moved
		}
		else if (FileStatus == "conflited") // TODO: what is the appropriate status?
		{
			// "Unmerged" conflict cases are generally marked with a "U",
			// but there are also the special cases of both "A"dded, or both "D"eleted
			State = EWorkspaceState::Conflicted;
		}
		else
		{
			UE_LOG(LogSourceControl, Warning, TEXT("Unknown"));
			State = EWorkspaceState::Unknown;
		}
	}

	EWorkspaceState::Type State;
};

/** Parse the array of strings results of a 'cm status --nostatus --noheaders --all --ignore' command
 *
 * Example cm status results:
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
static void ParseStatusResult(const FString& InFile, const TArray<FString>& InResults, FPlasticSourceControlState& OutFileState)
{
	// Assuming one line of results for one file.
	static const FString EmptyString;
	if (0 < InResults.Num())
	{
		const FString& Status = InResults[0];
		const FPlasticStatusParser StatusParser(Status);
		OutFileState.WorkspaceState = StatusParser.State;
	}
	else
	{
		// No result means Controled/Unchanged file
		OutFileState.WorkspaceState = EWorkspaceState::Controled;
	}
	// TODO debug log
	UE_LOG(LogSourceControl, Log, TEXT("%s = %d:%s"), *InFile, static_cast<uint32>(OutFileState.WorkspaceState), ToString(OutFileState.WorkspaceState));
	OutFileState.TimeStamp.Now();
}

// Run a "status" command for each file to get workspace states
static bool RunStatus(const TArray<FString>& InFiles, TArray<FString>& OutErrorMessages, TArray<FPlasticSourceControlState>& OutStates)
{
	bool bResult = true;

	TArray<FString> Status;
	Status.Add(TEXT("--nostatus"));
	Status.Add(TEXT("--noheaders"));
	Status.Add(TEXT("--all"));
	Status.Add(TEXT("--ignored"));

	for (const FString& File : InFiles)
	{
		OutStates.Add(FPlasticSourceControlState(File));
		FPlasticSourceControlState& FileState = OutStates.Last();

		TArray<FString> OneFile;
		OneFile.Add(File);
		TArray<FString> Results;
		TArray<FString> ErrorMessages;
		const bool bStatusOk = RunCommand(TEXT("status"), Status, OneFile, Results, ErrorMessages);
		if (bStatusOk)
		{
			ParseStatusResult(File, Results, FileState);
			if (FileState.IsConflicted())
			{
				// TODO In case of a conflict (unmerged file) get the base revision to merge
			}
		}
		else
		{
			OutErrorMessages.Append(ErrorMessages);
			bResult = false;
		}
	}

	return bResult;
}

// Parse the fileinfo output format "{RevisionChangeset};{RevisionHeadChangeset};{LockedBy}"
class FPlasticFileinfoParser
{
public:
	FPlasticFileinfoParser(const FString& InResult)
	{
		TArray<FString> Fileinfos;
		const int32 NbElmts = InResult.ParseIntoArray(Fileinfos, TEXT(";"));
		if (NbElmts >= 2)
		{
			RevisionChangeset = FCString::Atoi(*Fileinfos[0]);
			RevisionHeadChangeset = FCString::Atoi(*Fileinfos[1]);
			if (NbElmts >= 3)
			{
				LockedBy = MoveTemp(Fileinfos[2]);
			}
		}
	}

	int32 RevisionChangeset;
	int32 RevisionHeadChangeset;
	FString LockedBy;
};

/** Parse the array of strings results of a 'cm fileinfo --format="{RevisionChangeset};{RevisionHeadChangeset};{LockedBy}"' command
 *
 * Example cm fileinfo results:
16;16;False;;
14;14;False;;
16;16;False;;
*/
static void ParseFileinfoResults(const TArray<FString>& InFiles, const TArray<FString>& InResults, TArray<FPlasticSourceControlState>& InOutStates)
{
	// Iterate on all files and all status of the result (assuming no more line of results than number of files)
	for (int32 IdxResult = 0; IdxResult < InResults.Num(); IdxResult++)
	{
		const FString& File = InFiles[IdxResult];
		const FString& Fileinfo = InResults[IdxResult];
		FPlasticSourceControlState& FileState = InOutStates[IdxResult];
		FPlasticFileinfoParser FileinfoParser(Fileinfo);

		FileState.LocalRevisionChangeset = FileinfoParser.RevisionChangeset;
		FileState.DepotRevisionChangeset = FileinfoParser.RevisionHeadChangeset;
		FileState.LockedBy = MoveTemp(FileinfoParser.LockedBy);

		// TODO debug log
		UE_LOG(LogSourceControl, Log, TEXT("%s: %d;%d '%s'"), *File, FileState.LocalRevisionChangeset, FileState.DepotRevisionChangeset, *FileState.LockedBy);
	}
}

// Run a Plastic "fileinfo" (similar to "status") command to update status of given files.
static bool RunFileinfo(const TArray<FString>& InFiles, TArray<FString>& OutErrorMessages, TArray<FPlasticSourceControlState>& OutStates)
{
	TArray<FString> Results;
	TArray<FString> Parameters;
	Parameters.Add(TEXT("--format=\"{RevisionChangeset};{RevisionHeadChangeset};{LockedBy}\""));

	TArray<FString> ErrorMessages;
	const bool bResult = RunCommand(TEXT("fileinfo"), Parameters, InFiles, Results, ErrorMessages);
	OutErrorMessages.Append(ErrorMessages);
	if (bResult)
	{
		ParseFileinfoResults(InFiles, Results, OutStates);
	}

	return bResult;
}

// Run a Plastic "status" and "fileinfo" commands to update status of given files.
bool RunUpdateStatus(const TArray<FString>& InFiles, TArray<FString>& OutErrorMessages, TArray<FPlasticSourceControlState>& OutStates)
{
	bool bResult;
	
	// Run a "status" command for each file to get workspace states
	bResult = RunStatus(InFiles, OutErrorMessages, OutStates);

	// Run a Plastic "fileinfo" (similar to "status") command to update status of given files.
	bResult &= RunFileinfo(InFiles, OutErrorMessages, OutStates);

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
		if(State->WorkspaceState != InState.WorkspaceState)
		{
			State->WorkspaceState = InState.WorkspaceState;
			State->PendingMergeBaseFileHash = InState.PendingMergeBaseFileHash;
		//	State->TimeStamp = InState.TimeStamp; // TODO Bug report: Workaround a bug with the Source Control Module not updating file state after a "Save"
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
