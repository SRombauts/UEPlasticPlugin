// Copyright (c) 2016-2022 Codice Software

#include "PlasticSourceControlOperations.h"
#include "PlasticSourceControlSettings.h"
#include "PlasticSourceControlState.h"
#include "PlasticSourceControlCommand.h"
#include "PlasticSourceControlModule.h"
#include "PlasticSourceControlUtils.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Async/Async.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "SourceControlOperations.h"
#include "ISourceControlModule.h"

#define LOCTEXT_NAMESPACE "PlasticSourceControl"

FName FPlasticRevertUnchanged::GetName() const
{
	return "RevertUnchanged";
}

FText FPlasticRevertUnchanged::GetInProgressString() const
{
	return LOCTEXT("SourceControl_RevertUnchanged", "Reverting unchanged file(s) in Source Control...");
}

FName FPlasticRevertAll::GetName() const
{
	return "RevertAll";
}

FText FPlasticRevertAll::GetInProgressString() const
{
	return LOCTEXT("SourceControl_RevertAll", "Reverting checked-out file(s) in Source Control...");
}

FName FPlasticMakeWorkspace::GetName() const
{
	return "MakeWorkspace";
}

FText FPlasticMakeWorkspace::GetInProgressString() const
{
	return LOCTEXT("SourceControl_MakeWorkspace", "Create a new Repository and initialize the Workspace");
}

static bool AreAllFiles(const TArray<FString>& InFiles)
{
	for (const FString& File : InFiles)
	{
		if (File.IsEmpty() || File[File.Len() - 1] == TEXT('/'))
			return false;
	}
	return true;
}


FName FPlasticConnectWorker::GetName() const
{
	return "Connect";
}

bool FPlasticConnectWorker::Execute(FPlasticSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());
	TSharedRef<FConnect, ESPMode::ThreadSafe> Operation = StaticCastSharedRef<FConnect>(InCommand.Operation);

	const FPlasticSourceControlModule& PlasticSourceControl = FModuleManager::GetModuleChecked<FPlasticSourceControlModule>("PlasticSourceControl");
	if (PlasticSourceControl.GetProvider().IsPlasticAvailable())
	{
		// Get workspace name
		InCommand.bCommandSuccessful = PlasticSourceControlUtils::GetWorkspaceName(InCommand.WorkspaceName);
		if (InCommand.bCommandSuccessful)
		{
			// Get repository, server Url, branch and current changeset number
			InCommand.bCommandSuccessful = PlasticSourceControlUtils::GetWorkspaceInformation(InCommand.ChangesetNumber, InCommand.RepositoryName, InCommand.ServerUrl, InCommand.BranchName);
			if (InCommand.bCommandSuccessful)
			{
				// Execute a 'checkconnection' command to check the connectivity of the server.
				InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("checkconnection"), TArray<FString>(), TArray<FString>(), InCommand.Concurrency, InCommand.InfoMessages, InCommand.ErrorMessages);
				if (InCommand.bCommandSuccessful)
				{
					// Now update the status of assets in the Content directory
					// but only on real (re-)connection (but not each time Login() is called by Rename or Fixup Redirector command to check connection)
					// and only if enabled in the settings
					if (!PlasticSourceControl.GetProvider().IsAvailable() && PlasticSourceControl.AccessSettings().GetUpdateStatusAtStartup())
					{
						TArray<FString> ContentDir;
						ContentDir.Add(FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir()));
						PlasticSourceControlUtils::RunUpdateStatus(ContentDir, false, InCommand.Concurrency, InCommand.ErrorMessages, States, InCommand.ChangesetNumber, InCommand.BranchName);
					}
				}
				else
				{
					Operation->SetErrorText(FText::FromString(InCommand.ErrorMessages[0]));
				}
			}
		}
		else
		{
			Operation->SetErrorText(LOCTEXT("NotAPlasticRepository", "Failed to enable Plastic SCM source control. You need to initialize the project as a Plastic SCM repository first."));
		}
	}
	else
	{
		Operation->SetErrorText(LOCTEXT("PlasticScmCliUnavaillable", "Failed to launch Plastic SCM command line tool. You need to install it or set the correct path to it first."));
	}

	return InCommand.bCommandSuccessful;
}

