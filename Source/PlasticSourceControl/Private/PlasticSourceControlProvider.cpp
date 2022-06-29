// Copyright (c) 2016-2022 Codice Software

#include "PlasticSourceControlProvider.h"

#include "PlasticSourceControlCommand.h"
#include "PlasticSourceControlOperations.h"
#include "PlasticSourceControlProjectSettings.h"
#include "PlasticSourceControlSettings.h"
#include "PlasticSourceControlState.h"
#include "PlasticSourceControlUtils.h"
#include "SPlasticSourceControlSettings.h"

#include "ISourceControlModule.h"
#include "Logging/MessageLog.h"
#include "ScopedSourceControlProgress.h"
#include "SourceControlHelpers.h"
#include "SourceControlOperations.h"
#include "Interfaces/IPluginManager.h"

#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "Misc/QueuedThreadPool.h"

#define LOCTEXT_NAMESPACE "PlasticSourceControl"

static FName ProviderName("Plastic SCM");

FPlasticSourceControlProvider::FPlasticSourceControlProvider()
{
	// load our settings
	PlasticSourceControlSettings.LoadSettings();
}

void FPlasticSourceControlProvider::Init(bool bForceConnection)
{
	// Init() is called multiple times at startup: do not check Plastic SCM each time
	if (!bPlasticAvailable)
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("PlasticSourceControl"));
		if (Plugin.IsValid())
		{
			PluginVersion = Plugin->GetDescriptor().VersionName;
			UE_LOG(LogSourceControl, Log, TEXT("Plastic SCM plugin '%s'"), *PluginVersion);
		}

		CheckPlasticAvailability();

		// Override the source control logs verbosity level if needed based on settings
		if (AccessSettings().GetEnableVerboseLogs())
		{
			PlasticSourceControlUtils::SwitchVerboseLogs(true);
		}
	}

	if (bForceConnection && !bServerAvailable)
	{
		// Execute a 'checkconnection' command to set bServerAvailable based on the connectivity of the server
		TArray<FString> InfoMessages, ErrorMessages;
		const bool bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("checkconnection"), TArray<FString>(), TArray<FString>(), EConcurrency::Synchronous, InfoMessages, ErrorMessages);
		bServerAvailable = bCommandSuccessful;
		if (!bCommandSuccessful)
		{
			FMessageLog SourceControlLog("SourceControl");
			for (const FString& ErrorMessage : ErrorMessages)
			{
				SourceControlLog.Error(FText::FromString(ErrorMessage));
			}
		}
	}
}

void FPlasticSourceControlProvider::CheckPlasticAvailability()
{
	FString PathToPlasticBinary = AccessSettings().GetBinaryPath();
	if (PathToPlasticBinary.IsEmpty())
	{
		bPlasticAvailable = false;

		// Try to find Plastic binary, and update settings accordingly
		PathToPlasticBinary = PlasticSourceControlUtils::FindPlasticBinaryPath();
		if (!PathToPlasticBinary.IsEmpty())
		{
			AccessSettings().SetBinaryPath(PathToPlasticBinary);
		}
	}

	if (!PathToPlasticBinary.IsEmpty())
	{
		// Find the path to the root Plastic directory (if any, else uses the ProjectDir)
		const FString PathToProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
		bWorkspaceFound = PlasticSourceControlUtils::FindRootDirectory(PathToProjectDir, PathToWorkspaceRoot);

		// Launch the Plastic SCM cli shell on the background to issue all commands during this session
		bPlasticAvailable = PlasticSourceControlUtils::LaunchBackgroundPlasticShell(PathToPlasticBinary, PathToWorkspaceRoot);
		if (bPlasticAvailable)
		{
			PlasticSourceControlUtils::GetPlasticScmVersion(PlasticScmVersion);

			// Get user name (from the global Plastic SCM client config)
			PlasticSourceControlUtils::GetUserName(UserName);

			// Register Console Commands
			PlasticSourceControlConsole.Register();

			if (!bWorkspaceFound)
			{
				FFormatNamedArguments Args;
				Args.Add(TEXT("WorkspacePath"), FText::FromString(PathToWorkspaceRoot));
				FMessageLog("SourceControl").Info(FText::Format(LOCTEXT("NotInAWorkspace", "{WorkspacePath} is not in a workspace."), Args));
			}
		}
	}
}

