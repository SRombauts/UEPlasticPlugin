// Copyright (c) 2016-2018 Codice Software - Sebastien Rombauts (sebastien.rombauts@gmail.com)

#include "PlasticSourceControlPrivatePCH.h"
#include "PlasticSourceControlCommand.h"
#include "PlasticSourceControlModule.h"
#include "PlasticSourceControlState.h"
#include "PlasticSourceControlUtils.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#include "Misc/ScopeLock.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformTime.h"
#include "Modules/ModuleManager.h"
#include "XmlParser.h"

#if PLATFORM_LINUX
#include <sys/ioctl.h>
#endif

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h" // SECURITY_ATTRIBUTES
#undef GetUserName
#endif


namespace PlasticSourceControlConstants
{
#if PLATFORM_WINDOWS
	const TCHAR* pchDelim = TEXT("\r\n");
#else
	const TCHAR* pchDelim = TEXT("\n");
#endif
}

FScopedTempFile::FScopedTempFile(const FText& InText)
{
	Filename = FPaths::CreateTempFilename(*FPaths::ProjectLogDir(), TEXT("Plastic-Temp"), TEXT(".txt"));
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
// In/Out Pipes for the 'cm shell' persistent process
static void*			ShellOutputPipeRead = nullptr;
static void*			ShellOutputPipeWrite = nullptr;
static void*			ShellInputPipeRead = nullptr;
static void*			ShellInputPipeWrite = nullptr;
static FProcHandle		ShellProcessHandle;
static FCriticalSection	ShellCriticalSection;
static size_t			ShellCommandCounter = -1;

// Internal function to cleanup (called under the critical section)
static void _CleanupBackgroundCommandLineShell()
{
	FPlatformProcess::ClosePipe(ShellInputPipeRead, ShellInputPipeWrite);
	FPlatformProcess::ClosePipe(ShellOutputPipeRead, ShellOutputPipeWrite);
	ShellOutputPipeRead = ShellOutputPipeWrite = nullptr;
	ShellInputPipeRead = ShellInputPipeWrite = nullptr;
}

// Internal function to actualy launch the Plastic SCM background 'cm shell' process if possible (called under the critical section)
static bool _StartBackgroundPlasticShell(const FString& InPathToPlasticBinary, const FString& InWorkingDirectory)
{
	const FString FullCommand(TEXT("shell"));

	const bool bLaunchDetached = false;				// the new process will NOT have its own window
	const bool bLaunchHidden = true;				// the new process will be minimized in the task bar
	const bool bLaunchReallyHidden = bLaunchHidden; // the new process will not have a window or be in the task bar

	verify(FPlatformProcess::CreatePipe(ShellOutputPipeRead, ShellOutputPipeWrite));	// For reading from child process
	verify(CreatePipeWrite(ShellInputPipeRead, ShellInputPipeWrite));	// For writing to child process

	ShellProcessHandle = FPlatformProcess::CreateProc(*InPathToPlasticBinary, *FullCommand, bLaunchDetached, bLaunchHidden, bLaunchReallyHidden, nullptr, 0, *InWorkingDirectory, ShellOutputPipeWrite, ShellInputPipeRead);
	if (!ShellProcessHandle.IsValid())
	{
		UE_LOG(LogSourceControl, Warning, TEXT("Failed to launch 'cm shell'")); // not a bug, just no Plastic SCM cli found
		_CleanupBackgroundCommandLineShell();
	}
	else
	{
		UE_LOG(LogSourceControl, Log, TEXT("LaunchBackgroundPlasticShell: '%s %s' ok (handle %d)"), *InPathToPlasticBinary, *FullCommand, ShellProcessHandle.Get());
		ShellCommandCounter = 0;
	}

	return ShellProcessHandle.IsValid();
}

// Internal function (called under the critical section)
static void _RestartBackgroundCommandLineShell()
{
	const FPlasticSourceControlModule& PlasticSourceControl = FModuleManager::GetModuleChecked<FPlasticSourceControlModule>("PlasticSourceControl");
	const FString& PathToPlasticBinary = PlasticSourceControl.AccessSettings().GetBinaryPath();
	const FString& WorkingDirectory = PlasticSourceControl.GetProvider().GetPathToWorkspaceRoot();

	FPlatformProcess::CloseProc(ShellProcessHandle);
	_CleanupBackgroundCommandLineShell();
	_StartBackgroundPlasticShell(PathToPlasticBinary, WorkingDirectory);
}

// Internal function (called under the critical section)
static bool _RunCommandInternal(const FString& InCommand, const TArray<FString>& InParameters, const TArray<FString>& InFiles, const EConcurrency::Type InConcurrency, FString& OutResults, FString& OutErrors)
{
	bool bResult = false;

	// Detect previous crash of cm.exe and restart 'cm shell'
	if (!FPlatformProcess::IsProcRunning(ShellProcessHandle))
	{
		UE_LOG(LogSourceControl, Warning, TEXT("RunCommand(%d): 'cm shell' has stopped. Restarting!"), ShellCommandCounter);
		_RestartBackgroundCommandLineShell();
	}

	ShellCommandCounter++;

	// Start with the Plastic command itself ("status", "log", "chekin"...)
	FString FullCommand = InCommand;
	// Append to the command all parameters, and then finally the files
	for (const FString& Parameter : InParameters)
	{
		FullCommand += TEXT(" ");
		FullCommand += Parameter;
	}
	for (const FString& File : InFiles)
	{
		FullCommand += TEXT(" \"");
		FullCommand += File;
		FullCommand += TEXT("\"");
	}
	// @todo: temporary debug logs (whithout the end-of-line)
	const FString LoggableCommand = FullCommand.Left(256); // Limit command log size to 256 characters
	UE_LOG(LogSourceControl, Log, TEXT("RunCommand(%d): '%s' (%d chars, %d files)"), ShellCommandCounter, *LoggableCommand, FullCommand.Len()+1, InFiles.Num());
	FullCommand += TEXT('\n'); // Finalize the command line

	// Send command to 'cm shell' process
	const bool bWriteOk = FPlatformProcess::WritePipe(ShellInputPipeWrite, FullCommand);

	// And wait up to 180.0 seconds for any kind of output from cm shell: in case of lengthier operation, intermediate output (like percentage of progress) is expected, which would refresh the timout
	const double Timeout = 180.0;
	const double StartTimestamp = FPlatformTime::Seconds();
	double LastActivity = StartTimestamp;
	double LastLog = StartTimestamp;
	const double LogInterval = 5.0;
	int32 PreviousLogLen = 0;
	while (FPlatformProcess::IsProcRunning(ShellProcessHandle))
	{
		FString Output = FPlatformProcess::ReadPipe(ShellOutputPipeRead);
		if (0 < Output.Len())
		{
			LastActivity = FPlatformTime::Seconds(); // freshen the timestamp while cm is still actively outputting information
			OutResults.Append(MoveTemp(Output));
			// Search the output for the line containing the result code, also indicating the end of the command
			const uint32 IndexCommandResult = OutResults.Find(TEXT("CommandResult "), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
			if (INDEX_NONE != IndexCommandResult)
			{
				const uint32 IndexEndResult = OutResults.Find(PlasticSourceControlConstants::pchDelim, ESearchCase::CaseSensitive, ESearchDir::FromStart, IndexCommandResult + 14);
				if (INDEX_NONE != IndexEndResult)
				{
					const FString Result = OutResults.Mid(IndexCommandResult + 14, IndexEndResult - IndexCommandResult - 14);
					const int32 ResultCode = FCString::Atoi(*Result);
					bResult = (0 == ResultCode);
					// remove the CommandResult line from the OutResults
					OutResults.RemoveAt(IndexCommandResult, OutResults.Len() - IndexCommandResult);
					break;
				}
			}
		}
		else if ((FPlatformTime::Seconds() - LastLog > LogInterval) && (PreviousLogLen < OutResults.Len()) && (InConcurrency == EConcurrency::Asynchronous))
		{
			// In case of long running operation, start to print intermediate output from cm shell (like percentage of progress)
			// (but only when runing Asynchronous commands, since Synchronous commands block the main thread until they finish)
			UE_LOG(LogSourceControl, Log, TEXT("RunCommand(%d): '%s' in progress for %lfs...\n%s"), ShellCommandCounter, *InCommand, (FPlatformTime::Seconds() - StartTimestamp), *OutResults.Mid(PreviousLogLen));
			PreviousLogLen = OutResults.Len();
			LastLog = FPlatformTime::Seconds(); // freshen the timestamp of last log
		}
		else if (FPlatformTime::Seconds() - LastActivity > Timeout)
		{
			// In case of timeout, ask the blocking 'cm shell' process to exit, and detach from it immediatly: it will be relaunched by next command
			UE_LOG(LogSourceControl, Error, TEXT("RunCommand(%d): '%s' %d TIMEOUT after %lfs output:\n%s"), ShellCommandCounter, *InCommand, bResult, (FPlatformTime::Seconds() - StartTimestamp), *OutResults.Mid(PreviousLogLen));
			FPlatformProcess::WritePipe(ShellInputPipeWrite, TEXT("exit"));
			FPlatformProcess::CloseProc(ShellProcessHandle);
			_CleanupBackgroundCommandLineShell();
		}

		FPlatformProcess::Sleep(0.001f);
	}
	if (!InCommand.Equals(TEXT("exit")))
	{
		if (!FPlatformProcess::IsProcRunning(ShellProcessHandle))
		{
			// 'cm shell' normally only terminates in case of 'exit' command. Will restart on next command.
			UE_LOG(LogSourceControl, Error, TEXT("RunCommand(%d): '%s' 'cm shell' stopped after %lfs output:\n%s"), ShellCommandCounter, *LoggableCommand, (FPlatformTime::Seconds() - StartTimestamp), *OutResults.Left(4096)); // Limit result size to 4096 characters
		}
		else if (!bResult)
		{
			UE_LOG(LogSourceControl, Warning, TEXT("RunCommand(%d): '%s' (in %lfs) output:\n%s"), ShellCommandCounter, *LoggableCommand, (FPlatformTime::Seconds() - StartTimestamp), *OutResults.Left(4096)); // Limit result size to 4096 characters
		}
		else
		{
			if (PreviousLogLen > 0)
			{
				UE_LOG(LogSourceControl, Log, TEXT("RunCommand(%d): '%s' (in %lfs) output:\n%s"), ShellCommandCounter, *LoggableCommand, (FPlatformTime::Seconds() - StartTimestamp), *OutResults.Mid(PreviousLogLen).Left(4096)); // Limit result size to 4096 characters
			}
			else
			{
				UE_LOG(LogSourceControl, Log, TEXT("RunCommand(%d): '%s' (in %lfs) output:\n%s"), ShellCommandCounter, *LoggableCommand, (FPlatformTime::Seconds() - StartTimestamp), *OutResults.Left(4096)); // Limit result size to 4096 characters
			}
		}
	}
	else
	{
		// @todo: debug log
		UE_LOG(LogSourceControl, Log, TEXT("'exit'"));
	}
	// Return output as error if result code is an error
	if (!bResult)
	{
		OutErrors = MoveTemp(OutResults);
	}

	return bResult;
}

// Internal function (called under the critical section)
static void _ExitBackgroundCommandLineShell()
{
	// Tell the 'cm shell' to exit
	FString Results, Errors;
	_RunCommandInternal(TEXT("exit"), TArray<FString>(), TArray<FString>(), EConcurrency::Synchronous, Results, Errors);
	// And wait up to one seconde for its termination
	int timeout = 100;
	while (FPlatformProcess::IsProcRunning(ShellProcessHandle) && (0 < timeout--))
	{
		FPlatformProcess::Sleep(0.01f);
	}
	FPlatformProcess::CloseProc(ShellProcessHandle);
	_CleanupBackgroundCommandLineShell();
}

// Launch the Plastic SCM background 'cm shell' process in background for optimized successive commands (thread-safe)
bool LaunchBackgroundPlasticShell(const FString& InPathToPlasticBinary, const FString& InWorkingDirectory)
{
	// Protect public APIs from multi-thread access
	FScopeLock Lock(&ShellCriticalSection);

	// terminate previous shell if one is already running
	if (ShellProcessHandle.IsValid())
	{
		_ExitBackgroundCommandLineShell();
	}

	return _StartBackgroundPlasticShell(InPathToPlasticBinary, InWorkingDirectory);
}

// Terminate the background 'cm shell' process and associated pipes (thread-safe)
void Terminate()
{
	// Protect public APIs from multi-thread access
	FScopeLock Lock(&ShellCriticalSection);

	if (ShellProcessHandle.IsValid())
	{
		_ExitBackgroundCommandLineShell();
	}
}

// Run command (thread-safe)
static bool RunCommandInternal(const FString& InCommand, const TArray<FString>& InParameters, const TArray<FString>& InFiles, const EConcurrency::Type InConcurrency, FString& OutResults, FString& OutErrors)
{
	bool bResult = false;

	// Protect public APIs from multi-thread access
	FScopeLock Lock(&ShellCriticalSection);

	if (ShellProcessHandle.IsValid())
	{
		bResult = _RunCommandInternal(InCommand, InParameters, InFiles, InConcurrency, OutResults, OutErrors);
	}
	else
	{
		UE_LOG(LogSourceControl, Error, TEXT("RunCommand(%d): '%s': cm shell not running"), ShellCommandCounter, *InCommand);
		OutErrors = InCommand + ": Plastic SCM shell not running!";
	}

	return bResult;
}

// Basic parsing or results & errors from the Plastic command line process
bool RunCommand(const FString& InCommand, const TArray<FString>& InParameters, const TArray<FString>& InFiles, const EConcurrency::Type InConcurrency, TArray<FString>& OutResults, TArray<FString>& OutErrorMessages)
{
	FString Results;
	FString Errors;

	const bool bResult = RunCommandInternal(InCommand, InParameters, InFiles, InConcurrency, Results, Errors);

	Results.ParseIntoArray(OutResults, PlasticSourceControlConstants::pchDelim, true);
	Errors.ParseIntoArray(OutErrorMessages, PlasticSourceControlConstants::pchDelim, true);

	return bResult;
}

FString FindPlasticBinaryPath()
{
#if PLATFORM_WINDOWS
	return FString(TEXT("cm"));
#else
	return FString(TEXT("/usr/bin/cm"));
#endif
}

// Find the root of the Plastic workspace, looking from the provided path and upward in its parent directories.
bool FindRootDirectory(const FString& InPath, FString& OutWorkspaceRoot)
{
	bool bFound = false;
	FString PathToPlasticSubdirectory;
	OutWorkspaceRoot = InPath;

	auto TrimTrailing = [](FString& Str, const TCHAR Char)
	{
		int32 Len = Str.Len();
		while(Len && Str[Len - 1] == Char)
		{
			Str = Str.LeftChop(1);
			Len = Str.Len();
		}
	};

	TrimTrailing(OutWorkspaceRoot, '\\');
	TrimTrailing(OutWorkspaceRoot, '/');

	while(!bFound && !OutWorkspaceRoot.IsEmpty())
	{
		// Look for the ".plastic" subdirectory present at the root of every Plastic workspace
		PathToPlasticSubdirectory = OutWorkspaceRoot / TEXT(".plastic");
		bFound = IFileManager::Get().DirectoryExists(*PathToPlasticSubdirectory);
		if(!bFound)
		{
			int32 LastSlashIndex;
			if(OutWorkspaceRoot.FindLastChar(TEXT('/'), LastSlashIndex))
			{
				OutWorkspaceRoot = OutWorkspaceRoot.Left(LastSlashIndex);
			}
			else
			{
				OutWorkspaceRoot.Empty();
			}
		}
	}
	if (!bFound)
	{
		OutWorkspaceRoot = InPath; // If not found, return the provided dir as best possible root.
	}
	return bFound;
}

void GetPlasticScmVersion(FString& OutPlasticScmVersion)
{
	TArray<FString> InfoMessages;
	TArray<FString> ErrorMessages;
	const bool bResult = RunCommand(TEXT("version"), TArray<FString>(), TArray<FString>(), EConcurrency::Synchronous, InfoMessages, ErrorMessages);
	if (bResult && InfoMessages.Num() > 0)
	{
		OutPlasticScmVersion = InfoMessages[0];
	}
}

void GetUserName(FString& OutUserName)
{
	TArray<FString> InfoMessages;
	TArray<FString> ErrorMessages;
	const bool bResult = RunCommand(TEXT("whoami"), TArray<FString>(), TArray<FString>(), EConcurrency::Synchronous, InfoMessages, ErrorMessages);
	if (bResult && InfoMessages.Num() > 0)
	{
		OutUserName = InfoMessages[0];
	}
}

bool GetWorkspaceName(FString& OutWorkspaceName)
{
	TArray<FString> InfoMessages;
	TArray<FString> ErrorMessages;
	TArray<FString> Parameters;
	Parameters.Add(TEXT("."));
	Parameters.Add(TEXT("--format={0}"));
	// Get the workspace name
	const bool bResult = RunCommand(TEXT("getworkspacefrompath"), Parameters, TArray<FString>(), EConcurrency::Synchronous, InfoMessages, ErrorMessages);
	if (bResult && InfoMessages.Num() > 0)
	{
		// NOTE: getworkspacefrompath never returns an error!
		if (!InfoMessages[0].Equals(TEXT(". is not in a workspace.")))
		{
			OutWorkspaceName = MoveTemp(InfoMessages[0]);
		}
	}

	return bResult;
}

static bool ParseWorkspaceInformation(const TArray<FString>& InInfoMessages, int32& OutChangeset, FString& OutRepositoryName, FString& OutServerUrl, FString& OutBranchName)
{
	bool bResult = true;

	// Get workspace status, in the form "cs:41@rep:UE4PlasticPlugin@repserver:localhost:8087" (disabled by the "--nostatus" flag)
	//                                or "cs:41@rep:UE4PlasticPlugin@repserver:SRombauts@cloud" (when connected directly to the cloud)
	if (InInfoMessages.Num() > 0)
	{
		static const FString ChangesetPrefix(TEXT("cs:"));
		static const FString RepPrefix(TEXT("@rep:"));
		static const FString ServerPrefix(TEXT("@repserver:"));
		const FString& WorkspaceStatus = InInfoMessages[0];
		const int32 RepIndex = WorkspaceStatus.Find(RepPrefix, ESearchCase::CaseSensitive);
		const int32 ServerIndex = WorkspaceStatus.Find(ServerPrefix, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		if ((RepIndex > INDEX_NONE) && (ServerIndex > INDEX_NONE))
		{
			const FString ChangesetString = WorkspaceStatus.Mid(ChangesetPrefix.Len(), RepIndex - ChangesetPrefix.Len());
			OutChangeset = FCString::Atoi(*ChangesetString);
			OutRepositoryName = WorkspaceStatus.Mid(RepIndex + RepPrefix.Len(), ServerIndex - RepIndex - RepPrefix.Len());
			OutServerUrl = WorkspaceStatus.RightChop(ServerIndex + ServerPrefix.Len());
		}
		else
		{
			bResult = false;
		}
	}
	// Get the branch name, in the form "Branch /main@UE4PlasticPluginDev" (enabled by the "--wkconfig" flag)
	if (InInfoMessages.Num() > 1)
	{
		OutBranchName = InInfoMessages[1];
	}

	return bResult;
}

bool GetWorkspaceInformation(int32& OutChangeset, FString& OutRepositoryName, FString& OutServerUrl, FString& OutBranchName)
{
	TArray<FString> InfoMessages;
	TArray<FString> ErrorMessages;
	TArray<FString> Parameters;
	Parameters.Add(TEXT("--wkconfig")); // Branch name
	Parameters.Add(TEXT("--nochanges")); // No file status
	bool bResult = RunCommand(TEXT("status"), Parameters, TArray<FString>(), EConcurrency::Synchronous, InfoMessages, ErrorMessages);
	if (bResult)
	{
		ParseWorkspaceInformation(InfoMessages, OutChangeset, OutRepositoryName, OutServerUrl, OutBranchName);
	}

	return bResult;
}

/**
 * @brief Extract the renamed from filename from a Plastic SCM status result.
 *
 * Examples of status results:
 MV 100% Content\ToMove_BP.uasset -> Content\Moved_BP.uasset
 *
 * @param[in] InResult One line of status
 * @return Renamed from filename extracted from the line of status
 *
 * @see FilenameFromPlasticStatus()
 */
static FString RenamedFromPlasticStatus(const FString& InResult)
{
	FString RenamedFrom;
	int32 RenameIndex;
	if (InResult.FindLastChar('>', RenameIndex))
	{
		// Extract only the first part of a rename "from -> to" (after the 2 letters status surrounded by 2 spaces)
		RenamedFrom = InResult.Mid(9, RenameIndex - 9 - 2);
	}
	return RenamedFrom;
}

/**
 * @brief Extract the relative filename from a Plastic SCM status result.
 *
 * Examples of status results:
 CO Content\CheckedOut_BP.uasset
 MV 100% Content\ToMove_BP.uasset -> Content\Moved_BP.uasset
 *
 * @param[in] InResult One line of status
 * @return Relative filename extracted from the line of status
 *
 * @see FPlasticStatusFileMatcher and StateFromPlasticStatus()
 */
static FString FilenameFromPlasticStatus(const FString& InResult)
{
	FString RelativeFilename;
	int32 RenameIndex;
	if (InResult.FindLastChar('>', RenameIndex))
	{
		// Extract only the second part of a rename "from -> to"
		RelativeFilename = InResult.RightChop(RenameIndex + 2);
	}
	else
	{
		// Extract the relative filename from the Plastic SCM status result (after the 2 letters status surrounded by 2 spaces)
		RelativeFilename = InResult.RightChop(4);
	}

	return RelativeFilename;
}

/**
 * @brief Match the relative filename of a Plastic SCM status result with a provided absolute filename
 *
 * Examples of status results:
 CO Content\CheckedOut_BP.uasset
 MV 100% Content\ToMove_BP.uasset -> Content\Moved_BP.uasset
 */
class FPlasticStatusFileMatcher
{
public:
	FPlasticStatusFileMatcher(const FString& InAbsoluteFilename)
		: AbsoluteFilename(InAbsoluteFilename)
	{
	}

	bool operator()(const FString& InResult) const
	{
		return AbsoluteFilename.Contains(FilenameFromPlasticStatus(InResult));
	}

private:
	const FString& AbsoluteFilename;
};

/**
 * Extract and interpret the file state from the given Plastic "status" result.
 * empty string = unmodified/controlled or hidden changes
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
static EWorkspaceState::Type StateFromPlasticStatus(const FString& InResult)
{
	EWorkspaceState::Type State;
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
	else if ((FileStatus == "PR") || (FileStatus == "LM")) // Not Controlled/Not in Depot/Untracked (or Locally Moved/Renamed)
	{
		State = EWorkspaceState::Private;
	}
	else if (FileStatus == "IG")
	{
		State = EWorkspaceState::Ignored;
	}
	else if (FileStatus == "DE")
	{
		State = EWorkspaceState::Deleted; // Deleted (removed from source control)
	}
	else if (FileStatus == "LD")
	{
		State = EWorkspaceState::LocallyDeleted; // Locally Deleted (ie. missing)
	}
	else if (FileStatus == "MV")
	{
		State = EWorkspaceState::Moved; // Moved/Renamed
	}
	else
	{
		UE_LOG(LogSourceControl, Warning, TEXT("Unknown file status '%s'"), *FileStatus);
		State = EWorkspaceState::Unknown;
	}

	return State;
}

/**
 * @brief Parse the array of strings results of a 'cm status --noheaders --all --ignore' command
 *
 * Called in case of a regular status command for one or multiple files (not for a whole directory). 
 *
 * @param[in]	InFiles		List of files in a directory (never empty).
 * @param[in]	InResults	Lines of results from the "status" command
 * @param[out]	OutStates	States of files for witch the status has been gathered
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
static void ParseFileStatusResult(const TArray<FString>& InFiles, const TArray<FString>& InResults, TArray<FPlasticSourceControlState>& OutStates, int32& OutChangeset, FString& OutBranchName)
{
	const FPlasticSourceControlModule& PlasticSourceControl = FModuleManager::GetModuleChecked<FPlasticSourceControlModule>("PlasticSourceControl");
	const FString& WorkingDirectory = PlasticSourceControl.GetProvider().GetPathToWorkspaceRoot();

	// Parse the first two lines with Changeset number and Branch name
	FString RepositoryName, ServerUrl;
	ParseWorkspaceInformation(InResults, OutChangeset, RepositoryName, ServerUrl, OutBranchName);

	// Iterate on each file explicitly listed in the command
	for (const FString& File : InFiles)
	{
		FPlasticSourceControlState FileState(File);

		// Search the file in the list of status
		// NOTE: in case of rename by editor, there are two results: checked-out AND renamed
		// => we want to get the second one, witch is always the rename, so we search from the end
		int32 IdxResult = InResults.FindLastByPredicate(FPlasticStatusFileMatcher(File));
		if (IdxResult != INDEX_NONE)
		{
			// File found in status results; only the case for "changed" files
			FileState.WorkspaceState = StateFromPlasticStatus(InResults[IdxResult]);

			// Extract the original name of a Moved/Renamed file
			if (EWorkspaceState::Moved == FileState.WorkspaceState)
			{
				FileState.MovedFrom = FPaths::ConvertRelativePathToFull(WorkingDirectory, RenamedFromPlasticStatus(InResults[IdxResult]));
			}
		}
		else
		{
			// File not found in status
			if (FPaths::FileExists(File))
			{
				// usually means the file is unchanged, or is on Hidden changes
				FileState.WorkspaceState = EWorkspaceState::Controlled; // Unchanged
			}
			else
			{
				// but also the case for newly created content: there is no file on disk until the content is saved for the first time
				FileState.WorkspaceState = EWorkspaceState::Private; // Not Controlled
			}
		}
		FileState.TimeStamp.Now();

		// @todo: temporary debug log (only for the first few files)
		if (OutStates.Num() < 20) UE_LOG(LogSourceControl, Log, TEXT("%s = %d:%s"), *File, static_cast<uint32>(FileState.WorkspaceState), FileState.ToString());

		OutStates.Add(MoveTemp(FileState));
	}
	// @todo: temporary debug log (if too many files)
	if (OutStates.Num() > 20) UE_LOG(LogSourceControl, Log, TEXT("[...] %d more files"), OutStates.Num() - 20);
}

/**
 * @brief Parse the array of strings results of a 'cm status --noheaders --all --ignore' command
 *
 * Called in case of a "directory status" (no file listed in the command) ONLY to detect Removed/Deleted files !
 *
 * @param[in]	InResults	Lines of results from the "status" command
 * @param[out]	OutStates	States of files for witch the status has been gathered
 *
 * @see #ParseFileStatusResult() above for an example of a cm status results
*/
static void ParseDirectoryStatusResult(const TArray<FString>& InResults, TArray<FPlasticSourceControlState>& OutStates)
{
	const FPlasticSourceControlModule& PlasticSourceControl = FModuleManager::GetModuleChecked<FPlasticSourceControlModule>("PlasticSourceControl");
	const FString& PathToPlasticBinary = PlasticSourceControl.AccessSettings().GetBinaryPath();
	const FString& WorkingDirectory = PlasticSourceControl.GetProvider().GetPathToWorkspaceRoot();

	// Iterate on each line of result of the status command
	// NOTE: in case of rename by editor, there are two results: checkouted AND renamed
	// => we want to get the second one, witch is always the rename, so we just iterate and the second state will overwrite the first one
	for (const FString& Result : InResults)
	{
		const FString RelativeFilename = FilenameFromPlasticStatus(Result);
		const FString File = FPaths::ConvertRelativePathToFull(WorkingDirectory, RelativeFilename);
		const EWorkspaceState::Type WorkspaceState = StateFromPlasticStatus(Result);
		if ((EWorkspaceState::Deleted == WorkspaceState) || (EWorkspaceState::LocallyDeleted == WorkspaceState))
		{
			FPlasticSourceControlState FileState(File);
			FileState.WorkspaceState = WorkspaceState;
			FileState.TimeStamp.Now();

			// @todo: temporary debug log
			UE_LOG(LogSourceControl, Log, TEXT("%s = %d:%s"), *File, static_cast<uint32>(FileState.WorkspaceState), FileState.ToString());

			OutStates.Add(MoveTemp(FileState));
		}
	}
}

/// Visitor to list all files in subdirectory
class FFileVisitor : public IPlatformFile::FDirectoryVisitor
{
public:
	virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
	{
		if (!bIsDirectory)
		{
			Files.Add(FilenameOrDirectory);
		}
		return true;
	}

	TArray<FString> Files;
};

/**
 * @brief Run a "status" command for a directory to get workspace file states
 *
 *  It is either a command for a whole directory (ie. "Content/", in case of "Submit to Source Control"),
 * or for one or more files all on a same directory (by design, since we group files by directory in RunUpdateStatus())
 *
 * @param[in]	InFiles				List of files in a directory, or the path to the directory itself (never empty).
 * @param[out]	OutErrorMessages	Error messages from the "status" command
 * @param[out]	OutStates			States of files for witch the status has been gathered (distinct than InFiles in case of a "directory status")
 * @param[out]	OutChangeset		The current Changeset Number
 * @param[out]	OutBranchName		Name of the current checked-out branch
 */
static bool RunStatus(const TArray<FString>& InFiles, const EConcurrency::Type InConcurrency, TArray<FString>& OutErrorMessages, TArray<FPlasticSourceControlState>& OutStates, int32& OutChangeset, FString& OutBranchName)
{
	ensure(0 < InFiles.Num());

	TArray<FString> Parameters;
	Parameters.Add(TEXT("--wkconfig"));
	Parameters.Add(TEXT("--noheaders"));
	Parameters.Add(TEXT("--all"));
	Parameters.Add(TEXT("--ignored"));
	// "cm status" only operate on one path (file or folder) at a time, so use one folder path for multiple files in a directory
	const FString Path = FPaths::GetPath(*InFiles[0]);
	TArray<FString> OnePath;
	// Only one file: optim very useful for the .uproject file at the root to avoid parsing the whole repository
	// (does not work if file does not exist anymore)
	if ((1 == InFiles.Num()) && (FPaths::FileExists(InFiles[0])))
	{
		OnePath.Add(InFiles[0]);
	}
	else
	{
		OnePath.Add(Path);
	}
	TArray<FString> Results;
	TArray<FString> ErrorMessages;
	const bool bResult = RunCommand(TEXT("status"), Parameters, OnePath, InConcurrency, Results, ErrorMessages);
	// Normalize paths in the result (convert all '\' to '/')
	for (int32 IdxResult = 0; IdxResult < Results.Num(); IdxResult++)
	{
		FPaths::NormalizeFilename(Results[IdxResult]);
	}
	OutErrorMessages.Append(ErrorMessages);
	if (bResult)
	{
		if (1 == InFiles.Num() && FPaths::DirectoryExists(InFiles[0]))
		{
			// 1) Special case for "status" of a directory: requires a specific parse logic.
			//   (this is triggered by the "Submit to Source Control" top menu button)
			// Find recursively all files in the directory: this enable getting the list of "Controlled" (unchanged) assets
			FFileVisitor FileVisitor;
			FPlatformFileManager::Get().GetPlatformFile().IterateDirectoryRecursively(*Path, FileVisitor);
			// @todo: temporary debug log
			UE_LOG(LogSourceControl, Log, TEXT("RunStatus(%s): 1) special case for status of a directory containing %d file(s) (%s)"), *InFiles[0], FileVisitor.Files.Num(), *Path);
			ParseFileStatusResult(FileVisitor.Files, Results, OutStates, OutChangeset, OutBranchName);
			// The above cannot detect assets removed / locally deleted since there is no file left to enumerate (either by the Content Browser or by File Manager)
			// => so we also parse the status results to explicitly look for Removed/Deleted assets
			Results.RemoveAt(0, 2); // Before that, remove the first two line Changeset, and BranchName
			ParseDirectoryStatusResult(Results, OutStates);
		}
		else
		{
			// 2) General case for one or more files in the same directory.
			// @todo: temporary debug log
			UE_LOG(LogSourceControl, Log, TEXT("RunStatus(%s...): 2) general case for %d file(s) in a directory (%s)"), *InFiles[0], InFiles.Num(), *Path);
			ParseFileStatusResult(InFiles, Results, OutStates, OutChangeset, OutBranchName);
		}
	}

	return bResult;
}

// Parse the fileinfo output format "{RevisionChangeset};{RevisionHeadChangeset};{RepSpec};{LockedBy};{LockedWhere}"
// for example "40;41;repo@server:port;srombauts;UE4PlasticPluginDev"
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
				RepSpec = MoveTemp(Fileinfos[2]);
				if (NbElmts >=4)
				{
					LockedBy = MoveTemp(Fileinfos[3]);
					if (NbElmts >= 5)
					{
						LockedWhere = MoveTemp(Fileinfos[4]);
					}
				}
			}
		}
	}

	int32 RevisionChangeset;
	int32 RevisionHeadChangeset;
	FString RepSpec;
	FString LockedBy;
	FString LockedWhere;
};