bool FPlasticConnectWorker::UpdateStates()
{
	return PlasticSourceControlUtils::UpdateCachedStates(MoveTemp(States));
}

FName FPlasticCheckOutWorker::GetName() const
{
	return "CheckOut";
}

bool FPlasticCheckOutWorker::Execute(FPlasticSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	// Detect special case for a partial checkout (CS:-1 in Gluon mode)!
	if (-1 != InCommand.ChangesetNumber)
	{
		InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("checkout"), TArray<FString>(), InCommand.Files, InCommand.Concurrency, InCommand.InfoMessages, InCommand.ErrorMessages);
	}
	else
	{
		InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("partial checkout"), TArray<FString>(), InCommand.Files, InCommand.Concurrency, InCommand.InfoMessages, InCommand.ErrorMessages);
	}

	// now update the status of our files
	PlasticSourceControlUtils::RunUpdateStatus(InCommand.Files, false, InCommand.Concurrency, InCommand.ErrorMessages, States, InCommand.ChangesetNumber, InCommand.BranchName);

	return InCommand.bCommandSuccessful;
}

bool FPlasticCheckOutWorker::UpdateStates()
{
	return PlasticSourceControlUtils::UpdateCachedStates(MoveTemp(States));
}

/// Parse check-in result, usually locking like "Created changeset cs:8@br:/main@MyProject@SRombauts@cloud (mount:'/')"
static FText ParseCheckInResults(const TArray<FString>& InResults)
{
	if (InResults.Num() > 0)
	{
		static const FString ChangesetPrefix(TEXT("Created changeset "));
		if (InResults.Last().StartsWith(ChangesetPrefix))
		{
			FString ChangesetString;
			static const FString BranchPrefix(TEXT("@br:"));
			const int32 BranchIndex = InResults.Last().Find(BranchPrefix, ESearchCase::CaseSensitive);
			if (BranchIndex > INDEX_NONE)
			{
				ChangesetString = InResults.Last().Mid(ChangesetPrefix.Len(), BranchIndex - ChangesetPrefix.Len());
			}
			return FText::Format(LOCTEXT("SubmitMessage", "Submitted changeset {0}"), FText::FromString(ChangesetString));
		}
		else
		{
			return FText::FromString(InResults.Last());
		}
	}
	return FText();
}

FName FPlasticCheckInWorker::GetName() const
{
	return "CheckIn";
}

bool FPlasticCheckInWorker::Execute(FPlasticSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());
	TSharedRef<FCheckIn, ESPMode::ThreadSafe> Operation = StaticCastSharedRef<FCheckIn>(InCommand.Operation);

	UE_LOG(LogSourceControl, Verbose, TEXT("CheckIn: %d file(s) Description: '%s'"), InCommand.Files.Num(), *Operation->GetDescription().ToString());

	if (InCommand.Files.Num() > 0)
	{
		// make a temp file to place our commit message in
		FScopedTempFile CommitMsgFile(Operation->GetDescription());
		if (!CommitMsgFile.GetFilename().IsEmpty())
		{
			TArray<FString> Parameters;
			FString ParamCommitMsgFilename = TEXT("--commentsfile=\"");
			ParamCommitMsgFilename += FPaths::ConvertRelativePathToFull(CommitMsgFile.GetFilename());
			ParamCommitMsgFilename += TEXT("\"");
			Parameters.Add(ParamCommitMsgFilename);
			// Detect special case for a partial checkout (CS:-1 in Gluon mode)!
			if (-1 != InCommand.ChangesetNumber)
			{
				Parameters.Add(TEXT("--all")); // Also files Changed (not CheckedOut) and Moved/Deleted Locally
			//  NOTE: --update added as #23 but removed as #32 because most assets are locked by the Unreal Editor
			//	Parameters.Add(TEXT("--update")); // Processes the update-merge automatically if it eventually happens.
				InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("checkin"), Parameters, InCommand.Files, InCommand.Concurrency, InCommand.InfoMessages, InCommand.ErrorMessages);
			}
			else
			{
				Parameters.Add(TEXT("--applychanged")); // Also files Changed (not CheckedOut) and Moved/Deleted Locally
				InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("partial checkin"), Parameters, InCommand.Files, InCommand.Concurrency, InCommand.InfoMessages, InCommand.ErrorMessages);
			}
			if (InCommand.bCommandSuccessful)
			{
				// Remove any deleted files from status cache
				FPlasticSourceControlModule& PlasticSourceControl = FModuleManager::GetModuleChecked<FPlasticSourceControlModule>("PlasticSourceControl");
				FPlasticSourceControlProvider& Provider = PlasticSourceControl.GetProvider();

				TArray<TSharedRef<ISourceControlState, ESPMode::ThreadSafe>> LocalStates;
				Provider.GetState(InCommand.Files, LocalStates, EStateCacheUsage::Use);
				for (const auto& State : LocalStates)
				{
					if (State->IsDeleted())
					{
						Provider.RemoveFileFromCache(State->GetFilename());
					}
				}

				Operation->SetSuccessMessage(ParseCheckInResults(InCommand.InfoMessages));
				UE_LOG(LogSourceControl, Log, TEXT("CheckIn successful"));
			}
		}

		// now update the status of our files
		PlasticSourceControlUtils::RunUpdateStatus(InCommand.Files, false, InCommand.Concurrency, InCommand.ErrorMessages, States, InCommand.ChangesetNumber, InCommand.BranchName);
	}
	else
	{

	}

	return InCommand.bCommandSuccessful;
}