void FPlasticSourceControlProvider::Close()
{
	// clear the cache
	StateCache.Empty();
	// terminate the background 'cm shell' process and associated pipes
	PlasticSourceControlUtils::Terminate();
	// Remove all extensions to the "Source Control" menu in the Editor Toolbar
	PlasticSourceControlMenu.Unregister();
	// Unregister Console Commands
	PlasticSourceControlConsole.Unregister();

	bServerAvailable = false;
	bPlasticAvailable = false;
	bWorkspaceFound = false;
	UserName.Empty();
}

TSharedRef<FPlasticSourceControlState, ESPMode::ThreadSafe> FPlasticSourceControlProvider::GetStateInternal(const FString& InFilename) const
{
	TSharedRef<FPlasticSourceControlState, ESPMode::ThreadSafe>* State = StateCache.Find(InFilename);
	if (State != NULL)
	{
		// found cached item
		return (*State);
	}
	else
	{
		// cache an unknown state for this item
		TSharedRef<FPlasticSourceControlState, ESPMode::ThreadSafe> NewState = MakeShareable(new FPlasticSourceControlState(FString(InFilename)));
		StateCache.Add(InFilename, NewState);
		return NewState;
	}
}

FText FPlasticSourceControlProvider::GetStatusText() const
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("PlasticScmVersion"), FText::FromString(PlasticScmVersion));
	Args.Add(TEXT("PluginVersion"), FText::FromString(PluginVersion));
	Args.Add(TEXT("WorkspacePath"), FText::FromString(PathToWorkspaceRoot));
	Args.Add(TEXT("WorkspaceName"), FText::FromString(WorkspaceName));
	Args.Add(TEXT("BranchName"), FText::FromString(BranchName));
	// Detect special case for a partial checkout (CS:-1 in Gluon mode)!
	if (-1 != ChangesetNumber)
	{
		Args.Add(TEXT("ChangesetNumber"), FText::FromString(FString::Printf(TEXT("%d  (standard full workspace)"), ChangesetNumber)));
	}
	else
	{
		Args.Add(TEXT("ChangesetNumber"), FText::FromString(FString::Printf(TEXT("N/A  (Gluon/partial workspace)"))));
	}
	Args.Add(TEXT("UserName"), FText::FromString(UserName));
	const FString DisplayName = PlasticSourceControlUtils::UserNameToDisplayName(UserName);
	if (DisplayName != UserName)
	{
		Args.Add(TEXT("DisplayName"), FText::FromString(TEXT("(Display: ") + DisplayName + TEXT(")")));
	}
	else
	{
		Args.Add(TEXT("DisplayName"), FText::GetEmpty());
	}

	FText FormattedError;
	TArray<FString> RecentErrors = GetLastErrors();
	if (RecentErrors.Num() > 0)
	{
		FFormatNamedArguments ErrorArgs;
		ErrorArgs.Add(TEXT("ErrorText"), FText::FromString(RecentErrors[0]));

		FormattedError = FText::Format(LOCTEXT("PlasticErrorStatusText", "Error: {ErrorText} {UserName}\n\n"), ErrorArgs);
	}
	Args.Add(TEXT("ErrorText"), FormattedError);

	return FText::Format(LOCTEXT("PlasticStatusText", "{ErrorText}Plastic SCM {PlasticScmVersion}  (plugin v{PluginVersion})\nWorkspace: {WorkspaceName}  ({WorkspacePath})\n{BranchName}\nChangeset: {ChangesetNumber}\nUser: '{UserName}'  {DisplayName}"), Args);
}

/** Quick check if source control is enabled. Specifically, it returns true if a source control provider is set (regardless of whether the provider is available) and false if no provider is set. So all providers except the stub DefaultSourceProvider will return true. */
bool FPlasticSourceControlProvider::IsEnabled() const
{
	return true;
}

/** Quick check if source control is available for use (return whether the server is available or not) */
bool FPlasticSourceControlProvider::IsAvailable() const
{
	return bServerAvailable;
}

const FName& FPlasticSourceControlProvider::GetName(void) const
{
	return ProviderName;
}

void FPlasticSourceControlProvider::SetLastErrors(const TArray<FString>& InErrors)
{
	FScopeLock Lock(&LastErrorsCriticalSection);
	LastErrors = InErrors;
}

TArray<FString> FPlasticSourceControlProvider::GetLastErrors() const
{
	FScopeLock Lock(&LastErrorsCriticalSection);
	TArray<FString> Result = LastErrors;
	return Result;
}