/** Parse the array of strings result of a 'cm fileinfo --format="{RevisionChangeset};{RevisionHeadChangeset};{RepSpec};{LockedBy};{LockedWhere}"' command
 *
 * Example cm fileinfo results:
16;16;;
14;15;;
17;17;srombauts;Workspace_2
 */
static void ParseFileinfoResults(const TArray<FString>& InResults, TArray<FPlasticSourceControlState>& InOutStates)
{
	const FPlasticSourceControlModule& PlasticSourceControl = FModuleManager::GetModuleChecked<FPlasticSourceControlModule>("PlasticSourceControl");
	const FPlasticSourceControlProvider& Provider = PlasticSourceControl.GetProvider();

	// Iterate on all files and all status of the result (assuming same number of line of results than number of file states)
	for (int32 IdxResult = 0; IdxResult < InResults.Num(); IdxResult++)
	{
		const FString& Fileinfo = InResults[IdxResult];
		FPlasticSourceControlState& FileState = InOutStates[IdxResult];
		const FString& File = FileState.GetFilename();
		FPlasticFileinfoParser FileinfoParser(Fileinfo);

		FileState.LocalRevisionChangeset = FileinfoParser.RevisionChangeset;
		FileState.DepotRevisionChangeset = FileinfoParser.RevisionHeadChangeset;
		FileState.RepSpec = FileinfoParser.RepSpec;
		FileState.LockedBy = MoveTemp(FileinfoParser.LockedBy);
		FileState.LockedWhere = MoveTemp(FileinfoParser.LockedWhere);

		// If a file is locked by someone but not checked-out locally (or moved/renamed) this means it is locked by someone else or elsewhere
		if ((FileState.WorkspaceState != EWorkspaceState::CheckedOut) && (FileState.WorkspaceState != EWorkspaceState::Moved) && (0 < FileState.LockedBy.Len())) 
		{
			// @todo: temporary debug log
			UE_LOG(LogSourceControl, Warning, TEXT("LockedByOther(%s) by '%s!=%s' (or %s!=%s)"), *File, *FileState.LockedBy, *Provider.GetUserName(), *FileState.LockedWhere, *Provider.GetWorkspaceName());
			FileState.WorkspaceState = EWorkspaceState::LockedByOther;
		}

		// @todo: temporary debug log (only for the first few files)
		if (IdxResult < 20) UE_LOG(LogSourceControl, Log, TEXT("%s: %d;%d %s by '%s' (%s)"), *File, FileState.LocalRevisionChangeset, FileState.DepotRevisionChangeset, *FileState.RepSpec, *FileState.LockedBy, *FileState.LockedWhere);
	}
	// @todo: temporary debug log (if too many files)
	if (InResults.Num() > 20) UE_LOG(LogSourceControl, Log, TEXT("[...] %d more files"), InResults.Num() - 20);
}

