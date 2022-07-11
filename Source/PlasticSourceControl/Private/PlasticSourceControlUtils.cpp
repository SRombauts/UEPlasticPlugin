// Copyright (c) 2016-2022 Codice Software

#include "PlasticSourceControlUtils.h"

#include "PlasticSourceControlCommand.h"
#include "PlasticSourceControlModule.h"
#include "PlasticSourceControlProjectSettings.h"
#include "PlasticSourceControlProvider.h"
#include "PlasticSourceControlSettings.h"
#include "PlasticSourceControlState.h"
#include "Runtime/Launch/Resources/Version.h"
#if ENGINE_MAJOR_VERSION == 4
#include "HAL/PlatformFilemanager.h"
#elif ENGINE_MAJOR_VERSION == 5
#include "HAL/PlatformFileManager.h"
#endif
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#include "Misc/ScopeLock.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformTime.h"
#include "Modules/ModuleManager.h"
#include "XmlParser.h"
#include "ISourceControlModule.h"
#include "SoftwareVersion.h"

#if ENGINE_MAJOR_VERSION == 5
#include "PlasticSourceControlChangelist.h"
#include "PlasticSourceControlChangelistState.h"
#endif

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

FScopedTempFile::FScopedTempFile(const FString& InText)
{
	Filename = FPaths::CreateTempFilename(*FPaths::ProjectLogDir(), TEXT("Plastic-Temp"), TEXT(".txt"));
	if (!FFileHelper::SaveStringToFile(InText, *Filename, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogSourceControl, Error, TEXT("Failed to write to temp file: %s"), *Filename);
	}
}

FScopedTempFile::FScopedTempFile(const FText& InText)
{
	Filename = FPaths::CreateTempFilename(*FPaths::ProjectLogDir(), TEXT("Plastic-Temp"), TEXT(".txt"));
	if (!FFileHelper::SaveStringToFile(InText.ToString(), *Filename, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogSourceControl, Error, TEXT("Failed to write to temp file: %s"), *Filename);
	}
}

FScopedTempFile::~FScopedTempFile()
{
	if (FPaths::FileExists(Filename))
	{
		if (!FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*Filename))
		{
			UE_LOG(LogSourceControl, Error, TEXT("Failed to delete temp file: %s"), *Filename);
		}
	}
}

const FString& FScopedTempFile::GetFilename() const
{
	return Filename;
}

#if ENGINE_MAJOR_VERSION == 4

// Needed to SetHandleInformation() on WritePipe for input (opposite of ReadPipe, for output) (idem FInteractiveProcess)
// Note: this has been implemented in Unreal Engine 5.0 in January 2022
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

#endif