bool FPlasticCheckInWorker::UpdateStates()
{
	return PlasticSourceControlUtils::UpdateCachedStates(MoveTemp(States));
}

FName FPlasticMarkForAddWorker::GetName() const
{
	return "MarkForAdd";
}

bool FPlasticMarkForAddWorker::Execute(FPlasticSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());
	if (InCommand.Files.Num() > 0)
	{

		TArray<FString> Parameters;
		Parameters.Add(TEXT("--parents")); // NOTE: deprecated in 8.0.16.3100 when it became the default https://www.plasticscm.com/download/releasenotes/8.0.16.3100
		// Note: using "?" is a workaround to trigger the Plastic's "SkipIgnored" internal flag meaning "don't add file that are ignored":
		//			options.SkipIgnored = cla.GetWildCardArguments().Count > 0;
		//		 It's behavior is similar as Subversion:
		//  		if you explicitely add one file that is ignored, "cm" will happily accept it and add it,
		//			if you try to add a set of files with a pattern, "cm" will skip the files that are ignored and only add the other ones
		// TODO: provide an updated version of "cm" with a new flag like --applyignorerules
		if (AreAllFiles(InCommand.Files))
		{
			Parameters.Add(TEXT("?"));	// needed only when used with a list of files
		}
		else
		{
			Parameters.Add(TEXT("-R"));	// needed only at the time of workspace creation, to add directories recursively
		}
		// Detect special case for a partial checkout (CS:-1 in Gluon mode)!
		if (-1 != InCommand.ChangesetNumber)
		{
			InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("add"), Parameters, InCommand.Files, InCommand.Concurrency, InCommand.InfoMessages, InCommand.ErrorMessages);
		}
		else
		{
			InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("partial add"), Parameters, InCommand.Files, InCommand.Concurrency, InCommand.InfoMessages, InCommand.ErrorMessages);
		}

		// now update the status of our files
		PlasticSourceControlUtils::RunUpdateStatus(InCommand.Files, false, InCommand.Concurrency, InCommand.ErrorMessages, States, InCommand.ChangesetNumber, InCommand.BranchName);
	}
	else
	{
		UE_LOG(LogSourceControl, Warning, TEXT("Checkin operation without files"));
	}

	return InCommand.bCommandSuccessful;
}

bool FPlasticMarkForAddWorker::UpdateStates()
{
	return PlasticSourceControlUtils::UpdateCachedStates(MoveTemp(States));
}

FName FPlasticDeleteWorker::GetName() const
{
	return "Delete";
}