/**
 * @brief Run a Plastic "fileinfo" command to update status of given files.
 *
 * @param[in]		InForceFileinfo		Also force execute the fileinfo command required to do get RepSpec of xlinks when getting history (or for diffs)
 * @param[in]		InConcurrency		Is the command running in the background, or blocking the main thread
 * @param[out]		OutErrorMessages	Error messages from the "fileinfo" command
 * @param[in,out]	InOutStates			List of file states in the directory, gathered by the "status" command, completed by results of the "fileinfo" command
 */
static bool RunFileinfo(const bool InForceFileinfo, const EConcurrency::Type InConcurrency, TArray<FString>& OutErrorMessages, TArray<FPlasticSourceControlState>& InOutStates)
{
	bool bResult = true;
	TArray<FString> Files;
	for (const auto& State : InOutStates)
	{
		// Optimize by not issuing "fileinfo" commands on "Added"/"Deleted"/"NotControled"/"Ignored" but also "CheckedOut" and "Moved" files.
		// This can greatly reduce the time needed to do some basic operation like "Add to source control" when using a distant server or the Plastic Cloud.
		// this can't work with xlink file when we want to update the history
		// we need to know that we are running a fileinfo command to get the history, that's the role of InForceFileinfo
		if (	(InForceFileinfo)
			||	(State.WorkspaceState == EWorkspaceState::Controlled)
			||	(State.WorkspaceState == EWorkspaceState::Changed)
			||	(State.WorkspaceState == EWorkspaceState::Replaced)
			||	(State.WorkspaceState == EWorkspaceState::Conflicted)
		//	||	(State.WorkspaceState == EWorkspaceState::LockedByOther) // we do not have this info at this stage, cf. ParseFileinfoResults()
			)
		{
			Files.Add(State.GetFilename());
		}
	}
	if (Files.Num() > 0)
	{
		TArray<FString> Results;
		TArray<FString> ErrorMessages;
		TArray<FString> Parameters;
		Parameters.Add(TEXT("--format=\"{RevisionChangeset};{RevisionHeadChangeset};{RepSpec};{LockedBy};{LockedWhere}\""));
		bResult = RunCommand(TEXT("fileinfo"), Parameters, Files, InConcurrency, Results, ErrorMessages);
		OutErrorMessages.Append(ErrorMessages);
		if (bResult)
		{
			ParseFileinfoResults(Results, InOutStates);
		}
	}

	return bResult;
}