namespace PlasticSourceControlUtils
{
// In/Out Pipes for the 'cm shell' persistent child process
static void*			ShellOutputPipeRead = nullptr;
static void*			ShellOutputPipeWrite = nullptr;
static void*			ShellInputPipeRead = nullptr;
static void*			ShellInputPipeWrite = nullptr;
static FProcHandle		ShellProcessHandle;
static FCriticalSection	ShellCriticalSection;
static size_t			ShellCommandCounter = -1;
static double			ShellCumulatedTime = 0.;

// Internal function to cleanup (called under the critical section)
static void _CleanupBackgroundCommandLineShell()
{
	FPlatformProcess::ClosePipe(ShellOutputPipeRead, ShellOutputPipeWrite);
	FPlatformProcess::ClosePipe(ShellInputPipeRead, ShellInputPipeWrite);
	ShellOutputPipeRead = ShellOutputPipeWrite = nullptr;
	ShellInputPipeRead = ShellInputPipeWrite = nullptr;
}

// Internal function to launch the Plastic SCM background 'cm' process in interactive shell mode (called under the critical section)
static bool _StartBackgroundPlasticShell(const FString& InPathToPlasticBinary, const FString& InWorkingDirectory)
{
	const FString FullCommand(TEXT("shell --encoding=UTF-8"));

	const bool bLaunchDetached = false;				// the new process will NOT have its own window
	const bool bLaunchHidden = true;				// the new process will be minimized in the task bar
	const bool bLaunchReallyHidden = bLaunchHidden; // the new process will not have a window or be in the task bar

	const double StartTimestamp = FPlatformTime::Seconds();

#if ENGINE_MAJOR_VERSION == 4
	verify(FPlatformProcess::CreatePipe(ShellOutputPipeRead, ShellOutputPipeWrite));		// For reading outputs from cm shell child process
	verify(             CreatePipeWrite(ShellInputPipeRead, ShellInputPipeWrite));			// For writing commands to cm shell child process NOLINT
#elif ENGINE_MAJOR_VERSION == 5
	verify(FPlatformProcess::CreatePipe(ShellOutputPipeRead, ShellOutputPipeWrite, false));	// For reading outputs from cm shell child process
	verify(FPlatformProcess::CreatePipe(ShellInputPipeRead, ShellInputPipeWrite, true));	// For writing commands to cm shell child process
#endif

	ShellProcessHandle = FPlatformProcess::CreateProc(*InPathToPlasticBinary, *FullCommand, bLaunchDetached, bLaunchHidden, bLaunchReallyHidden, nullptr, 0, *InWorkingDirectory, ShellOutputPipeWrite, ShellInputPipeRead);
	if (!ShellProcessHandle.IsValid())
	{
		UE_LOG(LogSourceControl, Warning, TEXT("Failed to launch 'cm shell'")); // not a bug, just no Plastic SCM cli found
		_CleanupBackgroundCommandLineShell();
	}
	else
	{
		const double ElapsedTime = (FPlatformTime::Seconds() - StartTimestamp);
		UE_LOG(LogSourceControl, Verbose, TEXT("_StartBackgroundPlasticShell: '%s %s' ok (in %.3lfs, handle %d)"), *InPathToPlasticBinary, *FullCommand, ElapsedTime, ShellProcessHandle.Get());
		ShellCommandCounter = 0;
		ShellCumulatedTime = ElapsedTime;
	}

	return ShellProcessHandle.IsValid();
}

// Internal function (called under the critical section)
static void _ExitBackgroundCommandLineShell()
{
	if (ShellProcessHandle.IsValid())
	{
		if (FPlatformProcess::IsProcRunning(ShellProcessHandle))
		{
			// Tell the 'cm shell' to exit
			FPlatformProcess::WritePipe(ShellInputPipeWrite, TEXT("exit"));
			// And wait up to one second for its termination
			const double Timeout = 1.0;
			const double StartTimestamp = FPlatformTime::Seconds();
			while (FPlatformProcess::IsProcRunning(ShellProcessHandle))
			{
				if ((FPlatformTime::Seconds() - StartTimestamp) > Timeout)
				{
					UE_LOG(LogSourceControl, Warning, TEXT("ExitBackgroundCommandLineShell: cm shell didn't stop gracefully in %lfs."), Timeout);
					FPlatformProcess::TerminateProc(ShellProcessHandle);
					break;
				}
				FPlatformProcess::Sleep(0.01f);
			}
		}
		FPlatformProcess::CloseProc(ShellProcessHandle);
		_CleanupBackgroundCommandLineShell();
	}
}

// Internal function (called under the critical section)
static void _RestartBackgroundCommandLineShell()
{
	const FPlasticSourceControlProvider& Provider = FPlasticSourceControlModule::Get().GetProvider();
	const FString& PathToPlasticBinary = Provider.AccessSettings().GetBinaryPath();
	const FString& WorkingDirectory = Provider.GetPathToWorkspaceRoot();

	_ExitBackgroundCommandLineShell();
	_StartBackgroundPlasticShell(PathToPlasticBinary, WorkingDirectory);
}

// Internal function (called under the critical section)
static bool _RunCommandInternal(const FString& InCommand, const TArray<FString>& InParameters, const TArray<FString>& InFiles, const EConcurrency::Type InConcurrency, FString& OutResults, FString& OutErrors)
{
	bool bResult = false;

	ShellCommandCounter++;

	// Detect previous crash of cm.exe and restart 'cm shell'
	if (!FPlatformProcess::IsProcRunning(ShellProcessHandle))
	{
		UE_LOG(LogSourceControl, Warning, TEXT("RunCommand: 'cm shell' has stopped. Restarting!"));
		_RestartBackgroundCommandLineShell();
	}

	// Start with the Plastic command itself ("status", "log", "checkin"...)
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
	const FString LoggableCommand = FullCommand.Left(256); // Limit command log size to 256 characters
	UE_LOG(LogSourceControl, Verbose, TEXT("RunCommand: '%s' (%d chars, %d files)"), *LoggableCommand, FullCommand.Len()+1, InFiles.Num());
	FullCommand += TEXT('\n'); // Finalize the command line

	// Send command to 'cm shell' process in UTF-8
	// NOTE: this explicit conversion to UTF-8 shouldn't be needed since FPlatformProcess::WritePipe() says it does it, but reading the implementation for Windows Platform show it merily truncates 16bits to 8bits chars!
	// NOTE: on the other hand, ReadPipe() does the conversion from UTF-8 correctly already!
	const FTCHARToUTF8 FullCommandUtf8(*FullCommand);
	const bool bWriteOk = FPlatformProcess::WritePipe(ShellInputPipeWrite, reinterpret_cast<const uint8*>(FullCommandUtf8.Get()), FullCommandUtf8.Length());

	// And wait up to 180.0 seconds for any kind of output from cm shell: in case of lengthier operation, intermediate output (like percentage of progress) is expected, which would refresh the timeout
	static const double Timeout = 180.0;
	const double StartTimestamp = FPlatformTime::Seconds();
	double LastActivity = StartTimestamp;
	double LastLog = StartTimestamp;
	static const double LogInterval = 5.0;
	int32 PreviousLogLen = 0;
	while (FPlatformProcess::IsProcRunning(ShellProcessHandle))
	{
		FString Output = FPlatformProcess::ReadPipe(ShellOutputPipeRead);
		if (!Output.IsEmpty())
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
					bResult = (ResultCode == 0);
					// remove the CommandResult line from the OutResults
					OutResults.RemoveAt(IndexCommandResult, OutResults.Len() - IndexCommandResult);
					break;
				}
			}
		}
		else if ((FPlatformTime::Seconds() - LastLog > LogInterval) && (PreviousLogLen < OutResults.Len()))
		{
			// In case of long running operation, start to print intermediate output from cm shell (like percentage of progress)
			UE_LOG(LogSourceControl, Log, TEXT("RunCommand: '%s' in progress for %.3lfs...\n%s"), *InCommand, (FPlatformTime::Seconds() - StartTimestamp), *OutResults.Mid(PreviousLogLen));
			PreviousLogLen = OutResults.Len();
			LastLog = FPlatformTime::Seconds(); // freshen the timestamp of last log
		}
		else if (FPlatformTime::Seconds() - LastActivity > Timeout)
		{
			// In case of timeout, ask the blocking 'cm shell' process to exit, detach from it and restart it immediately
			UE_LOG(LogSourceControl, Error, TEXT("RunCommand: '%s' TIMEOUT after %.3lfs output (%d chars):\n%s"), *InCommand, (FPlatformTime::Seconds() - StartTimestamp), OutResults.Len(), *OutResults.Mid(PreviousLogLen));
			_RestartBackgroundCommandLineShell();
			return false;
		}
		else if (IsEngineExitRequested())
		{
			UE_LOG(LogSourceControl, Warning, TEXT("RunCommand: '%s' Engine Exit was requested after %.3lfs output (%d chars):\n%s"), *InCommand, (FPlatformTime::Seconds() - StartTimestamp), OutResults.Len(), *OutResults.Mid(PreviousLogLen));
			_ExitBackgroundCommandLineShell();
		}

		FPlatformProcess::Sleep(0.001f);
	}
	const double ElapsedTime = (FPlatformTime::Seconds() - StartTimestamp);

	if (!InCommand.Equals(TEXT("exit")))
	{
		if (!FPlatformProcess::IsProcRunning(ShellProcessHandle))
		{
			// 'cm shell' normally only terminates in case of 'exit' command. Will restart on next command.
			UE_LOG(LogSourceControl, Error, TEXT("RunCommand: '%s' 'cm shell' stopped after %.3lfs output (%d chars):\n%s"), *LoggableCommand, ElapsedTime, OutResults.Len(), *OutResults.Left(4096)); // Limit result size to 4096 characters
		}
		else if (!bResult)
		{
			UE_LOG(LogSourceControl, Warning, TEXT("RunCommand: '%s' (in %.3lfs) output (%d chars):\n%s"), *LoggableCommand, ElapsedTime, OutResults.Len(), *OutResults.Left(4096)); // Limit result size to 4096 characters
		}
		else
		{
			if (PreviousLogLen > 0)
			{
				UE_LOG(LogSourceControl, Log, TEXT("RunCommand: '%s' (in %.3lfs) output (%d chars):\n%s"), *LoggableCommand, ElapsedTime, OutResults.Len(), *OutResults.Mid(PreviousLogLen).Left(4096)); // Limit result size to 4096 characters
			}
			else
			{
				if (OutResults.Len() <= 200) // Limit result size to 200 characters
				{
					UE_LOG(LogSourceControl, Log, TEXT("RunCommand: '%s' (in %.3lfs) output (%d chars):\n%s"), *LoggableCommand, ElapsedTime, OutResults.Len(), *OutResults);
				}
				else
				{
					UE_LOG(LogSourceControl, Log, TEXT("RunCommand: '%s' (in %.3lfs) (output %d chars not displayed)"), *LoggableCommand, ElapsedTime, OutResults.Len());
					UE_LOG(LogSourceControl, Verbose, TEXT("\n%s"), *OutResults.Left(4096));; // Limit result size to 4096 characters
				}
			}
		}
	}
	// Return output as error if result code is an error
	if (!bResult)
	{
		OutErrors = MoveTemp(OutResults);
	}

	ShellCumulatedTime += ElapsedTime;
	UE_LOG(LogSourceControl, Verbose, TEXT("RunCommand: cumulated time spent in shell: %.3lfs (count %d)"), ShellCumulatedTime, ShellCommandCounter);

	return bResult;
}

// Launch the Plastic SCM background 'cm shell' process in background for optimized successive commands (thread-safe)
bool LaunchBackgroundPlasticShell(const FString& InPathToPlasticBinary, const FString& InWorkingDirectory)
{
	// Protect public APIs from multi-thread access
	FScopeLock Lock(&ShellCriticalSection);

	// terminate previous shell if one is already running
	_ExitBackgroundCommandLineShell();

	return _StartBackgroundPlasticShell(InPathToPlasticBinary, InWorkingDirectory);
}

// Terminate the background 'cm shell' process and associated pipes (thread-safe)
void Terminate()
{
	// Protect public APIs from multi-thread access
	FScopeLock Lock(&ShellCriticalSection);

	_ExitBackgroundCommandLineShell();
}

