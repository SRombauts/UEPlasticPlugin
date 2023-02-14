// Copyright Unity Technologies

#include "PlasticSourceControlProvider.h"

#include "PlasticSourceControlChangelistState.h"
#include "PlasticSourceControlCommand.h"
#include "PlasticSourceControlOperations.h"
#include "PlasticSourceControlProjectSettings.h"
#include "PlasticSourceControlSettings.h"
#include "PlasticSourceControlShell.h"
#include "PlasticSourceControlState.h"
#include "PlasticSourceControlUtils.h"
#include "SPlasticSourceControlSettings.h"

#include "ISourceControlModule.h"
#include "Logging/MessageLog.h"
#include "ScopedSourceControlProgress.h"
#include "SourceControlHelpers.h"
#include "SourceControlOperations.h"
#include "Interfaces/IPluginManager.h"

#include "Algo/Transform.h"
#include "Misc/Paths.h"
#include "Misc/MessageDialog.h"
#include "HAL/PlatformProcess.h"
#include "Misc/QueuedThreadPool.h"

#define LOCTEXT_NAMESPACE "PlasticSourceControl"

static FName ProviderName("Plastic SCM");

FPlasticSourceControlProvider::FPlasticSourceControlProvider()
{
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

	if (bForceConnection && bPlasticAvailable && bWorkspaceFound && !bServerAvailable)
	{
		// Execute a 'checkconnection' command to set bServerAvailable based on the connectivity of the server
		TArray<FString> InfoMessages, ErrorMessages;
		TArray<FString> Parameters;
		if (PlasticSourceControlUtils::GetWorkspaceInfo(BranchName, RepositoryName, ServerUrl, ErrorMessages))
		{
			Parameters.Add(FString::Printf(TEXT("--server=%s"), *ServerUrl));
		}
		bServerAvailable = PlasticSourceControlUtils::RunCommand(TEXT("checkconnection"), Parameters, TArray<FString>(), InfoMessages, ErrorMessages);
		if (!bServerAvailable)
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

		// Launch the Plastic SCM cli shell on the background to issue all commands during this session
		bPlasticAvailable = PlasticSourceControlShell::Launch(PathToPlasticBinary, PathToProjectDir);
		if (!bPlasticAvailable)
		{
			return;
		}

		bPlasticAvailable = PlasticSourceControlUtils::GetPlasticScmVersion(PlasticScmVersion);
		if (!bPlasticAvailable)
		{
			return;
		}

		FString ActualPathToPlasticBinary;
		PlasticSourceControlUtils::GetCmLocation(ActualPathToPlasticBinary);

		bWorkspaceFound = PlasticSourceControlUtils::GetWorkspacePath(PathToProjectDir, PathToWorkspaceRoot);


		bUsesLocalReadOnlyState = PlasticSourceControlUtils::GetConfigSetFilesAsReadOnly();

		// Get user name (from the global Plastic SCM client config)
		PlasticSourceControlUtils::GetUserName(UserName);

		// Register Console Commands
		PlasticSourceControlConsole.Register();

		if (!bWorkspaceFound)
		{
			// This info message is only useful here, if bPlasticAvailable, for the Login window
			FFormatNamedArguments Args;
			Args.Add(TEXT("WorkspacePath"), FText::FromString(PathToWorkspaceRoot));
			FMessageLog("SourceControl").Info(FText::Format(LOCTEXT("NotInAWorkspace", "{WorkspacePath} is not in a workspace."), Args));
		}
	}
}

void FPlasticSourceControlProvider::Close()
{
	// clear the cache
	StateCache.Empty();
	// terminate the background 'cm shell' process and associated pipes
	PlasticSourceControlShell::Terminate();
	// Remove all extensions to the "Source Control" menu in the Editor Toolbar
	PlasticSourceControlMenu.Unregister();
	// Unregister Console Commands
	PlasticSourceControlConsole.Unregister();

	bServerAvailable = false;
	bPlasticAvailable = false;
	bWorkspaceFound = false;
	UserName.Empty();
}

TSharedRef<FPlasticSourceControlState, ESPMode::ThreadSafe> FPlasticSourceControlProvider::GetStateInternal(const FString& InFilename)
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
#if ENGINE_MAJOR_VERSION == 4
		TSharedRef<FPlasticSourceControlState, ESPMode::ThreadSafe> NewState = MakeShareable(new FPlasticSourceControlState(FString(InFilename)));
#elif ENGINE_MAJOR_VERSION == 5
		TSharedRef<FPlasticSourceControlState, ESPMode::ThreadSafe> NewState = MakeShared<FPlasticSourceControlState>(FString(InFilename));