// FILE_CONFLICT /Content/FirstPersonBP/Blueprints/FirstPersonProjectile.uasset 1 4 6 903
// (explanations: 'The file /Content/FirstPersonBP/Blueprints/FirstPersonProjectile.uasset needs to be merged from cs:4 to cs:6 base cs:1. Changed by both contributors.')
class FPlasticMergeConflictParser
{
public:
	FPlasticMergeConflictParser(const FString& InResult)
	{
		static const FString FILE_CONFLICT(TEXT("FILE_CONFLICT "));
		if (InResult.StartsWith(FILE_CONFLICT, ESearchCase::CaseSensitive))
		{
			FString Temp = InResult.RightChop(FILE_CONFLICT.Len());
			int32 WhitespaceIndex;
			if (Temp.FindChar(TEXT(' '), WhitespaceIndex))
			{
				Filename = Temp.Left(WhitespaceIndex);
			}
			Temp = Temp.RightChop(WhitespaceIndex + 1);
			if (Temp.FindChar(TEXT(' '), WhitespaceIndex))
			{
				const FString Base = Temp.Left(WhitespaceIndex);
				BaseChangeset = FCString::Atoi(*Base);
			}
			Temp = Temp.RightChop(WhitespaceIndex + 1);
			if (Temp.FindChar(TEXT(' '), WhitespaceIndex))
			{
				const FString Source = Temp.Left(WhitespaceIndex);
				SourceChangeset = FCString::Atoi(*Source);
			}
		}
	}