bool FPlasticDeleteWorker::Execute(FPlasticSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	// Detect special case for a partial checkout (CS:-1 in Gluon mode)!
	if (-1 != InCommand.ChangesetNumber)
	{
		InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("remove"), TArray<FString>(), InCommand.Files, InCommand.Concurrency, InCommand.InfoMessages, InCommand.ErrorMessages);
	}
	else
	{
		InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("partial remove"), TArray<FString>(), InCommand.Files, InCommand.Concurrency, InCommand.InfoMessages, InCommand.ErrorMessages);
	}

	// now update the status of our files
	PlasticSourceControlUtils::RunUpdateStatus(InCommand.Files, false, InCommand.Concurrency, InCommand.ErrorMessages, States, InCommand.ChangesetNumber, InCommand.BranchName);

	return InCommand.bCommandSuccessful;
}

bool FPlasticDeleteWorker::UpdateStates()
{
	return PlasticSourceControlUtils::UpdateCachedStates(MoveTemp(States));
}

FName FPlasticRevertWorker::GetName() const
{
	return "Revert";
}

bool FPlasticRevertWorker::Execute(FPlasticSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	FPlasticSourceControlModule& PlasticSourceControl = FModuleManager::GetModuleChecked<FPlasticSourceControlModule>("PlasticSourceControl");
	FPlasticSourceControlProvider& Provider = PlasticSourceControl.GetProvider();

	TArray<FString> ChangedFiles;
	TArray<FString> CheckedOutFiles;

	for (const FString& File : InCommand.Files)
	{
		TSharedRef<FPlasticSourceControlState, ESPMode::ThreadSafe> State = Provider.GetStateInternal(File);

		if (EWorkspaceState::Changed == State->WorkspaceState)
		{
			// only revert the changes of the given file in workspace
			ChangedFiles.Add(State->LocalFilename);
		}
		else
		{
			CheckedOutFiles.Add(State->LocalFilename);
			// in case of a Moved/Renamed, find the rename origin to revert both at once
			if (EWorkspaceState::Moved == State->WorkspaceState)
			{
				CheckedOutFiles.Add(State->MovedFrom);

				// Delete the redirector
				IFileManager::Get().Delete(*State->MovedFrom);
			}
		}
	}

	InCommand.bCommandSuccessful = true;

	if (ChangedFiles.Num() > 0)
	{
		InCommand.bCommandSuccessful &= PlasticSourceControlUtils::RunCommand(TEXT("undochange"), TArray<FString>(), ChangedFiles, InCommand.Concurrency, InCommand.InfoMessages, InCommand.ErrorMessages);
	}

	if (CheckedOutFiles.Num() > 0)
	{
		// revert the checkout and any changes of the given file in workspace
		// Detect special case for a partial checkout (CS:-1 in Gluon mode)!
		if (-1 != InCommand.ChangesetNumber)
		{
			InCommand.bCommandSuccessful &= PlasticSourceControlUtils::RunCommand(TEXT("undocheckout"), TArray<FString>(), CheckedOutFiles, InCommand.Concurrency, InCommand.InfoMessages, InCommand.ErrorMessages);
		}
		else
		{
			InCommand.bCommandSuccessful &= PlasticSourceControlUtils::RunCommand(TEXT("partial undocheckout"), TArray<FString>(), CheckedOutFiles, InCommand.Concurrency, InCommand.InfoMessages, InCommand.ErrorMessages);
		}
	}

	// NOTE: optim, no need to update the status of our files since this is done immediately after by the Editor

	return InCommand.bCommandSuccessful;
}

bool FPlasticRevertWorker::UpdateStates()
{
	return false;
}

FName FPlasticRevertUnchangedWorker::GetName() const
{
	return "RevertUnchanged";
}

bool FPlasticRevertUnchangedWorker::Execute(FPlasticSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	TArray<FString> Parameters;
	Parameters.Add(TEXT("-R"));

	// revert the checkout of all unchanged files recursively
	InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("uncounchanged"), Parameters, InCommand.Files, InCommand.Concurrency, InCommand.InfoMessages, InCommand.ErrorMessages);

	// Now update the status of assets in the Content directory
	TArray<FString> ContentDir;
	ContentDir.Add(FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir()));
	PlasticSourceControlUtils::RunUpdateStatus(ContentDir, false, InCommand.Concurrency, InCommand.ErrorMessages, States, InCommand.ChangesetNumber, InCommand.BranchName);

	return InCommand.bCommandSuccessful;
}

bool FPlasticRevertUnchangedWorker::UpdateStates()
{
	return PlasticSourceControlUtils::UpdateCachedStates(MoveTemp(States));
}