ECommandResult::Type FPlasticSourceControlProvider::GetState(const TArray<FString>& InFiles, TArray< TSharedRef<ISourceControlState, ESPMode::ThreadSafe> >& OutState, EStateCacheUsage::Type InStateCacheUsage)
{
	if (!IsEnabled())
	{
		return ECommandResult::Failed;
	}

	const TArray<FString> AbsoluteFiles = SourceControlHelpers::AbsoluteFilenames(InFiles);

	if (InStateCacheUsage == EStateCacheUsage::ForceUpdate)
	{
		UE_LOG(LogSourceControl, Log, TEXT("GetState: ForceUpdate"));
		Execute(ISourceControlOperation::Create<FUpdateStatus>(), AbsoluteFiles);
	}

	for (const FString& AbsoluteFile : AbsoluteFiles)
	{
		OutState.Add(GetStateInternal(AbsoluteFile));
	}

	return ECommandResult::Succeeded;
}

#if ENGINE_MAJOR_VERSION == 5
// TODO UE5 Changelist
ECommandResult::Type FPlasticSourceControlProvider::GetState(const TArray<FSourceControlChangelistRef>& InChangelists, TArray<FSourceControlChangelistStateRef>& OutState, EStateCacheUsage::Type InStateCacheUsage)
{
	return ECommandResult::Failed;
}
#endif

TArray<FSourceControlStateRef> FPlasticSourceControlProvider::GetCachedStateByPredicate(TFunctionRef<bool(const FSourceControlStateRef&)> Predicate) const
{
	TArray<FSourceControlStateRef> Result;
	for (const auto& CacheItem : StateCache)
	{
		FSourceControlStateRef State = CacheItem.Value;
		if (Predicate(State))
		{
			Result.Add(State);
		}
	}
	return Result;
}

bool FPlasticSourceControlProvider::RemoveFileFromCache(const FString& Filename)
{
	return StateCache.Remove(Filename) > 0;
}

FDelegateHandle FPlasticSourceControlProvider::RegisterSourceControlStateChanged_Handle(const FSourceControlStateChanged::FDelegate& SourceControlStateChanged)
{
	return OnSourceControlStateChanged.Add(SourceControlStateChanged);
}

void FPlasticSourceControlProvider::UnregisterSourceControlStateChanged_Handle(FDelegateHandle Handle)
{
	OnSourceControlStateChanged.Remove(Handle);
}

ECommandResult::Type FPlasticSourceControlProvider::Execute(
	const FSourceControlOperationRef& InOperation,
#if ENGINE_MAJOR_VERSION == 5
	FSourceControlChangelistPtr InChangelist,
#endif
	const TArray<FString>& InFiles,
	EConcurrency::Type InConcurrency,
	const FSourceControlOperationComplete& InOperationCompleteDelegate )
{
	if (!bWorkspaceFound && !(InOperation->GetName() == "Connect") && !(InOperation->GetName() == "MakeWorkspace"))
	{
		UE_LOG(LogSourceControl, Warning, TEXT("'%s': only Connect operation allowed without a workspace"), *InOperation->GetName().ToString());
		return ECommandResult::Failed;
	}

	// Query to see if we allow this operation
	TSharedPtr<IPlasticSourceControlWorker, ESPMode::ThreadSafe> Worker = CreateWorker(InOperation->GetName());
	if (!Worker.IsValid())
	{
		// this operation is unsupported by this source control provider
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("OperationName"), FText::FromName(InOperation->GetName()));
		Arguments.Add(TEXT("ProviderName"), FText::FromName(GetName()));
		FMessageLog("SourceControl").Error(FText::Format(LOCTEXT("UnsupportedOperation", "Operation '{OperationName}' not supported by source control provider '{ProviderName}'"), Arguments));
		return ECommandResult::Failed;
	}

	FPlasticSourceControlCommand* Command = new FPlasticSourceControlCommand(InOperation, Worker.ToSharedRef());
	Command->Files = SourceControlHelpers::AbsoluteFilenames(InFiles);
	Command->OperationCompleteDelegate = InOperationCompleteDelegate;

	// fire off operation
	if (InConcurrency == EConcurrency::Synchronous)
	{
		Command->bAutoDelete = false;

		UE_LOG(LogSourceControl, Log, TEXT("ExecuteSynchronousCommand: %s"), *InOperation->GetName().ToString());
		return ExecuteSynchronousCommand(*Command, InOperation->GetInProgressString());
	}
	else
	{
		Command->bAutoDelete = true;

		UE_LOG(LogSourceControl, Log, TEXT("IssueAsynchronousCommand: %s"), *InOperation->GetName().ToString());
		return IssueCommand(*Command);
	}
}