	FString Filename;
	int32 BaseChangeset;
	int32 SourceChangeset;
};

// Check if merging, and from which changelist, then execute a cm merge command to amend status for listed files
bool RunCheckMergeStatus(const TArray<FString>& InFiles, TArray<FString>& OutErrorMessages, TArray<FPlasticSourceControlState>& OutStates)
{
	bool bResult = false;
	FPlasticSourceControlModule& PlasticSourceControl = FModuleManager::GetModuleChecked<FPlasticSourceControlModule>("PlasticSourceControl");
	FPlasticSourceControlProvider& Provider = PlasticSourceControl.GetProvider();

	const FString MergeProgressFilename = FPaths::Combine(*Provider.GetPathToWorkspaceRoot(), TEXT(".plastic/plastic.mergeprogress"));
	if (FPaths::FileExists(MergeProgressFilename))
	{
		// read in file as string
		FString MergeProgressContent;
		if (FFileHelper::LoadFileToString(MergeProgressContent, *MergeProgressFilename))
		{
			// @todo: temporary debug logs
			UE_LOG(LogSourceControl, Log, TEXT("RunCheckMergeStatus: %s:\n%s"), *MergeProgressFilename, *MergeProgressContent);
			// Content is in one line, looking like the following:
			// Target: mount:56e62dd7-241f-41e9-8c6b-dd4ca4513e62#/#UE4MergeTest@localhost:8087 merged from: Merge 4
			// Target: mount:56e62dd7-241f-41e9-8c6b-dd4ca4513e62#/#UE4MergeTest@localhost:8087 merged from: Cherrypicking 3
			// Target: mount:56e62dd7-241f-41e9-8c6b-dd4ca4513e62#/#UE4MergeTest@localhost:8087 merged from: IntervalCherrypick 2 4
			// 1) Extract the word after "merged from: "
			static const FString MergeFromString(TEXT("merged from: "));
			const int32 MergeFromIndex = MergeProgressContent.Find(MergeFromString, ESearchCase::CaseSensitive);
			if (MergeFromIndex > INDEX_NONE)
			{
				const FString MergeType = MergeProgressContent.RightChop(MergeFromIndex + MergeFromString.Len());
				int32 SpaceBeforeChangesetIndex;
				if (MergeType.FindChar(TEXT(' '), SpaceBeforeChangesetIndex))
				{
					// 2) In case of "Merge" or "Cherrypicking" extract the merge changelist xxx after the last space (use case for merge from "branch", from "label", and for "merge on Update")
					const FString ChangesetString = MergeType.RightChop(SpaceBeforeChangesetIndex + 1);
					const int32 Changeset = FCString::Atoi(*ChangesetString);
					const FString ChangesetSpecification = FString::Printf(TEXT("cs:%d"), Changeset);

					TArray<FString> Results;
					TArray<FString> ErrorMessages;
					TArray<FString> Parameters;
					Parameters.Add(ChangesetSpecification);

					int32 SpaceBeforeChangeset2Index;
					if (ChangesetString.FindLastChar(TEXT(' '), SpaceBeforeChangeset2Index))
					{
						// 3) In case of "IntervalCherrypick", extract the 2 changelists
						const FString Changeset2String = ChangesetString.RightChop(SpaceBeforeChangeset2Index + 1);
						const int32 Changeset2 = FCString::Atoi(*Changeset2String);
						const FString Changeset2Specification = FString::Printf(TEXT("--interval-origin=cs:%d"), Changeset2);

						Parameters.Add(Changeset2Specification);
					}
					else
					{
						if (MergeType.StartsWith(TEXT("Cherrypicking"), ESearchCase::CaseSensitive))
						{
							Parameters.Add(TEXT("--cherrypicking"));
						}
					}
					// Store the Merge Parameters for reuse with later "Resolve" operation
					const TArray<FString> PendingMergeParameters = Parameters;
					Parameters.Add(TEXT("--machinereadable"));
					// call 'cm merge cs:xxx --machinereadable' (only dry-run, whithout the --merge parameter)
					bResult = RunCommand(TEXT("merge"), Parameters, TArray<FString>(), EConcurrency::Synchronous, Results, ErrorMessages);
					OutErrorMessages.Append(ErrorMessages);
					// Parse the result, one line for each conflicted files:
					for (const FString& Result : Results)
					{
						FPlasticMergeConflictParser MergeConflict(Result);
						UE_LOG(LogSourceControl, Log, TEXT("MergeConflict.Filename: '%s'"), *MergeConflict.Filename);
						for (FPlasticSourceControlState& State : OutStates)
						{
							UE_LOG(LogSourceControl, Log, TEXT("State.LocalFilename: '%s'"), *State.LocalFilename);
							if (State.LocalFilename.EndsWith(MergeConflict.Filename, ESearchCase::CaseSensitive))
							{
								// @todo: temporary debug log
								UE_LOG(LogSourceControl, Log, TEXT("MergeConflict '%s' found Base cs:%d From cs:%d"), *MergeConflict.Filename, MergeConflict.BaseChangeset, MergeConflict.SourceChangeset);
								State.WorkspaceState = EWorkspaceState::Conflicted;
								State.PendingMergeFilename = MergeConflict.Filename;
								State.PendingMergeBaseChangeset = MergeConflict.BaseChangeset;
								State.PendingMergeSourceChangeset = MergeConflict.SourceChangeset;
								State.PendingMergeParameters = PendingMergeParameters;
								break;
							}
						}
					}
				}
			}
		}
	}

	return bResult;
}