FName FPlasticRevertAllWorker::GetName() const
{
	return "RevertAll";
}

bool FPlasticRevertAllWorker::Execute(FPlasticSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	TArray<FString> Parameters;
	Parameters.Add(TEXT("--all"));
	// revert the checkout of all files recursively
	// Detect special case for a partial checkout (CS:-1 in Gluon mode)!
	if (-1 != InCommand.ChangesetNumber)
	{
		InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("undocheckout"), Parameters, InCommand.Files, InCommand.Concurrency, InCommand.InfoMessages, InCommand.ErrorMessages);
	}
	else
	{
		InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("partial undocheckout"), Parameters, InCommand.Files, InCommand.Concurrency, InCommand.InfoMessages, InCommand.ErrorMessages);
	}

	// Now update the status of assets in the Content directory
	TArray<FString> ContentDir;
	ContentDir.Add(FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir()));
	PlasticSourceControlUtils::RunUpdateStatus(ContentDir, false, InCommand.Concurrency, InCommand.ErrorMessages, States, InCommand.ChangesetNumber, InCommand.BranchName);

	return InCommand.bCommandSuccessful;
}

bool FPlasticRevertAllWorker::UpdateStates()
{
	return PlasticSourceControlUtils::UpdateCachedStates(MoveTemp(States));
}

FName FPlasticMakeWorkspaceWorker::GetName() const
{
	return "MakeWorkspace";
}

bool FPlasticMakeWorkspaceWorker::Execute(FPlasticSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());
	TSharedRef<FPlasticMakeWorkspace, ESPMode::ThreadSafe> Operation = StaticCastSharedRef<FPlasticMakeWorkspace>(InCommand.Operation);

	{
		TArray<FString> Parameters;
		Parameters.Add(Operation->ServerUrl);
		Parameters.Add(Operation->RepositoryName);
		PlasticSourceControlUtils::RunCommand(TEXT("makerepository"), Parameters, TArray<FString>(), EConcurrency::Synchronous, InCommand.InfoMessages, InCommand.ErrorMessages);
	}
	{
		TArray<FString> Parameters;
		Parameters.Add(Operation->WorkspaceName);
		Parameters.Add(TEXT(".")); // current path, ie. ProjectDir
		Parameters.Add(FString::Printf(TEXT("--repository=rep:%s@repserver:%s"), *Operation->RepositoryName, *Operation->ServerUrl));
		InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("makeworkspace"), Parameters, TArray<FString>(), EConcurrency::Synchronous, InCommand.InfoMessages, InCommand.ErrorMessages);
	}

	return InCommand.bCommandSuccessful;
}

bool FPlasticMakeWorkspaceWorker::UpdateStates()
{
	return false;
}

FName FPlasticUpdateStatusWorker::GetName() const
{
	return "UpdateStatus";
}