bool FPlasticSourceControlProvider::CanCancelOperation(const FSourceControlOperationRef& InOperation) const
{
	return false;
}

void FPlasticSourceControlProvider::CancelOperation(const FSourceControlOperationRef& InOperation)
{
}

bool FPlasticSourceControlProvider::UsesLocalReadOnlyState() const
{
	return false; // TODO: use configuration
}

bool FPlasticSourceControlProvider::UsesChangelists() const
{
	return false; // We don't want to show ChangeList column anymore (Plastic SCM term would be ChangeSet)
}

bool FPlasticSourceControlProvider::UsesCheckout() const
{
	return GetDefault<UPlasticSourceControlProjectSettings>()->bPromptForCheckoutOnChange;
}

TSharedPtr<IPlasticSourceControlWorker, ESPMode::ThreadSafe> FPlasticSourceControlProvider::CreateWorker(const FName& InOperationName)
{
	const FGetPlasticSourceControlWorker* Operation = WorkersMap.Find(InOperationName);
	if (Operation != nullptr)
	{
		return Operation->Execute(*this);
	}

	return nullptr;
}

void FPlasticSourceControlProvider::RegisterWorker(const FName& InName, const FGetPlasticSourceControlWorker& InDelegate)
{
	WorkersMap.Add(InName, InDelegate);
}

void FPlasticSourceControlProvider::OutputCommandMessages(const FPlasticSourceControlCommand& InCommand) const
{
	FMessageLog SourceControlLog("SourceControl");

	for (const FString& ErrorMessage : InCommand.ErrorMessages)
	{
		SourceControlLog.Error(FText::FromString(ErrorMessage));
	}

	for (const FString& InfoMessage : InCommand.InfoMessages)
	{
		SourceControlLog.Info(FText::FromString(InfoMessage));
	}
}

void FPlasticSourceControlProvider::UpdateWorkspaceStatus(const class FPlasticSourceControlCommand& InCommand)
{
	if (InCommand.Operation->GetName() == "Connect")
	{
		// Is connection successful?
		bServerAvailable = InCommand.bCommandSuccessful;
		bWorkspaceFound = !InCommand.WorkspaceName.IsEmpty();

		WorkspaceName = InCommand.WorkspaceName;
		RepositoryName = InCommand.RepositoryName;
		ServerUrl = InCommand.ServerUrl;

		if (bWorkspaceFound)
		{
			// Extend the "Source Control" menu in the Editor Toolbar on each successful connection
			PlasticSourceControlMenu.Unregister(); // cleanup for any previous connection
			PlasticSourceControlMenu.Register();
		}

		SetLastErrors(InCommand.ErrorMessages);
	}
	else if (InCommand.bConnectionDropped)
	{
		// checkconnection failed on UpdateStatus
		bServerAvailable = false;

		SetLastErrors(InCommand.ErrorMessages);
	}
	else if (!bServerAvailable)
	{
		bServerAvailable = InCommand.bCommandSuccessful;

		SetLastErrors(TArray<FString>());
	}

	// And for all operations running UpdateStatus, get Changeset and Branch informations:
	if (InCommand.ChangesetNumber != 0)
	{
		ChangesetNumber = InCommand.ChangesetNumber;
	}
	if (!InCommand.BranchName.IsEmpty())
	{
		BranchName = InCommand.BranchName;
	}
}