// Run a batch of Plastic "status" and "fileinfo" commands to update status of given files and directories.
bool RunUpdateStatus(const TArray<FString>& InFiles, const bool InForceFileinfo, const EConcurrency::Type InConcurrency, TArray<FString>& OutErrorMessages, TArray<FPlasticSourceControlState>& OutStates, int32& OutChangeset, FString& OutBranchName)
{
	bool bResults = true;

	// The "status" command only operate on one directory at a time
	// (whole tree recursively) not on different folders with no common root.
	// But "Submit to Source Control" ask for the State of 3 different directories, Engine/Content, Project/Content and Project/Config,
	// In a same way, a check-in can involve files from different subdirectories, and UpdateStatus is called for them all at once.

	// 1) So here we group files by path (ie. by subdirectory)
	TMap<FString, TArray<FString>> GroupOfFiles;
	for (const FString& File : InFiles)
	{
		const FString Path = FPaths::GetPath(*File);
		TArray<FString>* Group = GroupOfFiles.Find(Path);
		if (Group != nullptr)
		{
			Group->Add(File);
		}
		else
		{
			TArray<FString> NewGroup;
			NewGroup.Add(File);
			GroupOfFiles.Add(Path, NewGroup);
		}
	}

	// @todo: temporary debug log
	if (InFiles.Num() > 0)
	{
		UE_LOG(LogSourceControl, Log, TEXT("RunUpdateStatus: %d file(s)/%d directory(ies) ('%s'...)"), InFiles.Num(), GroupOfFiles.Num(), *InFiles[0]);
	}
	else
	{
		UE_LOG(LogSourceControl, Warning, TEXT("RunUpdateStatus: NO file"));
	}

	// 2) then we can batch Plastic status operation by subdirectory
	for (const auto& Files : GroupOfFiles)
	{
		// Run a "status" command on the directory to get workspace file states.
		TArray<FPlasticSourceControlState> States;
		const bool bGroupOk = RunStatus(Files.Value, InConcurrency, OutErrorMessages, States, OutChangeset, OutBranchName);
		if (bGroupOk && (States.Num() > 0))
		{
			// Run a "fileinfo" command to update status of given files.
			// In case of "directory status", there is no explicit file in the group (it contains only the directory) 
			// => work on the list of files discovered by RunStatus()
			bResults &= RunFileinfo(InForceFileinfo, InConcurrency, OutErrorMessages, States);
		}
		OutStates.Append(MoveTemp(States));
	}

	// Check if merging, and from which changelist, then execute a cm merge command to amend status for listed files
	RunCheckMergeStatus(InFiles, OutErrorMessages, OutStates);

	return bResults;
}