bool FPlasticUpdateStatusWorker::Execute(FPlasticSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());
	TSharedRef<FUpdateStatus, ESPMode::ThreadSafe> Operation = StaticCastSharedRef<FUpdateStatus>(InCommand.Operation);

	// Note: ShouldCheckAllFiles is never set to true (SetCheckingAllFiles)
	UE_LOG(LogSourceControl, Log, TEXT("status (of %d files, ShouldUpdateHistory=%d, ShouldGetOpenedOnly=%d, ShouldUpdateModifiedState=%d)"),
		InCommand.Files.Num(), Operation->ShouldUpdateHistory(), Operation->ShouldGetOpenedOnly(), Operation->ShouldUpdateModifiedState());

	if (InCommand.Files.Num() > 0)
	{
		InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunUpdateStatus(InCommand.Files, Operation->ShouldUpdateHistory(), InCommand.Concurrency, InCommand.ErrorMessages, States, InCommand.ChangesetNumber, InCommand.BranchName);
		// Remove all "is not in a workspace" error and convert the result to "success" if there are no other errors
		PlasticSourceControlUtils::RemoveRedundantErrors(InCommand, TEXT("is not in a workspace."));
		if (!InCommand.bCommandSuccessful)
		{
			UE_LOG(LogSourceControl, Error, TEXT("FPlasticUpdateStatusWorker(ErrorMessages.Num()=%d) => checkconnection"), InCommand.ErrorMessages.Num());
			// In case of error, execute a 'checkconnection' command to check the connectivity of the server.
			InCommand.bConnectionDropped = !PlasticSourceControlUtils::RunCommand(TEXT("checkconnection"), TArray<FString>(), TArray<FString>(), InCommand.Concurrency, InCommand.InfoMessages, InCommand.ErrorMessages);
			return false;
		}

		if (Operation->ShouldUpdateHistory())
		{
			// Get the history of the files (on all branches)
			InCommand.bCommandSuccessful &= PlasticSourceControlUtils::RunGetHistory(Operation->ShouldUpdateHistory(), States, InCommand.ErrorMessages);

			// Special case for conflicts
			for (FPlasticSourceControlState& State : States)
			{
				if (State.IsConflicted())
				{
					// In case of a merge conflict, we need to put the tip of the "remote branch" on top of the history
					UE_LOG(LogSourceControl, Log, TEXT("%s: PendingMergeSourceChangeset %d"), *State.LocalFilename, State.PendingMergeSourceChangeset);
					for (int32 IdxRevision = 0; IdxRevision < State.History.Num(); IdxRevision++)
					{
						const auto& Revision = State.History[IdxRevision];
						if (Revision->ChangesetNumber == State.PendingMergeSourceChangeset)
						{
							// If the Source Changeset is not already at the top of the History, duplicate it there.
							if (IdxRevision > 0)
							{
								const auto RevisionCopy = Revision;
								State.History.Insert(RevisionCopy, 0);
							}
							break;
						}
					}
				}
			}
		}
		else
		{
			const FPlasticSourceControlModule& PlasticSourceControl = FModuleManager::GetModuleChecked<FPlasticSourceControlModule>("PlasticSourceControl");
			if (PlasticSourceControl.AccessSettings().GetUpdateStatusOtherBranches() && AreAllFiles(InCommand.Files))
			{
				// Get only the last revision of the files (checking all branches)
				// in order to warn the user if the file has been changed on another branch
				InCommand.bCommandSuccessful &= PlasticSourceControlUtils::RunGetHistory(Operation->ShouldUpdateHistory(), States, InCommand.ErrorMessages);
			}
		}
	}
	// no path provided: only update the status of assets in Content/ directory if requested
	// Perforce "opened files" are those that have been modified (or added/deleted): that is what we get with a simple status from the root
	// This is called by the "CheckOut" Content Browser filter as well as our source control Refresh menu.
	else if (Operation->ShouldGetOpenedOnly())
	{
		TArray<FString> ProjectDirs;
		ProjectDirs.Add(FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir()));
		InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunUpdateStatus(ProjectDirs, Operation->ShouldUpdateHistory(), InCommand.Concurrency, InCommand.ErrorMessages, States, InCommand.ChangesetNumber, InCommand.BranchName);
	}

	// TODO: re-evaluate how to optimize this heavy operation using some of these hints flags
	// - ShouldGetOpenedOnly hint would be to call for all a whole workspace status update
	// - ShouldUpdateModifiedState hint not used as the above normal Plastic status has already told us this information (like Git and Mercurial)

	return InCommand.bCommandSuccessful;
}

bool FPlasticUpdateStatusWorker::UpdateStates()
{
	return PlasticSourceControlUtils::UpdateCachedStates(MoveTemp(States));
}