void FPlasticSourceControlProvider::Tick()
{
	bool bStatesUpdated = false;
	for (int32 CommandIndex = 0; CommandIndex < CommandQueue.Num(); ++CommandIndex)
	{
		FPlasticSourceControlCommand& Command = *CommandQueue[CommandIndex];
		if (Command.bExecuteProcessed)
		{
			// Remove command from the queue
			CommandQueue.RemoveAt(CommandIndex);

			// Update workspace status and connection state on Connect and UpdateStatus operations
			UpdateWorkspaceStatus(Command);

			// let command update the states of any files
			bStatesUpdated |= Command.Worker->UpdateStates();

			// dump any messages to output log
			OutputCommandMessages(Command);

			if (Command.Files.Num())
			{
				UE_LOG(LogSourceControl, Log, TEXT("%s of %d files processed in %.3lfs"), *Command.Operation->GetName().ToString(), Command.Files.Num(), (FPlatformTime::Seconds() - Command.StartTimestamp));
			}
			else
			{
				UE_LOG(LogSourceControl, Log, TEXT("%s processed in %.3lfs"), *Command.Operation->GetName().ToString(), (FPlatformTime::Seconds() - Command.StartTimestamp));
			}

			// run the completion delegate callback if we have one bound
			ECommandResult::Type Result = Command.bCommandSuccessful ? ECommandResult::Succeeded : ECommandResult::Failed;
			Command.OperationCompleteDelegate.ExecuteIfBound(Command.Operation, Result);

			// commands that are left in the array during a tick need to be deleted
			if (Command.bAutoDelete)
			{
				// Only delete commands that are not running 'synchronously'
				delete &Command;
			}

			// only do one command per tick loop, as we don't want concurrent modification
			// of the command queue (which can happen in the completion delegate)
			break;
		}
	}

	if (bStatesUpdated)
	{
		OnSourceControlStateChanged.Broadcast();
	}
}

TArray< TSharedRef<ISourceControlLabel> > FPlasticSourceControlProvider::GetLabels(const FString& InMatchingSpec) const
{
	TArray< TSharedRef<ISourceControlLabel> > Tags;

	// NOTE list labels. Called by CrashDebugHelper() (to remote debug Engine crash)
	//					 and by SourceControlHelpers::AnnotateFile() (to add source file to report)
	// Reserved for internal use by Epic Games with Perforce only
	return Tags;
}

#if ENGINE_MAJOR_VERSION == 5
// TODO UE5 Changelist
TArray<FSourceControlChangelistRef> FPlasticSourceControlProvider::GetChangelists(EStateCacheUsage::Type InStateCacheUsage)
{
	return TArray<FSourceControlChangelistRef>();
}
#endif

#if SOURCE_CONTROL_WITH_SLATE
TSharedRef<class SWidget> FPlasticSourceControlProvider::MakeSettingsWidget() const
{
	return SNew(SPlasticSourceControlSettings);
}
#endif

ECommandResult::Type FPlasticSourceControlProvider::ExecuteSynchronousCommand(FPlasticSourceControlCommand& InCommand, const FText& Task)
{
	ECommandResult::Type Result = ECommandResult::Failed;

	// Display the progress dialog if a string was provided
	{
		FScopedSourceControlProgress Progress(Task);

		// Issue the command asynchronously...
		IssueCommand(InCommand);

		// ... then wait for its completion (thus making it synchronous)
		while (!InCommand.bExecuteProcessed)
		{
			// Tick the command queue and update progress.
			Tick();

			Progress.Tick();

			// Sleep for a bit so we don't busy-wait so much.
			FPlatformProcess::Sleep(0.01f);
		}

		// always do one more Tick() to make sure the command queue is cleaned up.
		Tick();

		if (InCommand.bCommandSuccessful)
		{
			Result = ECommandResult::Succeeded;
		}
		else
		{
			// TODO If the command failed, inform the user that they need to try again (see Perforce)
			// FMessageDialog::Open( EAppMsgType::Ok, LOCTEXT("Plastic_ServerUnresponsive", "Plastic server is unresponsive. Please check your connection and try again.") );

			UE_LOG(LogSourceControl, Error, TEXT("Command '%s' Failed!"), *InCommand.Operation->GetName().ToString());
		}
	}

	// Delete the command now (asynchronous commands are deleted in the Tick() method)
	check(!InCommand.bAutoDelete);

	// ensure commands that are not auto deleted do not end up in the command queue
	if (CommandQueue.Contains(&InCommand))
	{
		CommandQueue.Remove(&InCommand);
	}
	delete &InCommand;

	return Result;
}

ECommandResult::Type FPlasticSourceControlProvider::IssueCommand(FPlasticSourceControlCommand& InCommand)
{
	if (GThreadPool != nullptr)
	{
		// Queue this to our worker thread(s) for resolving
		GThreadPool->AddQueuedWork(&InCommand);
		CommandQueue.Add(&InCommand);
		return ECommandResult::Succeeded;
	}
	else
	{
		return ECommandResult::Failed;
	}
}

#undef LOCTEXT_NAMESPACE