#endif
		StateCache.Add(InFilename, NewState);
		return NewState;
	}
}

#if ENGINE_MAJOR_VERSION == 5
TSharedRef<FPlasticSourceControlChangelistState, ESPMode::ThreadSafe> FPlasticSourceControlProvider::GetStateInternal(const FPlasticSourceControlChangelist& InChangelist)
{
	TSharedRef<FPlasticSourceControlChangelistState, ESPMode::ThreadSafe>* State = ChangelistsStateCache.Find(InChangelist);
	if (State != NULL)
	{
		// found cached item
		return (*State);
	}
	else
	{
		// cache an unknown state for this item
		TSharedRef<FPlasticSourceControlChangelistState, ESPMode::ThreadSafe> NewState = MakeShared<FPlasticSourceControlChangelistState>(InChangelist);
		ChangelistsStateCache.Add(InChangelist, NewState);
		return NewState;
	}
}
#endif

FText FPlasticSourceControlProvider::GetStatusText() const
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("PlasticScmVersion"), FText::FromString(PlasticScmVersion.String));
	Args.Add(TEXT("PluginVersion"), FText::FromString(PluginVersion));
	Args.Add(TEXT("WorkspacePath"), FText::FromString(PathToWorkspaceRoot));
	Args.Add(TEXT("WorkspaceName"), FText::FromString(WorkspaceName));
	Args.Add(TEXT("BranchName"), FText::FromString(BranchName));
	// Detect special case for a partial checkout (CS:-1 in Gluon mode)!
	if (IsPartialWorkspace())
	{
		Args.Add(TEXT("ChangesetNumber"), FText::FromString(FString::Printf(TEXT("N/A  (Gluon partial workspace)"))));
	}
	else
	{
		Args.Add(TEXT("ChangesetNumber"), FText::FromString(FString::Printf(TEXT("%d  (regular full workspace)"), ChangesetNumber)));
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
ECommandResult::Type FPlasticSourceControlProvider::GetState(const TArray<FSourceControlChangelistRef>& InChangelists, TArray<FSourceControlChangelistStateRef>& OutState, EStateCacheUsage::Type InStateCacheUsage)
{
	if (!IsEnabled())
	{
		return ECommandResult::Failed;
	}

	if (InStateCacheUsage == EStateCacheUsage::ForceUpdate)
	{
		TSharedRef<class FUpdatePendingChangelistsStatus, ESPMode::ThreadSafe> UpdatePendingChangelistsOperation = ISourceControlOperation::Create<FUpdatePendingChangelistsStatus>();
		UpdatePendingChangelistsOperation->SetChangelistsToUpdate(InChangelists);

		ISourceControlProvider::Execute(UpdatePendingChangelistsOperation, EConcurrency::Synchronous);
	}

	for (FSourceControlChangelistRef Changelist : InChangelists)
	{
		FPlasticSourceControlChangelistRef PlasticChangelist = StaticCastSharedRef<FPlasticSourceControlChangelist>(Changelist);
		OutState.Add(GetStateInternal(PlasticChangelist.Get()));
	}

	return ECommandResult::Succeeded;
}

bool FPlasticSourceControlProvider::RemoveChangelistFromCache(const FPlasticSourceControlChangelist& Changelist)
{
	return ChangelistsStateCache.Remove(Changelist) > 0;
}

TArray<FSourceControlChangelistStateRef> FPlasticSourceControlProvider::GetCachedStateByPredicate(TFunctionRef<bool(const FSourceControlChangelistStateRef&)> Predicate) const
{
	TArray<FSourceControlChangelistStateRef> Result;
	for (const auto& CacheItem : ChangelistsStateCache)
	{
		FSourceControlChangelistStateRef State = CacheItem.Value;
		if (Predicate(State))
		{
			Result.Add(State);
		}
	}
	return Result;
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

#if ENGINE_MAJOR_VERSION == 5
	ECommandResult::Type FPlasticSourceControlProvider::Execute(const FSourceControlOperationRef& InOperation, FSourceControlChangelistPtr InChangelist, const TArray<FString>& InFiles, EConcurrency::Type InConcurrency, const FSourceControlOperationComplete& InOperationCompleteDelegate)
#else
	ECommandResult::Type FPlasticSourceControlProvider::Execute(const FSourceControlOperationRef& InOperation, const TArray<FString>& InFiles,	EConcurrency::Type InConcurrency, const FSourceControlOperationComplete& InOperationCompleteDelegate)
#endif
{
	if (!bWorkspaceFound && !(InOperation->GetName() == "Connect") && !(InOperation->GetName() == "MakeWorkspace"))
	{
		UE_LOG(LogSourceControl, Warning, TEXT("'%s': only Connect operation allowed without a workspace"), *InOperation->GetName().ToString());
		InOperationCompleteDelegate.ExecuteIfBound(InOperation, ECommandResult::Failed);
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

#if ENGINE_MAJOR_VERSION == 5
	TSharedPtr<FPlasticSourceControlChangelist, ESPMode::ThreadSafe> ChangelistPtr = StaticCastSharedPtr<FPlasticSourceControlChangelist>(InChangelist);
	Command->Changelist = ChangelistPtr ? ChangelistPtr.ToSharedRef().Get() : FPlasticSourceControlChangelist();
#endif

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

bool FPlasticSourceControlProvider::CanExecuteOperation(const FSourceControlOperationRef& InOperation) const
{
	return WorkersMap.Find(InOperation->GetName()) != nullptr;
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
	return bUsesLocalReadOnlyState;
}

bool FPlasticSourceControlProvider::UsesChangelists() const
{
	// We don't want to show ChangeList column anymore (Plastic SCM term would be ChangeSet) BUT we need this to display the changelists in the source control menu
	return true; // TODO: we should make this configurable, in order for users to be able to hide the View Changelists window from the menu
}

bool FPlasticSourceControlProvider::UsesUncontrolledChangelists() const
{
	return false; // TODO: not working yet; see for instance the Reconcile action when not using readonly flags
}

bool FPlasticSourceControlProvider::UsesCheckout() const
{
	return GetDefault<UPlasticSourceControlProjectSettings>()->bPromptForCheckoutOnChange;
}

bool FPlasticSourceControlProvider::UsesFileRevisions() const
{
	/* TODO: this API is broken, it is preventing the user to use the source control context menu, as well as selecting what files to submit

	// Only a partial workspace can sync files individually like Perforce, a regular workspace needs to update completely
	return IsPartialWorkspace();
	
	I believe that the logic is in fact flawed from what we would expect!
	IMO Context & selected check-in should be forbidden if the provider use changelists, not the reverse!
	(and using changelists could become a setting)

	NOTE: the bug was introduced in UE5.1 by:

Commit 5803c744 by marco anastasi, 10/04/2022 02:36 AM

Remove / disable 'Check-in' context menu item in Content Explorer and Scene Outliner for Source Control providers that do not use changelists

#rb stuart.hill, wouter.burgers
#preflight 633ac338c37844870ac69f67

[CL 22322177 by marco anastasi in ue5-main branch]

	*/
	
	return true;
}

bool FPlasticSourceControlProvider::AllowsDiffAgainstDepot() const
{
	return true;
}

TOptional<bool> FPlasticSourceControlProvider::IsAtLatestRevision() const
{
	return TOptional<bool>(); // NOTE: used by code in UE5's Status Bar but currently dormant as far as I can tell
}

TOptional<int> FPlasticSourceControlProvider::GetNumLocalChanges() const
{
	return TOptional<int>(); // NOTE: used by code in UE5's Status Bar but currently dormant as far as I can tell
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

		// only pop-up errors when running in full Editor, not in command line scripts
		if (!IsRunningCommandlet())
		{
			if (bPlasticAvailable)
			{
				if (PlasticScmVersion < PlasticSourceControlUtils::GetOldestSupportedPlasticScmVersion())
				{
					FFormatNamedArguments Args;
					Args.Add(TEXT("PlasticScmVersion"), FText::FromString(PlasticScmVersion.String));
					Args.Add(TEXT("OldestSupportedPlasticScmVersion"), FText::FromString(PlasticSourceControlUtils::GetOldestSupportedPlasticScmVersion().String));
					const FText UnsuportedVersionWarning = FText::Format(LOCTEXT("Plastic_UnsuportedVersion", "Plastic SCM {PlasticScmVersion} is not supported anymore by this plugin.\nPlastic SCM {OldestSupportedPlasticScmVersion} or a more recent version is required.\nPlease upgrade to the latest version."), Args);
					FMessageLog("SourceControl").Warning(UnsuportedVersionWarning);
					FMessageDialog::Open(EAppMsgType::Ok, UnsuportedVersionWarning);
				}
			}
			else if (InCommand.ErrorMessages.Num() > 0)
			{
				FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(InCommand.ErrorMessages[0]));
			}
		}

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
		if (bServerAvailable)
		{
			SetLastErrors(TArray<FString>());
		}
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
	TRACE_CPUPROFILER_EVENT_SCOPE(FPlasticSourceControlProvider::Tick);

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

			if (Command.Files.Num() > 1)
			{
				UE_LOG(LogSourceControl, Log, TEXT("%s of %d files processed in %.3lfs"), *Command.Operation->GetName().ToString(), Command.Files.Num(), (FPlatformTime::Seconds() - Command.StartTimestamp));
			}
			else if (Command.Files.Num() == 1)
			{
				UE_LOG(LogSourceControl, Log, TEXT("%s of %s processed in %.3lfs"), *Command.Operation->GetName().ToString(), *Command.Files[0], (FPlatformTime::Seconds() - Command.StartTimestamp));
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
		TRACE_CPUPROFILER_EVENT_SCOPE(FPlasticSourceControlProvider::Tick::BroadcastStateUpdate);
		OnSourceControlStateChanged.Broadcast();
	}
}

TArray<TSharedRef<ISourceControlLabel>> FPlasticSourceControlProvider::GetLabels(const FString& InMatchingSpec) const
{
	TArray< TSharedRef<ISourceControlLabel> > Tags;

	// NOTE list labels. Called by CrashDebugHelper() (to remote debug Engine crash)
	//					 and by SourceControlHelpers::AnnotateFile() (to add source file to report)
	// Reserved for internal use by Epic Games with Perforce only
	return Tags;
}

#if ENGINE_MAJOR_VERSION == 5
TArray<FSourceControlChangelistRef> FPlasticSourceControlProvider::GetChangelists(EStateCacheUsage::Type InStateCacheUsage)
{
	if (!IsEnabled())
	{
		return TArray<FSourceControlChangelistRef>();
	}

	if (InStateCacheUsage == EStateCacheUsage::ForceUpdate)
	{
		TSharedRef<class FUpdatePendingChangelistsStatus, ESPMode::ThreadSafe> UpdatePendingChangelistsOperation = ISourceControlOperation::Create<FUpdatePendingChangelistsStatus>();
		UpdatePendingChangelistsOperation->SetUpdateAllChangelists(true);

		ISourceControlProvider::Execute(UpdatePendingChangelistsOperation, EConcurrency::Synchronous);
	}

	TArray<FSourceControlChangelistRef> Changelists;
	Algo::Transform(ChangelistsStateCache, Changelists, [](const auto& Pair) { return MakeShared<FPlasticSourceControlChangelist, ESPMode::ThreadSafe>(Pair.Key); });
	return Changelists;
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
	TRACE_CPUPROFILER_EVENT_SCOPE(FPlasticSourceControlProvider::ExecuteSynchronousCommand);

	ECommandResult::Type Result = ECommandResult::Failed;

	// Display the progress dialog if a string was provided
	{
		FScopedSourceControlProgress Progress(Task);

		// Issue the command asynchronously...
		IssueCommand(InCommand);

		// ... then wait for its completion (thus making it synchronous)
#if ENGINE_MAJOR_VERSION == 4 || ENGINE_MINOR_VERSION < 1
		double LastProgressTimestamp = FPlatformTime::Seconds();
		double ProgressUpdateThreshold = .0;
#endif
		while (!InCommand.bExecuteProcessed)
		{
			// Tick the command queue and update progress.
			Tick();

#if ENGINE_MAJOR_VERSION == 4 || ENGINE_MINOR_VERSION < 1
			const double CurrentTimestamp = FPlatformTime::Seconds();
			const double ElapsedTime = CurrentTimestamp - LastProgressTimestamp;

			// Note: calling too many times Progress.Tick() crashes the GPU Out of Memory
			// We need to reduce the number of calls we make, but we don't want to have the progress bar stuttering horribly
			// So we tart to update it frequently/smoothly, and then we increase the intervals more and more (arithmetic series, with a cap)
			// in order to reduce the video memory usage for very long operation without visual penalty on quicker daily operations.
			if (ElapsedTime > ProgressUpdateThreshold)
			{
#endif
				Progress.Tick();

#if ENGINE_MAJOR_VERSION == 4 || ENGINE_MINOR_VERSION < 1
				LastProgressTimestamp = CurrentTimestamp;
				if (ProgressUpdateThreshold < 0.25)
				{
					ProgressUpdateThreshold += 0.001;
				}
			}
#endif

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
			// TODO If the command failed, inform the user that they need to try again (see Perforce, but they suppressed it!) Add a project settings for that!
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