// Run a Plastic "cat" command to dump the binary content of a revision into a file.
// cm cat revid:1230@rep:myrep@repserver:myserver:8084 --raw --file=Name124.tmp
bool RunDumpToFile(const FString& InPathToPlasticBinary, const FString& InRevSpec, const FString& InDumpFileName)
{
	int32	ReturnCode = 0;
	FString Results;
	FString Errors;

	// start with the Plastic command itself, then add revspec and temp filename to dump
	FString FullCommand = TEXT("cat ");
	FullCommand += InRevSpec;
	FullCommand += TEXT(" --raw --file=\"");
	FullCommand += InDumpFileName;
	FullCommand += TEXT("\"");

	// @todo: temporary debug logs
	UE_LOG(LogSourceControl, Log, TEXT("RunDumpToFile: '%s %s'"), *InPathToPlasticBinary, *FullCommand);
	const bool bResult = FPlatformProcess::ExecProcess(*InPathToPlasticBinary, *FullCommand, &ReturnCode, &Results, &Errors);
	UE_LOG(LogSourceControl, Log, TEXT("RunDumpToFile: ExecProcess ReturnCode=%d Results='%s'"), ReturnCode, *Results);
	if (!bResult || !Errors.IsEmpty())
	{
		UE_LOG(LogSourceControl, Error, TEXT("RunDumpToFile: ExecProcess ReturnCode=%d Errors='%s'"), ReturnCode, *Errors);
	}

	return bResult;
}


/**
 * Translate file actions from Plastic 'cm log' command to keywords used by the Editor UI.
 *
 * @see SHistoryRevisionListRowContent::GenerateWidgetForColumn(): "add", "edit", "delete", "branch" and "integrate"
*/
FString TranslateAction(const FString& InAction)
{
	if (InAction.Equals(TEXT("Added")))
	{
		return TEXT("add");
	}
	else if (InAction.Equals(TEXT("Moved")))
	{
		return TEXT("branch");
	}
	else if (InAction.Equals(TEXT("Deleted")))
	{
		return TEXT("delete");
	}
	else // if (InAction.Equals(TEXT("Changed")))
	{
		return TEXT("edit");
	}
}

/**
 * Parse the array of strings results of a 'cm log --xml' command
 *
 * Example cm log results:
<?xml version="1.0" encoding="utf-8"?>
<LogList>
  <Changeset>
    <ObjId>989</ObjId>
    <ChangesetId>2</ChangesetId>
    <Branch>/main</Branch>
    <Comment>Ignore Collections and Developers content</Comment>
    <Owner>dev</Owner>
    <GUID>a985c487-0f54-45c5-b0ef-9b87c4c3c3f9</GUID>
    <Changes>
      <Item>
        <Branch>/main</Branch>
        <RevNo>2</RevNo>
        <Owner>dev</Owner>
        <RevId>985</RevId>
        <ParentRevId>282</ParentRevId>
        <SrcCmPath>/ignore.conf</SrcCmPath>
        <SrcParentItemId>2</SrcParentItemId>
        <DstCmPath>/ignore.conf</DstCmPath>
        <DstParentItemId>2</DstParentItemId>
        <Date>2016-04-18T10:44:49.0000000+02:00</Date>
        <Type>Changed</Type>
      </Item>
    </Changes>
    <Date>2016-04-18T10:44:49.0000000+02:00</Date>
  </Changeset>
</LogList>
*/
static void ParseLogResults(const FXmlFile& InXmlResult, FPlasticSourceControlRevision& OutSourceControlRevision)
{
	static const FString LogList(TEXT("LogList"));
	static const FString Changeset(TEXT("Changeset"));
	static const FString Comment(TEXT("Comment"));
	static const FString Date(TEXT("Date"));
	static const FString Owner(TEXT("Owner"));
	static const FString Changes(TEXT("Changes"));
	static const FString Item(TEXT("Item"));
	static const FString RevId(TEXT("RevId"));
	static const FString ParentRevId(TEXT("ParentRevId"));
	static const FString SrcCmPath(TEXT("SrcCmPath"));
	static const FString DstCmPath(TEXT("DstCmPath"));
	static const FString Type(TEXT("Type"));

	const FXmlNode* LogListNode = InXmlResult.GetRootNode();
	if (LogListNode == nullptr || LogListNode->GetTag() != LogList)
	{
		return;
	}

	const FXmlNode* ChangesetNode = LogListNode->FindChildNode(Changeset);
	if (ChangesetNode == nullptr)
	{
		return;
	}

	const FXmlNode* CommentNode = ChangesetNode->FindChildNode(Comment);
	if (CommentNode != nullptr)
	{
		OutSourceControlRevision.Description = CommentNode->GetContent();
	}
	const FXmlNode* OwnerNode = ChangesetNode->FindChildNode(Owner);
	if (OwnerNode != nullptr)
	{
		OutSourceControlRevision.UserName = OwnerNode->GetContent();
	}
	const FXmlNode* DateNode = ChangesetNode->FindChildNode(Date);
	if (DateNode != nullptr)
	{	//                           |--|
		//    2016-04-18T10:44:49.0000000+02:00
		// => 2016-04-18T10:44:49.000+02:00
		const FString DateIso = DateNode->GetContent().LeftChop(10) + DateNode->GetContent().RightChop(27);
		FDateTime::ParseIso8601(*DateIso, OutSourceControlRevision.Date);
	}

	const FXmlNode* ChangesNode = ChangesetNode->FindChildNode(Changes);
	if (ChangesNode == nullptr)
	{
		return;
	}

	// Iterate on files to find the one we are tracking
	for (const FXmlNode* ItemNode : ChangesNode->GetChildrenNodes())
	{
		int32 RevisionId = -1;
		const FXmlNode* RevIdNode = ItemNode->FindChildNode(RevId);
		if (RevIdNode != nullptr)
		{
			RevisionId = FCString::Atoi(*RevIdNode->GetContent());
		}
		// Is this about the file we are looking for?
		if (RevisionId == OutSourceControlRevision.RevisionId)
		{
			const FXmlNode* DstCmPathNode = ItemNode->FindChildNode(DstCmPath);
			if (DstCmPathNode != nullptr)
			{
				OutSourceControlRevision.Filename = DstCmPathNode->GetContent();

				const FXmlNode* SrcCmPathNode = ItemNode->FindChildNode(SrcCmPath);
				const FXmlNode* ParentRevIdNode = ItemNode->FindChildNode(ParentRevId);
				// Detect case of rename ("branch" in Perforce vocabulary)
				if (ParentRevIdNode != nullptr && SrcCmPathNode != nullptr && !SrcCmPathNode->GetContent().Equals(DstCmPathNode->GetContent()))
				{
					TSharedRef<FPlasticSourceControlRevision, ESPMode::ThreadSafe> MovedFromRevision = MakeShareable(new FPlasticSourceControlRevision);
					MovedFromRevision->Filename = SrcCmPathNode->GetContent();
					MovedFromRevision->RevisionId = FCString::Atoi(*ParentRevIdNode->GetContent());
	
					OutSourceControlRevision.BranchSource = MovedFromRevision;
				}
			}
			const FXmlNode* TypeNode = ItemNode->FindChildNode(Type);
			if (TypeNode != nullptr)
			{
				OutSourceControlRevision.Action = TranslateAction(TypeNode->GetContent());
			}
			// 	Do not stop at first match, because in case of rename there are multiple log nodes: Changed+Moved (in this order)
		}
	}
}