/// Detect if the operation is a duplicate/copy or a rename/move, and if it leaved a redirector (ie it was a move of a source controled asset)
bool IsMoveOperation(const FString& InOrigin)
{
	bool bIsMoveOperation = true;

	FString PackageName;
	if (FPackageName::TryConvertFilenameToLongPackageName(InOrigin, PackageName))
	{
		// Use AsyncTask to call AssetRegistry GetAssetsByPackageName') on Game Thread
		const TSharedRef<TPromise<TArray<FAssetData>>, ESPMode::ThreadSafe> Promise = MakeShareable(new TPromise<TArray<FAssetData>>());
		AsyncTask(ENamedThreads::GameThread, [Promise, PackageName]()
		{
			TArray<FAssetData> AssetsData;
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
			AssetRegistryModule.Get().GetAssetsByPackageName(FName(*PackageName), AssetsData);
			Promise->SetValue(MoveTemp(AssetsData));
		});
		const TArray<FAssetData> AssetsData = Promise->GetFuture().Get();
		UE_LOG(LogSourceControl, Log, TEXT("IsMoveOperation: PackageName: %s, AssetsData: Num=%d"), *PackageName, AssetsData.Num());
		if (AssetsData.Num() > 0)
		{
			const FAssetData& AssetData = AssetsData[0];
			if (!AssetData.IsRedirector())
			{
				UE_LOG(LogSourceControl, Log, TEXT("IsMoveOperation: %s is a plain asset, so it's a duplicate/copy"), *InOrigin);
				bIsMoveOperation = false;
			}
			else
			{
				UE_LOG(LogSourceControl, Log, TEXT("IsMoveOperation: %s is a redirector, so it's a move/rename"), *InOrigin);
			}
		}
		else
		{
			// no asset in package (no redirector) so it should be a rename/move of a newly Added (not Controlled/Checked-In) file
			UE_LOG(LogSourceControl, Log, TEXT("IsMoveOperation: %s does not have asset in package (ie. no redirector) so it's a move/rename of a newly added file"), *InOrigin);
		}
	}

	return bIsMoveOperation;
}

FName FPlasticCopyWorker::GetName() const
{
	return "Copy";
}

bool FPlasticCopyWorker::Execute(FPlasticSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());
	TSharedRef<FCopy, ESPMode::ThreadSafe> Operation = StaticCastSharedRef<FCopy>(InCommand.Operation);

	if (InCommand.Files.Num() == 1)
	{
		const FString& Origin = InCommand.Files[0];
		const FString Destination = FPaths::ConvertRelativePathToFull(Operation->GetDestination());

		// Detect if the operation is a duplicate/copy or a rename/move, and if it leaved a redirector (ie it was a move of a source controled asset)
		const bool bIsMoveOperation = IsMoveOperation(Origin);
		if (bIsMoveOperation)
		{
			UE_LOG(LogSourceControl, Log, TEXT("Moving %s to %s..."), *Origin, *Destination);
			// In case of rename, we have to undo what the Editor (created a redirector and added the dest asset), and then redo it with Plastic SCM
			// - revert the 'cm add' that was applied to the destination by the Editor
			{
				TArray<FString> DestinationFiles;
				DestinationFiles.Add(Destination);
				InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("undochange"), TArray<FString>(), DestinationFiles, InCommand.Concurrency, InCommand.InfoMessages, InCommand.ErrorMessages);
			}
			// - execute a 'cm move --nomoveondisk' command to the destination to tell cm what happened
			if (InCommand.bCommandSuccessful)
			{
				TArray<FString> Parameters;
				Parameters.Add(TEXT("--nomoveondisk"));
				TArray<FString> Files;
				Files.Add(Origin);
				Files.Add(Destination);
				// Detect special case for a partial checkout (CS:-1 in Gluon mode)!
				if (-1 != InCommand.ChangesetNumber)
				{
					InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("move"), Parameters, Files, InCommand.Concurrency, InCommand.InfoMessages, InCommand.ErrorMessages);
				}
				else
				{
					InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("partial move"), Parameters, Files, InCommand.Concurrency, InCommand.InfoMessages, InCommand.ErrorMessages);
				}
			}
			// - add the redirector file (if it exists) to source control
			if (InCommand.bCommandSuccessful)
			{
				TArray<FString> Files;
				Files.Add(Origin);
				// Detect special case for a partial checkout (CS:-1 in Gluon mode)!
				if (-1 != InCommand.ChangesetNumber)
				{
					InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("add"), TArray<FString>(), Files, InCommand.Concurrency, InCommand.InfoMessages, InCommand.ErrorMessages);
				}
				else
				{
					InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("partial add"), TArray<FString>(), Files, InCommand.Concurrency, InCommand.InfoMessages, InCommand.ErrorMessages);
				}
			}
		}
		else
		{
			// copy operation: destination file already added to Source Control, and original asset not changed, so nothing to do
			InCommand.bCommandSuccessful = true;
		}

		// now update the status of our files:
		TArray<FString> BothFiles;
		BothFiles.Add(Origin);
		BothFiles.Add(Destination);
		PlasticSourceControlUtils::RunUpdateStatus(BothFiles, false, InCommand.Concurrency, InCommand.ErrorMessages, States, InCommand.ChangesetNumber, InCommand.BranchName);
	}
	else
	{
		UE_LOG(LogSourceControl, Error, TEXT("Copy is working for one file only: %d provided!"), InCommand.Files.Num());
	}

	return InCommand.bCommandSuccessful;
}