// Run command (thread-safe)
bool RunCommandInternal(const FString& InCommand, const TArray<FString>& InParameters, const TArray<FString>& InFiles, const EConcurrency::Type InConcurrency, FString& OutResults, FString& OutErrors)
{
	// Protect public APIs from multi-thread access
	FScopeLock Lock(&ShellCriticalSection);

	return _RunCommandInternal(InCommand, InParameters, InFiles, InConcurrency, OutResults, OutErrors);
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

const FSoftwareVersion& GetOldestSupportedPlasticScmVersion()
{
	// https://www.plasticscm.com/download/releasenotes/9.0.16.4839 cm changelist 'persistent' flag now contain a '--' prefix.
	static FSoftwareVersion OldestSupportedPlasticScmVersion(TEXT("9.0.16.4839"));
	return OldestSupportedPlasticScmVersion;
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
		while (Len && Str[Len - 1] == Char)
		{
			Str = Str.LeftChop(1);
			Len = Str.Len();
		}
	};

	TrimTrailing(OutWorkspaceRoot, '\\');
	TrimTrailing(OutWorkspaceRoot, '/');

	while (!bFound && !OutWorkspaceRoot.IsEmpty())
	{
		// Look for the ".plastic" subdirectory present at the root of every Plastic workspace
		PathToPlasticSubdirectory = OutWorkspaceRoot / TEXT(".plastic");
		bFound = FPaths::DirectoryExists(*PathToPlasticSubdirectory);
		if (!bFound)
		{
			int32 LastSlashIndex;
			if (OutWorkspaceRoot.FindLastChar(TEXT('/'), LastSlashIndex))
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

// This is called once by FPlasticSourceControlProvider::CheckPlasticAvailability()
bool GetPlasticScmVersion(FSoftwareVersion& OutPlasticScmVersion)
{
	TArray<FString> InfoMessages;
	TArray<FString> ErrorMessages;
	const bool bResult = RunCommand(TEXT("version"), TArray<FString>(), TArray<FString>(), EConcurrency::Synchronous, InfoMessages, ErrorMessages);
	if (bResult && InfoMessages.Num() > 0)
	{
		OutPlasticScmVersion = FSoftwareVersion(InfoMessages[0]);
		return true;
	}
	return false;
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

bool GetWorkspaceName(const FString& InWorkspaceRoot, FString& OutWorkspaceName, TArray<FString>& OutErrorMessages)
{
	TArray<FString> InfoMessages;

	TArray<FString> Parameters;
	Parameters.Add(TEXT("--format={0}"));
	TArray<FString> Files;
	Files.Add(InWorkspaceRoot);
	// Get the workspace name
	const bool bResult = RunCommand(TEXT("getworkspacefrompath"), Parameters, Files, EConcurrency::Synchronous, InfoMessages, OutErrorMessages);
	if (bResult && InfoMessages.Num() > 0)
	{
		// NOTE: an old version of cm getworkspacefrompath didn't return an error code so we had to rely on the error message
		if (!InfoMessages[0].EndsWith(TEXT(" is not in a workspace.")))
		{
			OutWorkspaceName = MoveTemp(InfoMessages[0]);
		}
	}

	return bResult;
}

static bool ParseWorkspaceInformation(const TArray<FString>& InInfoMessages, int32& OutChangeset, FString& OutRepositoryName, FString& OutServerUrl, FString& OutBranchName)
{
	bool bResult = true;

	// Get workspace status, in the form "cs:41@rep:UEPlasticPlugin@repserver:localhost:8087" (disabled by the "--nostatus" flag)
	//                                or "cs:41@rep:UEPlasticPlugin@repserver:SRombauts@cloud" (when connected directly to the cloud)
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
	// Get the branch name, in the form "Branch /main@UEPlasticPluginDev" (enabled by the "--wkconfig" flag)
	if (InInfoMessages.Num() > 1)
	{
		static const FString BranchPrefix(TEXT("Branch "));
		const FString& BranchInfo = InInfoMessages[1];
		const int32 BranchIndex = BranchInfo.Find(BranchPrefix, ESearchCase::CaseSensitive);
		if (BranchIndex > INDEX_NONE)
		{
			OutBranchName = BranchInfo;
		}
	}

	return bResult;
}

bool GetWorkspaceInformation(int32& OutChangeset, FString& OutRepositoryName, FString& OutServerUrl, FString& OutBranchName, TArray<FString>& OutErrorMessages)
{
	TArray<FString> InfoMessages;
	TArray<FString> Parameters;
	Parameters.Add(TEXT("--compact"));
	Parameters.Add(TEXT("--header")); // Only prints the workspace status. No file status.
	// NOTE: --wkconfig results in two network calls GetBranchInfoByName & GetLastChangesetOnBranch so it's okay to do it once here but not all the time
	Parameters.Add(TEXT("--wkconfig")); // Branch name. NOTE: Deprecated in 8.0.16.3000 https://www.plasticscm.com/download/releasenotes/8.0.16.3000
	bool bResult = RunCommand(TEXT("status"), Parameters, TArray<FString>(), EConcurrency::Synchronous, InfoMessages, OutErrorMessages);
	if (bResult)
	{
		bResult = ParseWorkspaceInformation(InfoMessages, OutChangeset, OutRepositoryName, OutServerUrl, OutBranchName);
	}

	return bResult;
}

FString UserNameToDisplayName(const FString& InUserName)
{
	if (const FString* Result = GetDefault<UPlasticSourceControlProjectSettings>()->UserNameToDisplayName.Find(InUserName))
	{
		return *Result;
	}
	else if (GetDefault<UPlasticSourceControlProjectSettings>()->bHideEmailDomainInUsername)
	{
		int32 EmailDomainSeparatorIndex;
		if (InUserName.FindChar(TEXT('@'), EmailDomainSeparatorIndex))
		{
			return InUserName.Left(EmailDomainSeparatorIndex);
		}
	}

	return InUserName;
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
	explicit FPlasticStatusFileMatcher(const FString& InAbsoluteFilename)
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
EWorkspaceState::Type StateFromPlasticStatus(const FString& InResult)
{
	EWorkspaceState::Type State;
	const FString FileStatus = (InResult[0] == TEXT(' ')) ? InResult.Mid(1, 2) : InResult;

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
		UE_LOG(LogSourceControl, Warning, TEXT("Unknown file status '%s' (in line '%s')"), *FileStatus, *InResult);
		State = EWorkspaceState::Unknown;
	}

	return State;
}

/**
 * @brief Parse the array of strings results of a 'cm status --noheaders --all --ignored' command
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
static void ParseFileStatusResult(TArray<FString>&& InFiles, const TArray<FString>& InResults, TArray<FPlasticSourceControlState>& OutStates, int32& OutChangeset, FString& OutBranchName)
{
	const FString& WorkingDirectory = FPlasticSourceControlModule::Get().GetProvider().GetPathToWorkspaceRoot();

	// Parse the first two lines with Changeset number and Branch name (the second being requested only once at init)
	FString RepositoryName, ServerUrl;
	ParseWorkspaceInformation(InResults, OutChangeset, RepositoryName, ServerUrl, OutBranchName);

	// Iterate on each file explicitly listed in the command
	for (FString& InFile : InFiles)
	{
		FPlasticSourceControlState FileState(MoveTemp(InFile));
		const FString& File = FileState.LocalFilename;

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

		// debug log (only for the first few files)
		if (OutStates.Num() < 20)
		{
			UE_LOG(LogSourceControl, Verbose, TEXT("%s = %d:%s"), *File, static_cast<uint32>(FileState.WorkspaceState), FileState.ToString());
		}

		OutStates.Add(MoveTemp(FileState));
	}
	// debug log (if too many files)
	if (OutStates.Num() > 20)
	{
		UE_LOG(LogSourceControl, Verbose, TEXT("[...] %d more files"), OutStates.Num() - 20);
	}
}

/**
 * @brief Detect Deleted files in case of a "whole directory status" (no file listed in the command)
 * 
 * Parse the array of strings results of a 'cm status --noheaders --all --ignored' command
 *
 * @param[in]	InResults	Lines of results from the "status" command
 * @param[out]	OutStates	States of files for witch the status has been gathered
 *
 * @see #ParseFileStatusResult() above for an example of a cm status results
*/
static void ParseDirectoryStatusResultForDeleted(const TArray<FString>& InResults, TArray<FPlasticSourceControlState>& OutStates)
{
	const FString& WorkingDirectory = FPlasticSourceControlModule::Get().GetProvider().GetPathToWorkspaceRoot();

	// Iterate on each line of result of the status command
	for (const FString& Result : InResults)
	{
		const EWorkspaceState::Type WorkspaceState = StateFromPlasticStatus(Result);
		if ((EWorkspaceState::Deleted == WorkspaceState) || (EWorkspaceState::LocallyDeleted == WorkspaceState))
		{
			FString RelativeFilename = FilenameFromPlasticStatus(Result);
			FString AbsoluteFilename = FPaths::ConvertRelativePathToFull(WorkingDirectory, MoveTemp(RelativeFilename));
			FPlasticSourceControlState FileState(MoveTemp(AbsoluteFilename));
			FileState.WorkspaceState = WorkspaceState;
			FileState.TimeStamp.Now();

			UE_LOG(LogSourceControl, Verbose, TEXT("%s = %d:%s"), *FileState.LocalFilename, static_cast<uint32>(FileState.WorkspaceState), FileState.ToString());

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
 *  ie. Changed, CheckedOut, Copied, Replaced, Added, Private, Ignored, Deleted, LocallyDeleted, Moved, LocallyMoved
 *
 *  It is either a command for a whole directory (ie. "Content/", in case of "Submit to Source Control"),
 * or for one or more files all on a same directory (by design, since we group files by directory in RunUpdateStatus())
 *
 * @param[in]	InDir				The path to the common directory of all the files listed after.
 * @param[in]	InFiles				List of files in a directory, or the path to the directory itself (never empty).
 * @param[out]	OutErrorMessages	Error messages from the "status" command
 * @param[out]	OutStates			States of files for witch the status has been gathered (distinct than InFiles in case of a "directory status")
 * @param[out]	OutChangeset		The current Changeset Number
 * @param[out]	OutBranchName		Name of the current checked-out branch
 */
static bool RunStatus(const FString& InDir, TArray<FString>&& InFiles, const EConcurrency::Type InConcurrency, TArray<FString>& OutErrorMessages, TArray<FPlasticSourceControlState>& OutStates, int32& OutChangeset, FString& OutBranchName)
{
	check(InFiles.Num() > 0);

	TArray<FString> Parameters;
	Parameters.Add(TEXT("--compact"));
	Parameters.Add(TEXT("--noheaders"));
	Parameters.Add(TEXT("--all"));
	Parameters.Add(TEXT("--ignored"));
	// "cm status" only operate on one path (file or directory) at a time, so use one common path for multiple files in a directory
	TArray<FString> OnePath;
	// Only one file: optim very useful for the .uproject file at the root to avoid parsing the whole repository
	// (but doesn't work if the file is deleted)
	const bool bSingleFile = (InFiles.Num() == 1) && (FPaths::FileExists(InFiles[0]));
	if (bSingleFile)
	{
		OnePath.Add(InFiles[0]);
	}
	else
	{
		OnePath.Add(InDir);
	}
	TArray<FString> Results;
	TArray<FString> ErrorMessages;
	const bool bResult = RunCommand(TEXT("status"), Parameters, OnePath, InConcurrency, Results, ErrorMessages);
	OutErrorMessages.Append(MoveTemp(ErrorMessages));
	if (bResult)
	{
		// Normalize paths in the result (convert all '\' to '/')
		for (FString& Result : Results)
		{
			FPaths::NormalizeFilename(Result);
		}

		const bool bWholeDirectory = (InFiles.Num() == 1) && (InFiles[0] == InDir);
		if (bWholeDirectory)
		{
			// 1) Special case for "status" of a directory: requires a specific parse logic.
			//   (this is triggered by the "Submit to Source Control" top menu button)
			// Find recursively all files in the directory: this enable getting the list of "Controlled" (unchanged) assets
			FFileVisitor FileVisitor;
			FPlatformFileManager::Get().GetPlatformFile().IterateDirectoryRecursively(*InDir, FileVisitor);
			UE_LOG(LogSourceControl, Verbose, TEXT("RunStatus(%s): 1) special case for status of a directory containing %d file(s)"), *InDir, FileVisitor.Files.Num());
			ParseFileStatusResult(MoveTemp(FileVisitor.Files), Results, OutStates, OutChangeset, OutBranchName);
			// The above cannot detect assets removed / locally deleted since there is no file left to enumerate (either by the Content Browser or by File Manager)
			// => so we also parse the status results to explicitly look for Removed/Deleted assets
			if (Results.Num() > 0)
			{
				Results.RemoveAt(0, 1); // Before that, remove the first line (Workspace/Changeset info)
			}
			ParseDirectoryStatusResultForDeleted(Results, OutStates);
		}
		else
		{
			// 2) General case for one or more files in the same directory.
			UE_LOG(LogSourceControl, Verbose, TEXT("RunStatus(%s...): 2) general case for %d file(s) in a directory (%s)"), *InFiles[0], InFiles.Num(), *InDir);
			ParseFileStatusResult(MoveTemp(InFiles), Results, OutStates, OutChangeset, OutBranchName);
		}
	}

	return bResult;
}

// Parse the fileinfo output format "{RevisionChangeset};{RevisionHeadChangeset};{RepSpec};{LockedBy};{LockedWhere}"
// for example "40;41;repo@server:port;srombauts;UEPlasticPluginDev"
class FPlasticFileinfoParser
{
public:
	explicit FPlasticFileinfoParser(const FString& InResult)
	{
		TArray<FString> Fileinfos;
		const int32 NbElmts = InResult.ParseIntoArray(Fileinfos, TEXT(";"), false); // Don't cull empty values in csv
		if (NbElmts == 5)
		{
			RevisionChangeset = FCString::Atoi(*Fileinfos[0]);
			RevisionHeadChangeset = FCString::Atoi(*Fileinfos[1]);
			RepSpec = MoveTemp(Fileinfos[2]);
			LockedBy = UserNameToDisplayName(MoveTemp(Fileinfos[3]));
			LockedWhere = MoveTemp(Fileinfos[4]);
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
	const FPlasticSourceControlProvider& Provider = FPlasticSourceControlModule::Get().GetProvider();

	ensureMsgf(InResults.Num() == InOutStates.Num(), TEXT("The fileinfo command should gives the same number of infos as the status command"));

	// Iterate on all files and all status of the result (assuming same number of line of results than number of file states)
	for (int32 IdxResult = 0; IdxResult < InResults.Num(); IdxResult++)
	{
		const FString& Fileinfo = InResults[IdxResult];
		FPlasticSourceControlState& FileState = InOutStates[IdxResult];
		const FString& File = FileState.LocalFilename;
		FPlasticFileinfoParser FileinfoParser(Fileinfo);

		FileState.LocalRevisionChangeset = FileinfoParser.RevisionChangeset;
		FileState.DepotRevisionChangeset = FileinfoParser.RevisionHeadChangeset;
		FileState.RepSpec = FileinfoParser.RepSpec;
		FileState.LockedBy = MoveTemp(FileinfoParser.LockedBy);
		FileState.LockedWhere = MoveTemp(FileinfoParser.LockedWhere);

		// If a file is locked but not checked-out locally (or moved/renamed) this means it is locked by someone else or elsewhere
		if ((FileState.WorkspaceState != EWorkspaceState::CheckedOut) && (FileState.WorkspaceState != EWorkspaceState::Moved) && !FileState.LockedBy.IsEmpty())
		{
			UE_LOG(LogSourceControl, Verbose, TEXT("LockedByOther(%s) by '%s!=%s' (or %s!=%s)"), *File, *FileState.LockedBy, *Provider.GetUserName(), *FileState.LockedWhere, *Provider.GetWorkspaceName());
			FileState.WorkspaceState = EWorkspaceState::LockedByOther;
		}

		// debug log (only for the first few files)
		if (IdxResult < 20)
		{
			UE_LOG(LogSourceControl, Verbose, TEXT("%s: %d;%d %s by '%s' (%s)"), *File, FileState.LocalRevisionChangeset, FileState.DepotRevisionChangeset, *FileState.RepSpec, *FileState.LockedBy, *FileState.LockedWhere);
		}
	}
	// debug log (if too many files)
	if (InResults.Num() > 20)
	{
		UE_LOG(LogSourceControl, Verbose, TEXT("[...] %d more files"), InResults.Num() - 20);
	}
}

/**
 * @brief Run a "fileinfo" command to update complementary status information of given files.
 *
 * ie RevisionChangeset, RevisionHeadChangeset, RepSpec, LockedBy, LockedWhere
 *
 * @param[in]		bInWholeDirectory	If executed on a whole directory (typically Content/) for a "Submit Content" operation, optimize fileinfo more aggressively
 * @param			bInUpdateHistory	If getting the history of files, force execute the fileinfo command required to get RepSpec of XLinks (history view or visual diff)
 * @param[in]		InConcurrency		Is the command running in the background, or blocking the main thread
 * @param[out]		OutErrorMessages	Error messages from the "fileinfo" command
 * @param[in,out]	InOutStates			List of file states in the directory, gathered by the "status" command, completed by results of the "fileinfo" command
 */
static bool RunFileinfo(const bool bInWholeDirectory, const bool bInUpdateHistory, const EConcurrency::Type InConcurrency, TArray<FString>& OutErrorMessages, TArray<FPlasticSourceControlState>& InOutStates)
{
	bool bResult = true;
	TArray<FString> SelectedFiles;

	TArray<FPlasticSourceControlState> SelectedStates;
	TArray<FPlasticSourceControlState> OptimizedStates;
	for (FPlasticSourceControlState& State : InOutStates)
	{
		// 1) Issue a "fileinfo" command for controlled files (to know if they are up to date and can be checked-out or checked-in)
		// but only if controlled unchanged, or locally changed / locally deleted,
		// optimizing for files that are CheckedOut/Added/Deleted/Moved/Copied/Replaced/NotControled/Ignored/Private/Unknown
		// (since there is no point to check if they are up to date in these cases; they are already checked-out or not controlled).
		// This greatly reduce the time needed to do some operations like "Add" or "Move/Rename/Copy" when there is some latency with the server (eg cloud).
		//
		// 2) bInWholeDirectory: In the case of a "whole directory status" triggered by the "Submit Content" operation,
		// don't even issue a "fileinfo" command for unchanged Controlled files since they won't be considered them for submit.
		// This greatly reduce the time needed to open the Submit window.
		//
		// 3) bInUpdateHistory: When the plugin needs to update the history of files, it needs to know if it's on a XLink,
		// so the fileinfo command is required here to get the RepSpec
		if (bInUpdateHistory
			|| ((State.WorkspaceState == EWorkspaceState::Controlled) && !bInWholeDirectory)
			||	(State.WorkspaceState == EWorkspaceState::Changed)
			||	(State.WorkspaceState == EWorkspaceState::LocallyDeleted)
			)
		{
			SelectedFiles.Add(State.LocalFilename);
			SelectedStates.Add(MoveTemp(State));
		}
		else
		{
			OptimizedStates.Add(MoveTemp(State));
		}
	}
	InOutStates = MoveTemp(OptimizedStates);

	if (SelectedStates.Num())
	{
		TArray<FString> Results;
		TArray<FString> ErrorMessages;
		TArray<FString> Parameters;
		Parameters.Add(TEXT("--format=\"{RevisionChangeset};{RevisionHeadChangeset};{RepSpec};{LockedBy};{LockedWhere}\""));
		bResult = RunCommand(TEXT("fileinfo"), Parameters, SelectedFiles, InConcurrency, Results, ErrorMessages);
		OutErrorMessages.Append(MoveTemp(ErrorMessages));
		if (bResult)
		{
			ParseFileinfoResults(Results, SelectedStates);
			InOutStates.Append(MoveTemp(SelectedStates));
		}
	}

	return bResult;
}

// FILE_CONFLICT /Content/FirstPersonBP/Blueprints/FirstPersonProjectile.uasset 1 4 6 903
// (explanations: 'The file /Content/FirstPersonBP/Blueprints/FirstPersonProjectile.uasset needs to be merged from cs:4 to cs:6 base cs:1. Changed by both contributors.')
class FPlasticMergeConflictParser
{
public:
	explicit FPlasticMergeConflictParser(const FString& InResult)
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
static bool RunCheckMergeStatus(const TArray<FString>& InFiles, TArray<FString>& OutErrorMessages, TArray<FPlasticSourceControlState>& OutStates)
{
	bool bResult = false;
	const FPlasticSourceControlProvider& Provider = FPlasticSourceControlModule::Get().GetProvider();

	const FString MergeProgressFilename = FPaths::Combine(*Provider.GetPathToWorkspaceRoot(), TEXT(".plastic/plastic.mergeprogress"));
	if (FPaths::FileExists(MergeProgressFilename))
	{
		// read in file as string
		FString MergeProgressContent;
		if (FFileHelper::LoadFileToString(MergeProgressContent, *MergeProgressFilename))
		{
			UE_LOG(LogSourceControl, Verbose, TEXT("RunCheckMergeStatus: %s:\n%s"), *MergeProgressFilename, *MergeProgressContent);
			// Content is in one line, looking like the following:
			// Target: mount:56e62dd7-241f-41e9-8c6b-dd4ca4513e62#/#UEMergeTest@localhost:8087 merged from: Merge 4
			// Target: mount:56e62dd7-241f-41e9-8c6b-dd4ca4513e62#/#UEMergeTest@localhost:8087 merged from: Cherrypicking 3
			// Target: mount:56e62dd7-241f-41e9-8c6b-dd4ca4513e62#/#UEMergeTest@localhost:8087 merged from: IntervalCherrypick 2 4
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
					// call 'cm merge cs:xxx --machinereadable' (only dry-run, without the --merge parameter)
					bResult = RunCommand(TEXT("merge"), Parameters, TArray<FString>(), EConcurrency::Synchronous, Results, ErrorMessages);
					OutErrorMessages.Append(MoveTemp(ErrorMessages));
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
								UE_LOG(LogSourceControl, Verbose, TEXT("MergeConflict '%s' found Base cs:%d From cs:%d"), *MergeConflict.Filename, MergeConflict.BaseChangeset, MergeConflict.SourceChangeset);
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

FString FindCommonDirectory(const FString& InPath1, const FString& InPath2)
{
	const int32 MinLen = FMath::Min(InPath1.Len(), InPath2.Len());
	int32 IndexAfterLastCommonSeparator = 0;
	for (int32 Index = 0; Index < MinLen; Index++)
	{
		if (InPath1[Index] != InPath2[Index])
		{
			break;
		}
		if (InPath1[Index] == TEXT('/'))
		{
			IndexAfterLastCommonSeparator = Index + 1;
		}
	}
	return InPath1.Left(IndexAfterLastCommonSeparator);
}

// Structure to group all files belonging to a root dir, storing their best/longest common directory
struct FFilesInCommonDir
{
	// Best/longest common directory, slash terminated, based on FindCommonDirectory()
	FString			CommonDir;
	TArray<FString>	Files;
};

// Run a batch of Plastic "status" and "fileinfo" commands to update status of given files and directories.
bool RunUpdateStatus(const TArray<FString>& InFiles, const bool bInUpdateHistory, const EConcurrency::Type InConcurrency, TArray<FString>& OutErrorMessages, TArray<FPlasticSourceControlState>& OutStates, int32& OutChangeset, FString& OutBranchName)
{
	bool bResults = true;

	const FString& WorkspaceRoot = FPlasticSourceControlModule::Get().GetProvider().GetPathToWorkspaceRoot();

	// The "status" command only operate on one directory-tree at a time (whole tree recursively)
	// not on different folders with no common root.
	// But "Submit to Source Control" ask for the State of many different directories,
	// from Project/Content and Project/Config, Engine/Content, Engine/Plugins/<...>/Content...

	// In a similar way, a checkin can involve files from different subdirectories, and UpdateStatus is called for all of them at once.

	static TArray<FString> RootDirs =
	{
		FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir()),
		FPaths::ConvertRelativePathToFull(FPaths::ProjectConfigDir()),
		FPaths::ConvertRelativePathToFull(FPaths::ProjectPluginsDir()),
		FPaths::ConvertRelativePathToFull(FPaths::GameSourceDir()),
		FPaths::ConvertRelativePathToFull(FPaths::EngineContentDir())
	};

	// 1) So here we group files by path (ie. by subdirectory)
	TMap<FString, FFilesInCommonDir> GroupOfFiles;
	for (const FString& File : InFiles)
	{
		// Discard all file/paths that are not under the workspace root (typically excluding the Engine content)
		if (!File.StartsWith(WorkspaceRoot))
		{
			UE_LOG(LogSourceControl, Verbose, TEXT("%s is out of the Workspace"), *File);
			continue;
		}

		bool bDirFound = false;
		for (const auto& RootDir : RootDirs)
		{
			if (File.StartsWith(RootDir))
			{
				FFilesInCommonDir* ExistingGroup = GroupOfFiles.Find(RootDir);
				if (ExistingGroup != nullptr)
				{
					// See if we have to update the CommonDir
					if (!File.StartsWith(ExistingGroup->CommonDir))
					{
						// the file is not in the same path, we need to find their common dir
						ExistingGroup->CommonDir = FindCommonDirectory(ExistingGroup->CommonDir, File);
					}
					ExistingGroup->Files.Add(File);
				}
				else
				{
					FString Path = FPaths::GetPath(File) + TEXT("/");
					GroupOfFiles.Add(RootDir, { MoveTemp(Path), {File}});
				}

				bDirFound = true;
				break;
			}
		}

		// If the file isn't part of our root directories, we simply add its directory as a new group.
		// It means that the group is dedicated to the directory, and as such its CommonDir is the directory itself.
		// This should be an edge case (typically the uproject file) .
		if (!bDirFound)
		{
			FString Path = FPaths::GetPath(File) + TEXT("/");
			FFilesInCommonDir* ExistingGroup = GroupOfFiles.Find(Path);
			if (ExistingGroup != nullptr)
			{
				ExistingGroup->Files.Add(File);
			}
			else
			{
				GroupOfFiles.Add(Path, { MoveTemp(Path), {File} });
			}
		}
	}

	if (InFiles.Num() > 0)
	{
		UE_LOG(LogSourceControl, Verbose, TEXT("RunUpdateStatus: %d file(s)/%d directory(ies) ('%s'...)"), InFiles.Num(), GroupOfFiles.Num(), *InFiles[0]);
	}
	else
	{
		UE_LOG(LogSourceControl, Warning, TEXT("RunUpdateStatus: NO file"));
	}

	// 2) then we can batch Plastic status operation by subdirectory
	for (auto& Group : GroupOfFiles)
	{
		const bool bWholeDirectory = ((Group.Value.Files.Num() == 1) && (Group.Value.CommonDir == Group.Value.Files[0]));

		// Run a "status" command on the directory to get workspace file states.
		// (ie. Changed, CheckedOut, Copied, Replaced, Added, Private, Ignored, Deleted, LocallyDeleted, Moved, LocallyMoved)
		TArray<FPlasticSourceControlState> States;
		const bool bGroupOk = RunStatus(Group.Value.CommonDir, MoveTemp(Group.Value.Files), InConcurrency, OutErrorMessages, States, OutChangeset, OutBranchName);
		if (bGroupOk && (States.Num() > 0))
		{
			// Run a "fileinfo" command to update complementary status information of given files.
			// (ie RevisionChangeset, RevisionHeadChangeset, RepSpec, LockedBy, LockedWhere)
			// In case of "whole directory status", there is no explicit file in the group (it contains only the directory)
			// => work on the list of files discovered by RunStatus()
			bResults &= RunFileinfo(bWholeDirectory, bInUpdateHistory, InConcurrency, OutErrorMessages, States);
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
	FString FullCommand = TEXT("cat \"");
	FullCommand += InRevSpec;
	FullCommand += TEXT("\" --raw --file=\"");
	FullCommand += InDumpFileName;
	FullCommand += TEXT("\"");

	UE_LOG(LogSourceControl, Verbose, TEXT("RunDumpToFile: '%s %s'"), *InPathToPlasticBinary, *FullCommand);
	const bool bResult = FPlatformProcess::ExecProcess(*InPathToPlasticBinary, *FullCommand, &ReturnCode, &Results, &Errors);
	UE_LOG(LogSourceControl, Log, TEXT("RunDumpToFile: ExecProcess ReturnCode=%d Results='%s'"), ReturnCode, *Results);
	if (!bResult || !Errors.IsEmpty())
	{
		UE_LOG(LogSourceControl, Error, TEXT("RunDumpToFile: ExecProcess ReturnCode=%d Errors='%s'"), ReturnCode, *Errors);
	}

	return bResult;
}

FString DecodeXmlEntities(const FString& InString)
{
	FString String = InString;
	String.ReplaceInline(TEXT("&amp;"), TEXT("&"));
	String.ReplaceInline(TEXT("&quot;"), TEXT("\""));
	String.ReplaceInline(TEXT("&apos;"), TEXT("'"));
	String.ReplaceInline(TEXT("&lt;"), TEXT("<"));
	String.ReplaceInline(TEXT("&gt;"), TEXT(">"));
	return String;
}

/**
 * Parse results of the 'cm history --moveddeleted --xml --encoding="utf-8"' command.
 * 
 * Results of the history command looks like that:
<RevisionHistoriesResult>
  <RevisionHistories>
	<RevisionHistory>
	  <ItemName>C:/Workspace/UE4PlasticPluginDev/Content/FirstPersonBP/Blueprints/BP_TestsRenamed.uasset</ItemName>
	  <Revisions>
		<Revision>
		  <RevisionSpec>C:/Workspace/UE4PlasticPluginDev/Content/FirstPersonBP/Blueprints/BP_TestsRenamed.uasset#cs:7</RevisionSpec>
		  <Branch>/main</Branch>
		  <CreationDate>2019-10-14T09:52:07+02:00</CreationDate>
		  <RevisionType>bin</RevisionType>
		  <ChangesetNumber>7</ChangesetNumber>
		  <Owner>SRombauts</Owner>
		  <Comment>New tests</Comment>
		  <Repository>UE4PlasticPluginDev</Repository>
		  <Server>localhost:8087</Server>
		  <RepositorySpec>UE4PlasticPluginDev@localhost:8087</RepositorySpec>
		</Revision>
		...
		<Revision>
		  <RevisionSpec>C:/Workspace/UE4PlasticPluginDev/Content/FirstPersonBP/Blueprints/BP_TestsRenamed.uasset#cs:12</RevisionSpec>
		  <Branch>Removed /Content/FirstPersonBP/Blueprints/BP_TestsRenamed.uasset</Branch>
		  <CreationDate>2022-04-28T16:00:37+02:00</CreationDate>
		  <RevisionType />
		  <ChangesetNumber>12</ChangesetNumber>
		  <Owner>sebastien.rombauts</Owner>
		  <Comment />
		  <Repository>UE4PlasticPluginDev</Repository>
		  <Server>localhost:8087</Server>
		  <RepositorySpec>UE4PlasticPluginDev@localhost:8087</RepositorySpec>
		</Revision>

	  </Revisions>
	</RevisionHistory>
	<RevisionHistory>
	  <ItemName>C:/Workspace/UE4PlasticPluginDev/Content/FirstPersonBP/Blueprints/BP_YetAnother.uasset</ItemName>
		...
	</RevisionHistory>
  </RevisionHistories>
</RevisionHistoriesResult>
*/
static bool ParseHistoryResults(const bool bInUpdateHistory, const FXmlFile& InXmlResult, TArray<FPlasticSourceControlState>& InOutStates)
{
	const FPlasticSourceControlProvider& Provider = FPlasticSourceControlModule::Get().GetProvider();
	const FString RootRepSpec = FString::Printf(TEXT("%s@%s"), *Provider.GetRepositoryName(), *Provider.GetServerUrl());

	static const FString RevisionHistoriesResult(TEXT("RevisionHistoriesResult"));
	static const FString RevisionHistories(TEXT("RevisionHistories"));
	static const FString RevisionHistory(TEXT("RevisionHistory"));
	static const FString ItemName(TEXT("ItemName"));
	static const FString Revisions(TEXT("Revisions"));
	static const FString Revision(TEXT("Revision"));
	static const FString Branch(TEXT("Branch"));
	static const FString CreationDate(TEXT("CreationDate"));
	static const FString RevisionType(TEXT("RevisionType"));
	static const FString ChangesetNumber(TEXT("ChangesetNumber"));
	static const FString Owner(TEXT("Owner"));
	static const FString Comment(TEXT("Comment"));

	const FXmlNode* RevisionHistoriesResultNode = InXmlResult.GetRootNode();
	if (RevisionHistoriesResultNode == nullptr || RevisionHistoriesResultNode->GetTag() != RevisionHistoriesResult)
	{
		return false;
	}

	const FXmlNode* RevisionHistoriesNode = RevisionHistoriesResultNode->FindChildNode(RevisionHistories);
	if (RevisionHistoriesNode == nullptr)
	{
		return false;
	}

	const TArray<FXmlNode*>& RevisionHistoryNodes = RevisionHistoriesNode->GetChildrenNodes();
	for (const FXmlNode* RevisionHistoryNode : RevisionHistoryNodes)
	{
		const FXmlNode* ItemNameNode = RevisionHistoryNode->FindChildNode(ItemName);
		if (ItemNameNode == nullptr)
		{
			continue;
		}

		const FString Filename = ItemNameNode->GetContent();
		FPlasticSourceControlState* InOutStatePtr = InOutStates.FindByPredicate(
			[&Filename](const FPlasticSourceControlState& State) { return State.LocalFilename == Filename; }
		); // NOLINT(whitespace/parens) "Closing ) should be moved to the previous line" doesn't work well for lambda functions
		if (InOutStatePtr == nullptr)
		{
			continue;
		}
		FPlasticSourceControlState& InOutState = *InOutStatePtr;

		const FXmlNode* RevisionsNode = RevisionHistoryNode->FindChildNode(Revisions);
		if (RevisionsNode == nullptr)
		{
			continue;
		}

		const TArray<FXmlNode*>& RevisionNodes = RevisionsNode->GetChildrenNodes();
		if (bInUpdateHistory)
		{
			InOutState.History.Reserve(RevisionNodes.Num());
		}

		// parse history in reverse: needed to get most recent at the top (implied by the UI)
		// Note: limit to last 100 changes, like Perforce
		static const int32 MaxRevisions = 100;
		const int32 MinIndex = FMath::Max(0, RevisionNodes.Num() - MaxRevisions);
		for (int32 Index = RevisionNodes.Num() - 1; Index >= MinIndex; Index--)
		{
			if (const FXmlNode* RevisionNode = RevisionNodes[Index])
			{
#if ENGINE_MAJOR_VERSION == 4
				const TSharedRef<FPlasticSourceControlRevision, ESPMode::ThreadSafe> SourceControlRevision = MakeShareable(new FPlasticSourceControlRevision);
#elif ENGINE_MAJOR_VERSION == 5
				const TSharedRef<FPlasticSourceControlRevision, ESPMode::ThreadSafe> SourceControlRevision = MakeShared<FPlasticSourceControlRevision>();
#endif
				SourceControlRevision->State = &InOutState;
				SourceControlRevision->Filename = Filename;
				SourceControlRevision->RevisionId = Index + 1;

				if (const FXmlNode* RevisionTypeNode = RevisionNode->FindChildNode(RevisionType))
				{
					if (!RevisionTypeNode->GetContent().IsEmpty())
					{
						if (Index == 0)
						{
							SourceControlRevision->Action = TEXT("add");
						}
						else
						{
							SourceControlRevision->Action = TEXT("edit");
						}
					}
					else
					{
						SourceControlRevision->Action = TEXT("delete");
					}
				}

				if (const FXmlNode* ChangesetNumberNode = RevisionNode->FindChildNode(ChangesetNumber))
				{
					const FString& Changeset = ChangesetNumberNode->GetContent();
					SourceControlRevision->ChangesetNumber = FCString::Atoi(*Changeset); // Value now used in the Revision column and in the Asset Menu History

					// Also append depot name to the revision, but only when it is different from the default one (ie for xlinks sub repository)
					if (!InOutState.RepSpec.IsEmpty() && (InOutState.RepSpec != RootRepSpec))
					{
						TArray<FString> RepSpecs;
						InOutState.RepSpec.ParseIntoArray(RepSpecs, TEXT("@"));
						SourceControlRevision->Revision = FString::Printf(TEXT("cs:%s@%s"), *Changeset, *RepSpecs[0]);
					}
					else
					{
						SourceControlRevision->Revision = FString::Printf(TEXT("cs:%s"), *Changeset);
					}
				}
				if (const FXmlNode* CommentNode = RevisionNode->FindChildNode(Comment))
				{
					SourceControlRevision->Description = DecodeXmlEntities(CommentNode->GetContent());
				}
				if (const FXmlNode* OwnerNode = RevisionNode->FindChildNode(Owner))
				{
					SourceControlRevision->UserName = UserNameToDisplayName(OwnerNode->GetContent());
				}
				if (const FXmlNode* DateNode = RevisionNode->FindChildNode(CreationDate))
				{
					FString DateIso = DateNode->GetContent();
					const int len = DateIso.Len();
					if (DateIso.Len() > 29)
					{	//                           |--|
						//    2016-04-18T10:44:49.0000000+02:00
						// => 2016-04-18T10:44:49.000+02:00
						DateIso = DateNode->GetContent().LeftChop(10) + DateNode->GetContent().RightChop(27);
					}
					FDateTime::ParseIso8601(*DateIso, SourceControlRevision->Date);
				}
				if (const FXmlNode* BranchNode = RevisionNode->FindChildNode(Branch))
				{
					SourceControlRevision->Branch = BranchNode->GetContent();
				}

				// Detect and skip more recent changesets on other branches (ie above the RevisionHeadChangeset)
				if (SourceControlRevision->ChangesetNumber > InOutState.DepotRevisionChangeset)
				{
					InOutState.HeadBranch = SourceControlRevision->Branch;
					InOutState.HeadAction = SourceControlRevision->Action;
					InOutState.HeadChangeList = SourceControlRevision->ChangesetNumber;
					InOutState.HeadUserName = SourceControlRevision->UserName;
					InOutState.HeadModTime = SourceControlRevision->Date.ToUnixTimestamp();
				}
				else if (bInUpdateHistory)
				{
					InOutState.History.Add(SourceControlRevision);
				}

				// Also grab the UserName of the author of the current depot/head changeset
				if ((SourceControlRevision->ChangesetNumber == InOutState.DepotRevisionChangeset) && InOutState.HeadUserName.IsEmpty())
				{
					InOutState.HeadUserName = SourceControlRevision->UserName;
				}

				if (!bInUpdateHistory)
				{
					break; // if not updating the history, just getting the head of the latest branch is enough
				}
			}
		}
	}

	return true;
}

// Run a Plastic "history" command and parse it's XML result.
bool RunGetHistory(const bool bInUpdateHistory, TArray<FPlasticSourceControlState>& InOutStates, TArray<FString>& OutErrorMessages)
{
	bool bResult = true;
	FString Results;
	FString Errors;
	TArray<FString> Parameters;
	// Detecting move and deletion is costly as it is implemented as two extra queries to the server; do it only when getting the history of the current branch
	if (bInUpdateHistory)
	{
		Parameters.Add(TEXT("--moveddeleted"));
	}
	Parameters.Add(TEXT("--xml"));
	Parameters.Add(TEXT("--encoding=\"utf-8\""));

	TArray<FString> Files;
	Files.Reserve(InOutStates.Num());
	for (const FPlasticSourceControlState& State : InOutStates)
	{
		// When getting only the last revision, optimize out if DepotRevisionChangeset is invalid (ie "fileinfo" was optimized out, eg for checked-out files)
		if (!bInUpdateHistory && State.DepotRevisionChangeset == ISourceControlState::INVALID_REVISION)
			continue;

		if (State.IsSourceControlled() && !State.IsAdded())
		{
			Files.Add(State.LocalFilename);
		}
	}
	if (Files.Num() > 0)
	{
		bResult = RunCommandInternal(TEXT("history"), Parameters, Files, EConcurrency::Synchronous, Results, Errors);
		if (bResult)
		{
			FXmlFile XmlFile;
			bResult = XmlFile.LoadFile(Results, EConstructMethod::ConstructFromBuffer);
			if (bResult)
			{
				bResult = ParseHistoryResults(bInUpdateHistory, XmlFile, InOutStates);
			}
		}
		if (!Errors.IsEmpty())
		{
			OutErrorMessages.Add(MoveTemp(Errors));
		}
	}

	return bResult;
}

#if ENGINE_MAJOR_VERSION == 5

/**
 * Parse results of the 'cm status --changelists --controlledchanged --xml --encoding="utf-8"' command.
 *
 * Results of the status changelists command looks like that:
<StatusOutput>
  <WorkspaceStatus>
	<Status>
	  <RepSpec>
		<Server>test@cloud</Server>
		<Name>UEPlasticPluginDev</Name>
	  </RepSpec>
	  <Changeset>11</Changeset>
	</Status>
  </WorkspaceStatus>
  <WkConfigType>Branch</WkConfigType>
  <WkConfigName>/main@rep:UEPlasticPluginDev@repserver:test@cloud</WkConfigName>
  <Changelists>
	<Changelist>
	  <Name>Default</Name>
	  <Description>Default Plastic SCM changelist</Description>
	  <Changes>
		<Change>
		  <Type>CO</Type>
		  <TypeVerbose>Checked-out</TypeVerbose>
		  <Path>UEPlasticPluginDev.uproject</Path>
		  <OldPath />
		  <PrintableMovedPath />
		  <MergesInfo />
		  <SimilarityPerUnit>0</SimilarityPerUnit>
		  <Similarity />
		  <Size>583</Size>
		  <PrintableSize>583 bytes</PrintableSize>
		  <PrintableLastModified>6 days ago</PrintableLastModified>
		  <RevisionType>enTextFile</RevisionType>
		  <LastModified>2022-06-07T12:28:32+02:00</LastModified>
		</Change>
		[...]
		<Change>
		</Change>
	  </Changes>
	</Changelist>
  </Changelists>
</StatusOutput>
*/
static bool ParseChangelistsResults(const FXmlFile& InXmlResult, TArray<FPlasticSourceControlChangelistState>& OutChangelistsStates, TArray<TArray<FPlasticSourceControlState>>& OutCLFilesStates)
{
	static const FString StatusOutput(TEXT("StatusOutput"));
	static const FString WkConfigType(TEXT("WkConfigType"));
	static const FString WkConfigName(TEXT("WkConfigName"));
	static const FString Changelists(TEXT("Changelists"));
	static const FString Changelist(TEXT("Changelist"));
	static const FString Name(TEXT("Name"));
	static const FString Description(TEXT("Description"));
	static const FString Changes(TEXT("Changes"));
	static const FString Change(TEXT("Change"));
	static const FString Type(TEXT("Type"));
	static const FString Path(TEXT("Path"));

	const FString& WorkingDirectory = FPlasticSourceControlModule::Get().GetProvider().GetPathToWorkspaceRoot();

	const FXmlNode* StatusOutputNode = InXmlResult.GetRootNode();
	if (StatusOutputNode == nullptr || StatusOutputNode->GetTag() != StatusOutput)
	{
		return false;
	}

	const FXmlNode* ChangelistsNode = StatusOutputNode->FindChildNode(Changelists);
	if (ChangelistsNode)
	{
		const TArray<FXmlNode*>& ChangelistNodes = ChangelistsNode->GetChildrenNodes();
		OutCLFilesStates.SetNum(ChangelistNodes.Num());
		for (int32 ChangelistIndex = 0; ChangelistIndex < ChangelistNodes.Num(); ChangelistIndex++)
		{
			const FXmlNode* ChangelistNode = ChangelistNodes[ChangelistIndex];
			check(ChangelistNode);
			const FXmlNode* NameNode = ChangelistNode->FindChildNode(Name);
			const FXmlNode* DescriptionNode = ChangelistNode->FindChildNode(Description);
			const FXmlNode* ChangesNode = ChangelistNode->FindChildNode(Changes);
			if (NameNode == nullptr || DescriptionNode == nullptr || ChangesNode == nullptr)
			{
				continue;
			}

			FString NameTemp = PlasticSourceControlUtils::DecodeXmlEntities(NameNode->GetContent());
			FPlasticSourceControlChangelist ChangelistTemp(MoveTemp(NameTemp), true);
			FString DescriptionTemp = PlasticSourceControlUtils::DecodeXmlEntities(DescriptionNode->GetContent());
			FPlasticSourceControlChangelistState ChangelistState(MoveTemp(ChangelistTemp), MoveTemp(DescriptionTemp));

			const TArray<FXmlNode*>& ChangeNodes = ChangesNode->GetChildrenNodes();
			for (const FXmlNode* ChangeNode : ChangeNodes)
			{
				check(ChangeNode);
				const FXmlNode* PathNode = ChangeNode->FindChildNode(Path);
				const FXmlNode* TypeNode = ChangeNode->FindChildNode(Type);
				if (PathNode == nullptr || TypeNode == nullptr)
				{
					continue;
				}

				// Here we make sure to only collect file states, since we shouldn't display the added directories to the Editor
				FString FileName = PathNode->GetContent();
				int32 DotIndex;
				if (FileName.FindChar(TEXT('.'), DotIndex))
				{
					FPlasticSourceControlState FileState(FPaths::ConvertRelativePathToFull(WorkingDirectory, MoveTemp(FileName)));
					FileState.WorkspaceState = PlasticSourceControlUtils::StateFromPlasticStatus(TypeNode->GetContent());
					FileState.Changelist = ChangelistState.Changelist;
					OutCLFilesStates[ChangelistIndex].Add(MoveTemp(FileState));
				}
			}

			OutChangelistsStates.Add(ChangelistState);
		}
	}

	if (!OutChangelistsStates.FindByPredicate(
		[](const FPlasticSourceControlChangelistState& CLState) { return CLState.Changelist.GetName() == FPlasticSourceControlChangelist::DefaultChangelist.GetName(); }
	))
	{
		// No Default Changelists isn't an error, but the Editor UX expects to always the Default changelist (so you can always move files back to it)
		FPlasticSourceControlChangelistState DefaultChangelistState(FPlasticSourceControlChangelist::DefaultChangelist, TEXT("Default Plastic SCM changelist"));
		OutChangelistsStates.Insert(DefaultChangelistState, 0);
		OutCLFilesStates.Insert(TArray<FPlasticSourceControlState>(), 0);
	}

	return true;
}

bool RunGetChangelists(const EConcurrency::Type InConcurrency, TArray<FPlasticSourceControlChangelistState>& OutChangelistsStates, TArray<TArray<FPlasticSourceControlState>>& OutCLFilesStates, TArray<FString>& OutErrorMessages)
{
	bool bCommandSuccessful;

	FString Results;
	FString Errors;
	TArray<FString> Parameters;
	Parameters.Add(TEXT("--changelists"));
	Parameters.Add(TEXT("--controlledchanged"));
	Parameters.Add(TEXT("--noheader"));
	Parameters.Add(TEXT("--xml"));
	Parameters.Add(TEXT("--encoding=\"utf-8\""));
	bCommandSuccessful = PlasticSourceControlUtils::RunCommandInternal(TEXT("status"), Parameters, TArray<FString>(), InConcurrency, Results, Errors);
	if (bCommandSuccessful)
	{
		FXmlFile XmlFile;
		bCommandSuccessful = XmlFile.LoadFile(Results, EConstructMethod::ConstructFromBuffer);
		if (bCommandSuccessful)
		{
			bCommandSuccessful = ParseChangelistsResults(XmlFile, OutChangelistsStates, OutCLFilesStates);
		}
	}
	if (!Errors.IsEmpty())
	{
		OutErrorMessages.Add(MoveTemp(Errors));
	}

	return bCommandSuccessful;
}

#endif

bool UpdateCachedStates(TArray<FPlasticSourceControlState>&& InStates)
{
	FPlasticSourceControlProvider& Provider = FPlasticSourceControlModule::Get().GetProvider();
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
	explicit FRemoveRedundantErrors(const FString& InFilter)
		: Filter(InFilter)
	{
	}

	bool operator()(const FString& String) const
	{
		if (String.Contains(Filter))
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
	for (const FString& ErrorMessage : InCommand.ErrorMessages)
	{
		if (ErrorMessage.Contains(InFilter, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
		{
			InCommand.InfoMessages.Add(ErrorMessage);
			bFoundRedundantError = true;
		}
	}

	InCommand.ErrorMessages.RemoveAll(FRemoveRedundantErrors(InFilter));

	// if we have no error messages now, assume success!
	if (bFoundRedundantError && InCommand.ErrorMessages.Num() == 0 && !InCommand.bCommandSuccessful)
	{
		InCommand.bCommandSuccessful = true;
	}
}

void SwitchVerboseLogs(const bool bInEnable)
{
	if (bInEnable && LogSourceControl.GetVerbosity() < ELogVerbosity::Verbose)
	{
		LogSourceControl.SetVerbosity(ELogVerbosity::Verbose);
	}
	else if (!bInEnable && LogSourceControl.GetVerbosity() == ELogVerbosity::Verbose)
	{
		LogSourceControl.SetVerbosity(ELogVerbosity::Log);
	}
}

} // namespace PlasticSourceControlUtils