// Run "cm log" on the changeset provided by the "history" command to get extra info about the change at a specific revision
static bool RunLogCommand(const FString& InChangeset, const FString& InRepSpec, FPlasticSourceControlRevision& OutSourceControlRevision)
{
	const FString RepositorySpecification = FString::Printf(TEXT("cs:%s@%s"), *InChangeset, *InRepSpec);

	FString Results;
	FString Errors;
	TArray<FString> Parameters;
	Parameters.Add(RepositorySpecification);
	Parameters.Add(TEXT("--xml"));
	Parameters.Add(TEXT("--encoding=\"utf-8\""));

	// Uses the raw RunCommandInternal() that does not split results in an array of strings, for XML parsing
	bool bResult = RunCommandInternal(TEXT("log"), Parameters, TArray<FString>(), EConcurrency::Synchronous, Results, Errors);
	if (bResult)
	{
		FXmlFile XmlFile;
		bResult = XmlFile.LoadFile(Results, EConstructMethod::ConstructFromBuffer);
		if (bResult)
		{
			ParseLogResults(XmlFile, OutSourceControlRevision);
		}
	}
	return bResult;
}

/**
 * Parse results of the 'cm history --format="{1};{6}"' command ("Changeset number" and "Revision id"),
 * then run "cm log" on each revision to get extra info about the change (description, date, filename, branch, action)
 * 
 * Results of the history command are with one changeset number and revision id by line, like that:
14;176
17;220
18;223
*/
static bool ParseHistoryResults(const TArray<FString>& InResults, FPlasticSourceControlState& InOutState)
{
	bool bResult = true;

	const FPlasticSourceControlModule& PlasticSourceControl = FModuleManager::GetModuleChecked<FPlasticSourceControlModule>("PlasticSourceControl");
	const FPlasticSourceControlProvider& Provider = PlasticSourceControl.GetProvider();
	const FString RootRepSpec = FString::Printf(TEXT("%s@%s"), *Provider.GetRepositoryName(), *Provider.GetServerUrl());

	InOutState.History.Reserve(InResults.Num());

	// parse history in reverse: needed to get most recent at the top (implied by the UI)
	for (int32 Index = InResults.Num() - 1; Index >= 0; Index--)
	{
		const FString& Result = InResults[Index];
		if (bResult)
		{
			TArray<FString> Infos;
			const int32 NbElmts = Result.ParseIntoArray(Infos, TEXT(";"));
			if (NbElmts == 2)
			{
				const TSharedRef<FPlasticSourceControlRevision, ESPMode::ThreadSafe> SourceControlRevision = MakeShareable(new FPlasticSourceControlRevision);
				SourceControlRevision->State = &InOutState;
				const FString& Changeset = Infos[0];
				const FString& RevisionId = Infos[1];
				SourceControlRevision->ChangesetNumber = FCString::Atoi(*Changeset); // Value now used in the Revision column and in the Asset Menu History
				SourceControlRevision->RevisionId = FCString::Atoi(*RevisionId); // 
				// Also append depot name to the revision, but only when it is different from the default one (ie for xlinks sub repository)
				if (InOutState.RepSpec != RootRepSpec)
				{
					TArray<FString> RepSpecs;
					InOutState.RepSpec.ParseIntoArray(RepSpecs, TEXT("@"));
					SourceControlRevision->Revision = FString::Printf(TEXT("cs:%s@%s"), *Changeset, *RepSpecs[0]);
				}
				else
				{
					SourceControlRevision->Revision = FString::Printf(TEXT("cs:%s"), *Changeset);
				}

				// Run "cm log" on the changeset number
				bResult = RunLogCommand(Changeset, InOutState.RepSpec, *SourceControlRevision);
				InOutState.History.Add(SourceControlRevision);
			}
			else
			{
				bResult = false;
			}
		}
	}

	return bResult;
}

// Run a Plastic "history" command and multiple "log" commands and parse them.
bool RunGetHistory(const FString& InFile, TArray<FString>& OutErrorMessages, FPlasticSourceControlState& InOutState)
{
	TArray<FString> Results;
	TArray<FString> Parameters;
	Parameters.Add(TEXT("--format=\"{1};{6}\"")); // Get "Changeset number" and "Revision id" of each revision of the asset
	TArray<FString> OneFile;
	OneFile.Add(*InFile);

	bool bResult = RunCommand(TEXT("history"), Parameters, OneFile, EConcurrency::Synchronous, Results, OutErrorMessages);
	if (bResult)
	{
		bResult = ParseHistoryResults(Results, InOutState);
	}

	return bResult;
}

bool UpdateCachedStates(TArray<FPlasticSourceControlState>&& InStates)
{
	FPlasticSourceControlModule& PlasticSourceControl = FModuleManager::GetModuleChecked<FPlasticSourceControlModule>( "PlasticSourceControl" );
	FPlasticSourceControlProvider& Provider = PlasticSourceControl.GetProvider();
	const FDateTime Now = FDateTime::Now();

	for (auto&& InState : InStates)
	{
		TSharedRef<FPlasticSourceControlState, ESPMode::ThreadSafe> State = Provider.GetStateInternal(InState.LocalFilename);
		*State = MoveTemp(InState);
		State->TimeStamp = Now;
	}

	return (InStates.Num() > 0);
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
	for(const FString& ErrorMessage : InCommand.ErrorMessages)
	{
		if(ErrorMessage.Contains(InFilter, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
		{
			InCommand.InfoMessages.Add(ErrorMessage);
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
