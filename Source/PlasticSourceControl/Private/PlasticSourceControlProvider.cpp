// Copyright (c) 2016 Codice Software - Sebastien Rombauts (sebastien.rombauts@gmail.com)

#include "PlasticSourceControlPrivatePCH.h"
#include "PlasticSourceControlProvider.h"
#include "PlasticSourceControlCommand.h"
#include "ISourceControlModule.h"
#include "PlasticSourceControlModule.h"
#include "PlasticSourceControlSettings.h"
#include "PlasticSourceControlOperations.h"
#include "PlasticSourceControlUtils.h"
#include "SPlasticSourceControlSettings.h"
#include "MessageLog.h"
#include "ScopedSourceControlProgress.h"

#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "Misc/QueuedThreadPool.h"

#define LOCTEXT_NAMESPACE "PlasticSourceControl"

static FName ProviderName("Plastic SCM");

void FPlasticSourceControlProvider::Init(bool bForceConnection)
{
	// TODO NOCOMMIT
	UE_LOG(LogSourceControl, Warning, TEXT("FPlasticSourceControlProvider::Init(%d)"), bForceConnection);
	CheckPlasticAvailability(bForceConnection);
}

void FPlasticSourceControlProvider::CheckPlasticAvailability(bool bForceConnection)
{
	FPlasticSourceControlModule& PlasticSourceControl = FModuleManager::LoadModuleChecked<FPlasticSourceControlModule>("PlasticSourceControl");
	const FString& PathToPlasticBinary = PlasticSourceControl.AccessSettings().GetBinaryPath();

	// Find the path to the root Plastic directory (if any, else uses the GameDir)
	const FString PathToGameDir = FPaths::ConvertRelativePathToFull(FPaths::GameDir());
	bWorkspaceFound = PlasticSourceControlUtils::FindRootDirectory(PathToGameDir, PathToWorkspaceRoot);

	// Launch the Plastic SCM cli shell on the background to issue all commands during this session
	bPlasticAvailable = PlasticSourceControlUtils::LaunchBackgroundPlasticShell(PathToPlasticBinary, PathToWorkspaceRoot);
	if(bPlasticAvailable)
	{
		FString PlasticScmVersion;
		PlasticSourceControlUtils::GetPlasticScmVersion(PlasticScmVersion);

		// Get user name (from the global Plastic SCM client config)
		PlasticSourceControlUtils::GetUserName(UserName);

		if(!bWorkspaceFound)
		{
			UE_LOG(LogSourceControl, Warning, TEXT("'%s' is not part of a Plastic workspace"), *FPaths::GameDir());
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

	bServerAvailable = false;
}

TSharedRef<FPlasticSourceControlState, ESPMode::ThreadSafe> FPlasticSourceControlProvider::GetStateInternal(const FString& Filename)
{
	TSharedRef<FPlasticSourceControlState, ESPMode::ThreadSafe>* State = StateCache.Find(Filename);
	if(State != NULL)
	{
		// found cached item
		return (*State);
	}
	else
	{
		// cache an unknown state for this item
		TSharedRef<FPlasticSourceControlState, ESPMode::ThreadSafe> NewState = MakeShareable( new FPlasticSourceControlState(Filename) );
		StateCache.Add(Filename, NewState);
		return NewState;
	}
}

FText FPlasticSourceControlProvider::GetStatusText() const
{
	FFormatNamedArguments Args;
	Args.Add( TEXT("WorkspacePath"), FText::FromString(PathToWorkspaceRoot) );
	Args.Add( TEXT("WorkspaceName"), FText::FromString(WorkspaceName) );
	Args.Add( TEXT("BranchName"), FText::FromString(BranchName) );
	Args.Add( TEXT("ChangesetNumber"), FText::FromString(FString::Printf(TEXT("%d"), ChangesetNumber)) );
	Args.Add( TEXT("UserName"), FText::FromString(UserName) );

	return FText::Format( NSLOCTEXT("Status", "Provider: Plastic\nEnabledLabel", "Workspace: {WorkspaceName} ({WorkspacePath})\n{BranchName}\nChangeset: {ChangesetNumber}\nUser: {UserName}"), Args );
}

/** Quick check if source control is enabled */
bool FPlasticSourceControlProvider::IsEnabled() const
{
	return bWorkspaceFound;
}

/** Quick check if source control is available for use (useful for server-based providers) */
bool FPlasticSourceControlProvider::IsAvailable() const
{
	return bServerAvailable;
}

const FName& FPlasticSourceControlProvider::GetName(void) const
{
	return ProviderName;
}

ECommandResult::Type FPlasticSourceControlProvider::GetState( const TArray<FString>& InFiles, TArray< TSharedRef<ISourceControlState, ESPMode::ThreadSafe> >& OutState, EStateCacheUsage::Type InStateCacheUsage )
{
	if(!IsEnabled())
	{
		return ECommandResult::Failed;
	}

	TArray<FString> AbsoluteFiles = SourceControlHelpers::AbsoluteFilenames(InFiles);

	if(InStateCacheUsage == EStateCacheUsage::ForceUpdate)
	{
		UE_LOG(LogSourceControl, Log, TEXT("GetState: ForceUpdate"));
		Execute(ISourceControlOperation::Create<FUpdateStatus>(), AbsoluteFiles);
	}

	for(const auto& AbsoluteFile : AbsoluteFiles)
	{
		OutState.Add(GetStateInternal(*AbsoluteFile));
	}

	return ECommandResult::Succeeded;
}

TArray<FSourceControlStateRef> FPlasticSourceControlProvider::GetCachedStateByPredicate(TFunctionRef<bool(const FSourceControlStateRef&)> Predicate) const
{
	TArray<FSourceControlStateRef> Result;
	for(const auto& CacheItem : StateCache)
	{
		FSourceControlStateRef State = CacheItem.Value;
		if(Predicate(State))
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

FDelegateHandle FPlasticSourceControlProvider::RegisterSourceControlStateChanged_Handle( const FSourceControlStateChanged::FDelegate& SourceControlStateChanged )
{
	return OnSourceControlStateChanged.Add( SourceControlStateChanged );
}

void FPlasticSourceControlProvider::UnregisterSourceControlStateChanged_Handle( FDelegateHandle Handle )
{
	OnSourceControlStateChanged.Remove( Handle );
}

ECommandResult::Type FPlasticSourceControlProvider::Execute( const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation, const TArray<FString>& InFiles, EConcurrency::Type InConcurrency, const FSourceControlOperationComplete& InOperationCompleteDelegate )
{
	if(!bWorkspaceFound && !(InOperation->GetName() == "Connect"))
	{
		UE_LOG(LogSourceControl, Warning, TEXT("'%s': only Connect operation allowed whithout a workspace"), *InOperation->GetName().ToString());
		return ECommandResult::Failed;
	}

	TArray<FString> AbsoluteFiles = SourceControlHelpers::AbsoluteFilenames(InFiles);

	// Query to see if we allow this operation
	TSharedPtr<IPlasticSourceControlWorker, ESPMode::ThreadSafe> Worker = CreateWorker(InOperation->GetName());
	if(!Worker.IsValid())
	{
		// this operation is unsupported by this source control provider
		FFormatNamedArguments Arguments;
		Arguments.Add( TEXT("OperationName"), FText::FromName(InOperation->GetName()) );
		Arguments.Add( TEXT("ProviderName"), FText::FromName(GetName()) );
		FMessageLog("SourceControl").Error(FText::Format(LOCTEXT("UnsupportedOperation", "Operation '{OperationName}' not supported by source control provider '{ProviderName}'"), Arguments));
		return ECommandResult::Failed;
	}

	FPlasticSourceControlCommand* Command = new FPlasticSourceControlCommand(InOperation, Worker.ToSharedRef());
	Command->Files = AbsoluteFiles;
	Command->OperationCompleteDelegate = InOperationCompleteDelegate;

	// fire off operation
	if(InConcurrency == EConcurrency::Synchronous)
	{
		Command->bAutoDelete = false;
		return ExecuteSynchronousCommand(*Command, InOperation->GetInProgressString());
	}
	else
	{
		Command->bAutoDelete = true;
		return IssueCommand(*Command);
	}
}

bool FPlasticSourceControlProvider::CanCancelOperation( const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation ) const
{
	return false;
}

void FPlasticSourceControlProvider::CancelOperation( const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation )
{
}

bool FPlasticSourceControlProvider::UsesLocalReadOnlyState() const
{
	return false; // TODO: use configuration
}

bool FPlasticSourceControlProvider::UsesChangelists() const
{
	return true;
}

TSharedPtr<IPlasticSourceControlWorker, ESPMode::ThreadSafe> FPlasticSourceControlProvider::CreateWorker(const FName& InOperationName) const
{
	const FGetPlasticSourceControlWorker* Operation = WorkersMap.Find(InOperationName);
	if(Operation != nullptr)
	{
		return Operation->Execute();
	}
		
	return nullptr;
}

void FPlasticSourceControlProvider::RegisterWorker( const FName& InName, const FGetPlasticSourceControlWorker& InDelegate )
{
	WorkersMap.Add( InName, InDelegate );
}

void FPlasticSourceControlProvider::OutputCommandMessages(const FPlasticSourceControlCommand& InCommand) const
{
	FMessageLog SourceControlLog("SourceControl");

	for(int32 ErrorIndex = 0; ErrorIndex < InCommand.ErrorMessages.Num(); ++ErrorIndex)
	{
		SourceControlLog.Error(FText::FromString(InCommand.ErrorMessages[ErrorIndex]));
	}

	for(int32 InfoIndex = 0; InfoIndex < InCommand.InfoMessages.Num(); ++InfoIndex)
	{
		SourceControlLog.Info(FText::FromString(InCommand.InfoMessages[InfoIndex]));
	}
}

void FPlasticSourceControlProvider::UpdateWorkspaceStatus(const class FPlasticSourceControlCommand& InCommand)
{
	if (InCommand.Operation->GetName() == "Connect")
	{
		// Is connection successful?
		bServerAvailable = InCommand.bCommandSuccessful;
		bWorkspaceFound = (InCommand.WorkspaceName.Len() > 0);

		WorkspaceName = InCommand.WorkspaceName;
		RepositoryName = InCommand.RepositoryName;
		ServerUrl = InCommand.ServerUrl;

		if (bWorkspaceFound)
		{
			// Extend the "Source Control" menu in the Editor Toolbar on each successfull connection
			PlasticSourceControlMenu.Unregister(); // cleanup for any previous connection
			PlasticSourceControlMenu.Register();
		}
	}
	else if (InCommand.bConnectionDropped)
	{
		// checkconnection failed on UpdateStatus
		bServerAvailable = false;
	}

	// And for all operations running UpdateStatus, get Changeset and Branch informations:
	if (InCommand.ChangesetNumber > -1)
	{
		ChangesetNumber = InCommand.ChangesetNumber;
		BranchName = InCommand.BranchName;
	}
}

void FPlasticSourceControlProvider::Tick()
{	
	bool bStatesUpdated = false;
	for(int32 CommandIndex = 0; CommandIndex < CommandQueue.Num(); ++CommandIndex)
	{
		FPlasticSourceControlCommand& Command = *CommandQueue[CommandIndex];
		if(Command.bExecuteProcessed)
		{
			// Remove command from the queue
			CommandQueue.RemoveAt(CommandIndex);

			// Update workspace status and connection state on Connect and UpdateStatus operations
			UpdateWorkspaceStatus(Command);

			// let command update the states of any files
			bStatesUpdated |= Command.Worker->UpdateStates();

			// dump any messages to output log
			OutputCommandMessages(Command);

			// run the completion delegate callback if we have one bound
			ECommandResult::Type Result = Command.bCommandSuccessful ? ECommandResult::Succeeded : ECommandResult::Failed;
			Command.OperationCompleteDelegate.ExecuteIfBound(Command.Operation, Result);

			// commands that are left in the array during a tick need to be deleted
			if(Command.bAutoDelete)
			{
				// Only delete commands that are not running 'synchronously'
				delete &Command;
			}
			
			// only do one command per tick loop, as we dont want concurrent modification 
			// of the command queue (which can happen in the completion delegate)
			break;
		}
	}

	if(bStatesUpdated)
	{
		OnSourceControlStateChanged.Broadcast();
	}
}

TArray< TSharedRef<ISourceControlLabel> > FPlasticSourceControlProvider::GetLabels( const FString& InMatchingSpec ) const
{
	TArray< TSharedRef<ISourceControlLabel> > Tags;

	// NOTE list labels. Called by CrashDebugHelper() (to remote debug Engine crash)
	//					 and by SourceControlHelpers::AnnotateFile() (to add source file to report)
	// Reserved for internal use by Epic Games with Perforce only
	return Tags;
}

#if SOURCE_CONTROL_WITH_SLATE
TSharedRef<class SWidget> FPlasticSourceControlProvider::MakeSettingsWidget() const
{
	return SNew(SPlasticSourceControlSettings);
}
#endif

ECommandResult::Type FPlasticSourceControlProvider::ExecuteSynchronousCommand(FPlasticSourceControlCommand& InCommand, const FText& Task)
{
	ECommandResult::Type Result = ECommandResult::Failed;

	UE_LOG(LogSourceControl, Log, TEXT("ExecuteSynchronousCommand: %s"), *InCommand.Operation->GetName().ToString());

	// Display the progress dialog if a string was provided
	{
		FScopedSourceControlProgress Progress(Task);

		// Issue the command asynchronously...
		IssueCommand( InCommand );

		// ... then wait for its completion (thus making it synchronous)
		while(!InCommand.bExecuteProcessed)
		{
			// Tick the command queue and update progress.
			Tick();
			
			Progress.Tick();

			// Sleep for a bit so we don't busy-wait so much.
			FPlatformProcess::Sleep(0.01f);
		}
	
		// always do one more Tick() to make sure the command queue is cleaned up.
		Tick();

		if(InCommand.bCommandSuccessful)
		{
			Result = ECommandResult::Succeeded;
		}
		else
		{
			UE_LOG(LogSourceControl, Error, TEXT("Command '%s' Failed!"), *InCommand.Operation->GetName().ToString());
		}
	}

	// Delete the command now (asynchronous commands are deleted in the Tick() method)
	check(!InCommand.bAutoDelete);

	// ensure commands that are not auto deleted do not end up in the command queue
	if ( CommandQueue.Contains( &InCommand ) ) 
	{
		CommandQueue.Remove( &InCommand );
	}
	delete &InCommand;

	return Result;
}

ECommandResult::Type FPlasticSourceControlProvider::IssueCommand(FPlasticSourceControlCommand& InCommand)
{
	UE_LOG(LogSourceControl, Log, TEXT("IssueCommand: %s"), *InCommand.Operation->GetName().ToString());

	if(GThreadPool != nullptr)
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