bool FPlasticCopyWorker::UpdateStates()
{
	return PlasticSourceControlUtils::UpdateCachedStates(MoveTemp(States));
}

FName FPlasticSyncWorker::GetName() const
{
	return "Sync";
}

bool FPlasticSyncWorker::Execute(FPlasticSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	TArray<FString> Parameters;
	// Update specified directory to the head of the repository
	// Detect special case for a partial checkout (CS:-1 in Gluon mode)!
	if (-1 != InCommand.ChangesetNumber)
	{
		Parameters.Add(TEXT("--last"));
		Parameters.Add(TEXT("--dontmerge"));
		InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("update"), Parameters, InCommand.Files, InCommand.Concurrency, InCommand.InfoMessages, InCommand.ErrorMessages);
	}
	else
	{
		InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("partial update"), Parameters, InCommand.Files, InCommand.Concurrency, InCommand.InfoMessages, InCommand.ErrorMessages);
	}

	if (InCommand.bCommandSuccessful)
	{
		// now update the status of our files
		// detect the special case of a Sync of the root folder:
		if ((InCommand.Files.Num() == 1) && (InCommand.Files.Last() == InCommand.PathToWorkspaceRoot))
		{
			// only update the status of assets in the Content directory
			TArray<FString> ContentDir;
			ContentDir.Add(FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir()));
			PlasticSourceControlUtils::RunUpdateStatus(ContentDir, false, InCommand.Concurrency, InCommand.ErrorMessages, States, InCommand.ChangesetNumber, InCommand.BranchName);
		}
		// else: optim, no need to update the status of our files since this is done immediately after by the Editor
	}

	return InCommand.bCommandSuccessful;
}

bool FPlasticSyncWorker::UpdateStates()
{
	return PlasticSourceControlUtils::UpdateCachedStates(MoveTemp(States));
}

FName FPlasticResolveWorker::GetName() const
{
	return "Resolve";
}

bool FPlasticResolveWorker::Execute(FPlasticSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	// Currently resolve operation is always on one file only, but the following would works for many
	for (const FString& File : InCommand.Files)
	{
		FPlasticSourceControlModule& PlasticSourceControl = FModuleManager::GetModuleChecked<FPlasticSourceControlModule>("PlasticSourceControl");
		FPlasticSourceControlProvider& Provider = PlasticSourceControl.GetProvider();
		TSharedRef<FPlasticSourceControlState, ESPMode::ThreadSafe> State = Provider.GetStateInternal(File);

		// To resolve the conflict, merge the file by keeping it like it is on file system
		// TODO: according to documentation, this cannot work for cherry-picking
		// merge cs:2@repo@url:port --merge --keepdestination "/path/to/file"

		// Use Merge Parameters obtained in the UpdateStatus operation
		TArray<FString> Parameters = State->PendingMergeParameters;
		Parameters.Add(TEXT("--merge"));
		Parameters.Add(TEXT("--keepdestination"));

		TArray<FString> OneFile;
		OneFile.Add(State->PendingMergeFilename);

		UE_LOG(LogSourceControl, Log, TEXT("resolve %s"), *State->PendingMergeFilename);

		// Mark the conflicted file as resolved
		InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("merge"), Parameters, OneFile, InCommand.Concurrency, InCommand.InfoMessages, InCommand.ErrorMessages);
	}

	// now update the status of our files
	PlasticSourceControlUtils::RunUpdateStatus(InCommand.Files, false, InCommand.Concurrency, InCommand.ErrorMessages, States, InCommand.ChangesetNumber, InCommand.BranchName);

	return InCommand.bCommandSuccessful;
}

bool FPlasticResolveWorker::UpdateStates()
{
	return PlasticSourceControlUtils::UpdateCachedStates(MoveTemp(States));
}

#undef LOCTEXT_NAMESPACE
