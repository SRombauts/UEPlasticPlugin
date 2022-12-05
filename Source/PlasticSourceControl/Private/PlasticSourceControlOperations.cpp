// Copyright Unity Technologies

#include "PlasticSourceControlOperations.h"

#include "PlasticSourceControlCommand.h"
#include "PlasticSourceControlModule.h"
#include "PlasticSourceControlProvider.h"
#include "PlasticSourceControlSettings.h"
#include "PlasticSourceControlState.h"
#include "PlasticSourceControlUtils.h"
#include "PlasticSourceControlVersions.h"
#include "ScopedTempFile.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Async/Async.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "SourceControlOperations.h"
#include "ISourceControlModule.h"
#include "Algo/NoneOf.h"

#define LOCTEXT_NAMESPACE "PlasticSourceControl"

template<typename Type>
static FPlasticSourceControlWorkerRef InstantiateWorker(FPlasticSourceControlProvider& PlasticSourceControlProvider)
{
	return MakeShareable(new Type(PlasticSourceControlProvider));
}

void IPlasticSourceControlWorker::RegisterWorkers(FPlasticSourceControlProvider& PlasticSourceControlProvider)
{
	// Register our operations (implemented in PlasticSourceControlOperations.cpp by sub-classing from Engine\Source\Developer\SourceControl\Public\SourceControlOperations.h)
	PlasticSourceControlProvider.RegisterWorker("Connect", FGetPlasticSourceControlWorker::CreateStatic(&InstantiateWorker<FPlasticConnectWorker>));
	PlasticSourceControlProvider.RegisterWorker("CheckOut", FGetPlasticSourceControlWorker::CreateStatic(&InstantiateWorker<FPlasticCheckOutWorker>));
	PlasticSourceControlProvider.RegisterWorker("UpdateStatus", FGetPlasticSourceControlWorker::CreateStatic(&InstantiateWorker<FPlasticUpdateStatusWorker>));
	PlasticSourceControlProvider.RegisterWorker("MarkForAdd", FGetPlasticSourceControlWorker::CreateStatic(&InstantiateWorker<FPlasticMarkForAddWorker>));
	PlasticSourceControlProvider.RegisterWorker("Delete", FGetPlasticSourceControlWorker::CreateStatic(&InstantiateWorker<FPlasticDeleteWorker>));
	PlasticSourceControlProvider.RegisterWorker("Revert", FGetPlasticSourceControlWorker::CreateStatic(&InstantiateWorker<FPlasticRevertWorker>));
	PlasticSourceControlProvider.RegisterWorker("RevertUnchanged", FGetPlasticSourceControlWorker::CreateStatic(&InstantiateWorker<FPlasticRevertUnchangedWorker>));
	PlasticSourceControlProvider.RegisterWorker("RevertAll", FGetPlasticSourceControlWorker::CreateStatic(&InstantiateWorker<FPlasticRevertAllWorker>));
	PlasticSourceControlProvider.RegisterWorker("MakeWorkspace", FGetPlasticSourceControlWorker::CreateStatic(&InstantiateWorker<FPlasticMakeWorkspaceWorker>));
	PlasticSourceControlProvider.RegisterWorker("Sync", FGetPlasticSourceControlWorker::CreateStatic(&InstantiateWorker<FPlasticSyncWorker>));
	PlasticSourceControlProvider.RegisterWorker("SyncAll", FGetPlasticSourceControlWorker::CreateStatic(&InstantiateWorker<FPlasticSyncWorker>));
	PlasticSourceControlProvider.RegisterWorker("CheckIn", FGetPlasticSourceControlWorker::CreateStatic(&InstantiateWorker<FPlasticCheckInWorker>));
	PlasticSourceControlProvider.RegisterWorker("Copy", FGetPlasticSourceControlWorker::CreateStatic(&InstantiateWorker<FPlasticCopyWorker>));
	PlasticSourceControlProvider.RegisterWorker("Resolve", FGetPlasticSourceControlWorker::CreateStatic(&InstantiateWorker<FPlasticResolveWorker>));

#if ENGINE_MAJOR_VERSION == 5
	PlasticSourceControlProvider.RegisterWorker("UpdateChangelistsStatus", FGetPlasticSourceControlWorker::CreateStatic(&InstantiateWorker<FPlasticGetPendingChangelistsWorker>));
	PlasticSourceControlProvider.RegisterWorker("NewChangelist", FGetPlasticSourceControlWorker::CreateStatic(&InstantiateWorker<FPlasticNewChangelistWorker>));
	PlasticSourceControlProvider.RegisterWorker("DeleteChangelist", FGetPlasticSourceControlWorker::CreateStatic(&InstantiateWorker<FPlasticDeleteChangelistWorker>));
	PlasticSourceControlProvider.RegisterWorker("EditChangelist", FGetPlasticSourceControlWorker::CreateStatic(&InstantiateWorker<FPlasticEditChangelistWorker>));
	PlasticSourceControlProvider.RegisterWorker("MoveToChangelist", FGetPlasticSourceControlWorker::CreateStatic(&InstantiateWorker<FPlasticReopenWorker>));
#endif
}


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
	TRACE_CPUPROFILER_EVENT_SCOPE(FPlasticConnectWorker::Execute);

	check(InCommand.Operation->GetName() == GetName());
	TSharedRef<FConnect, ESPMode::ThreadSafe> Operation = StaticCastSharedRef<FConnect>(InCommand.Operation);

	if (GetProvider().IsPlasticAvailable())
	{
		// Get workspace name
		InCommand.bCommandSuccessful = PlasticSourceControlUtils::GetWorkspaceName(InCommand.PathToWorkspaceRoot, InCommand.WorkspaceName, InCommand.ErrorMessages);
		if (InCommand.bCommandSuccessful)
		{
			// Get repository, server URL, branch and current changeset number
			// Note: this initiates the connection to the server and issue network calls, so we don't need an explicit 'checkconnection'
			InCommand.bCommandSuccessful = PlasticSourceControlUtils::GetWorkspaceInformation(InCommand.ChangesetNumber, InCommand.RepositoryName, InCommand.ServerUrl, InCommand.BranchName, InCommand.ErrorMessages);
			if (InCommand.bCommandSuccessful)
			{
				InCommand.InfoMessages.Add(TEXT("Connected successfully"));

				// Now update the status of assets in the Content directory
				// but only on real (re-)connection (but not each time Login() is called by Rename or Fixup Redirector command to check connection)
				// and only if enabled in the settings
				if (!GetProvider().IsAvailable() && GetProvider().AccessSettings().GetUpdateStatusAtStartup())
				{
					TArray<FString> ContentDir;
					ContentDir.Add(FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir()));
					PlasticSourceControlUtils::RunUpdateStatus(ContentDir, false, InCommand.ErrorMessages, States, InCommand.ChangesetNumber, InCommand.BranchName);
				}
			}
			else
			{
				if (InCommand.ErrorMessages.Num() > 0)
				{
					Operation->SetErrorText(FText::FromString(InCommand.ErrorMessages[0]));
				}
				else
				{
					const FText ErrorText(LOCTEXT("FailedToConnect", "Failed to connect to the Plastic SCM server."));
					Operation->SetErrorText(ErrorText);
					InCommand.ErrorMessages.Add(ErrorText.ToString());
				}
			}
		}
		else
		{
			const FText ErrorText(LOCTEXT("NotAPlasticRepository", "Failed to enable Plastic SCM source control. You need to create a Plastic SCM workspace for the project first."));
			Operation->SetErrorText(ErrorText);
			InCommand.ErrorMessages.Add(ErrorText.ToString());
		}
	}
	else
	{
		const FText ErrorText(LOCTEXT("PlasticScmCliUnavaillable", "Failed to launch Plastic SCM 'cm' command line tool. You need to install it and make sure that 'cm' is on the Path and correctly configured."));
		Operation->SetErrorText(ErrorText);
		InCommand.ErrorMessages.Add(ErrorText.ToString());
	}

	return InCommand.bCommandSuccessful;
}

bool FPlasticConnectWorker::UpdateStates()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPlasticConnectWorker::UpdateStates);

	return PlasticSourceControlUtils::UpdateCachedStates(MoveTemp(States));
}


#if ENGINE_MAJOR_VERSION == 5

static void UpdateChangelistState(FPlasticSourceControlProvider& SCCProvider, const FPlasticSourceControlChangelist& InChangelist, const TArray<FPlasticSourceControlState>& InStates)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPlastic::UpdateChangelistState);

	if (InChangelist.IsInitialized())
	{
		TSharedRef<FPlasticSourceControlChangelistState, ESPMode::ThreadSafe> ChangelistState = SCCProvider.GetStateInternal(InChangelist);

		for (const FPlasticSourceControlState& InState : InStates)
		{
			if ((InState.WorkspaceState != EWorkspaceState::CheckedOut) && (InState.WorkspaceState != EWorkspaceState::Added) && InState.WorkspaceState != EWorkspaceState::Deleted)
			{
				continue;
			}

			TSharedRef<FPlasticSourceControlState, ESPMode::ThreadSafe> State = SCCProvider.GetStateInternal(InState.GetFilename());
			ChangelistState->Files.Add(State);

			// Keep the changelist stored with cached file state in sync with the actual changelist that owns this file.
			State->Changelist = InChangelist;
		}
	}
}

#endif


FName FPlasticCheckOutWorker::GetName() const
{
	return "CheckOut";
}

bool FPlasticCheckOutWorker::Execute(FPlasticSourceControlCommand& InCommand)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPlasticCheckOutWorker::Execute);

	check(InCommand.Operation->GetName() == GetName());

	// Detect special case for a partial checkout (CS:-1 in Gluon mode)!
	if (-1 != InCommand.ChangesetNumber)
	{
		InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("checkout"), TArray<FString>(), InCommand.Files, InCommand.InfoMessages, InCommand.ErrorMessages);
	}
	else
	{
		InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("partial checkout"), TArray<FString>(), InCommand.Files, InCommand.InfoMessages, InCommand.ErrorMessages);
	}

	// now update the status of our files
	PlasticSourceControlUtils::RunUpdateStatus(InCommand.Files, false, InCommand.ErrorMessages, States, InCommand.ChangesetNumber, InCommand.BranchName);

	return InCommand.bCommandSuccessful;
}

bool FPlasticCheckOutWorker::UpdateStates()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPlasticCheckOutWorker::UpdateStates);

#if ENGINE_MAJOR_VERSION == 5
	// If files have been checked-out directly to a CL, modify the cached state to reflect it (defaults to the Default changelist).
	UpdateChangelistState(GetProvider(), InChangelist, States);
#endif

	return PlasticSourceControlUtils::UpdateCachedStates(MoveTemp(States));
}

#if ENGINE_MAJOR_VERSION == 5

bool DeleteChangelist(const FPlasticSourceControlProvider& PlasticSourceControlProvider, const FPlasticSourceControlChangelist& InChangelist, TArray<FString>& OutResults, TArray<FString>& OutErrorMessages)
{
	TArray<FString> Parameters;
	TArray<FString> Files;
	if (PlasticSourceControlProvider.GetPlasticScmVersion() < PlasticSourceControlVersions::NewChangelistFileArgs)
	{
		Parameters.Add(TEXT("rm"));
		Files.Add(InChangelist.GetName());
		return PlasticSourceControlUtils::RunCommand(TEXT("changelist"), Parameters, Files, OutResults, OutErrorMessages);
	}
	else
	{
		Parameters.Add(TEXT("delete"));
		const FScopedTempFile ChangelistNameFile(InChangelist.GetName());
		Parameters.Add(FString::Printf(TEXT("--namefile=\"%s\""), *FPaths::ConvertRelativePathToFull(ChangelistNameFile.GetFilename())));
		UE_LOG(LogSourceControl, Verbose, TEXT("DeleteChangelist(%s)"), *InChangelist.GetName());
		return PlasticSourceControlUtils::RunCommand(TEXT("changelist"), Parameters, Files, OutResults, OutErrorMessages);
	}
}

TArray<FString> FileNamesFromFileStates(const TArray<FSourceControlStateRef>& InFileStates)
{
	TArray<FString> Files;

	for (const FSourceControlStateRef& FileState : InFileStates)
	{
		Files.Add(FileState->GetFilename());
	}

	return Files;
}

#endif

/// Parse checkin result, usually looking like "Created changeset cs:8@br:/main@MyProject@SRombauts@cloud (mount:'/')"
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
	TRACE_CPUPROFILER_EVENT_SCOPE(FPlasticCheckInWorker::Execute);

	check(InCommand.Operation->GetName() == GetName());
	TSharedRef<FCheckIn, ESPMode::ThreadSafe> Operation = StaticCastSharedRef<FCheckIn>(InCommand.Operation);

	TArray<FString> Files;
#if ENGINE_MAJOR_VERSION == 5
	if (InCommand.Changelist.IsInitialized() && InCommand.Files.IsEmpty())
	{
		TSharedRef<FPlasticSourceControlChangelistState, ESPMode::ThreadSafe> ChangelistState = GetProvider().GetStateInternal(InCommand.Changelist);
		Files = FileNamesFromFileStates(ChangelistState->Files);

		InChangelist = InCommand.Changelist;
	}
	else
#endif
	{
		Files = InCommand.Files;
	}

	if (Files.Num() > 0)
	{
		UE_LOG(LogSourceControl, Verbose, TEXT("CheckIn: %d file(s) Description: '%s'"), Files.Num(), *Operation->GetDescription().ToString());

		// make a temp file to place our commit message in
		const FScopedTempFile CommitMsgFile(Operation->GetDescription());
		if (!CommitMsgFile.GetFilename().IsEmpty())
		{
			TArray<FString> Parameters;
			Parameters.Add(FString::Printf(TEXT("--commentsfile=\"%s\""), *FPaths::ConvertRelativePathToFull(CommitMsgFile.GetFilename())));
			// Detect special case for a partial checkout (CS:-1 in Gluon mode)!
			if (-1 != InCommand.ChangesetNumber)
			{
				Parameters.Add(TEXT("--all")); // Also files Changed (not CheckedOut) and Moved/Deleted Locally
			//  NOTE: --update added as #23 but removed as #32 because most assets are locked by the Unreal Editor
			//  Parameters.Add(TEXT("--update")); // Processes the update-merge automatically if it eventually happens.
				InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("checkin"), Parameters, Files, InCommand.InfoMessages, InCommand.ErrorMessages);
			}
			else
			{
				Parameters.Add(TEXT("--applychanged")); // Also files Changed (not CheckedOut) and Moved/Deleted Locally
				InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("partial checkin"), Parameters, Files, InCommand.InfoMessages, InCommand.ErrorMessages);
			}
			if (InCommand.bCommandSuccessful)
			{
				// Remove any deleted files from status cache
				TArray<TSharedRef<ISourceControlState, ESPMode::ThreadSafe>> LocalStates;
				GetProvider().GetState(Files, LocalStates, EStateCacheUsage::Use);
				for (const auto& State : LocalStates)
				{
					if (State->IsDeleted())
					{
						GetProvider().RemoveFileFromCache(State->GetFilename());
					}
				}

				Operation->SetSuccessMessage(ParseCheckInResults(InCommand.InfoMessages));
				UE_LOG(LogSourceControl, Log, TEXT("CheckIn successful"));
			}

#if ENGINE_MAJOR_VERSION == 5
			if (InChangelist.IsInitialized() && !InChangelist.IsDefault())
			{
				// NOTE: we need to explicitly delete persistent changelists when we submit its content, except for the Default changelist
				DeleteChangelist(GetProvider(), InChangelist, InCommand.InfoMessages, InCommand.ErrorMessages);
			}
#endif
		}

		// now update the status of our files
		PlasticSourceControlUtils::RunUpdateStatus(Files, false, InCommand.ErrorMessages, States, InCommand.ChangesetNumber, InCommand.BranchName);
	}
	else
	{
		UE_LOG(LogSourceControl, Warning, TEXT("Checkin: No files provided"));
	}

	return InCommand.bCommandSuccessful;
}

bool FPlasticCheckInWorker::UpdateStates()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPlasticCheckInWorker::UpdateStates);

#if ENGINE_MAJOR_VERSION == 5
	// Is the submit of a full changelist (from the View Changelists window) or a set of files (from the Submit Content window)?
	if (InChangelist.IsInitialized())
	{
		if (InChangelist.IsDefault())
		{
			// Remove all the files from the default changelist state, since they have been submitted, but we didn't delete the changelist itself
			TSharedRef<FPlasticSourceControlChangelistState, ESPMode::ThreadSafe> DefaultChangelist = GetProvider().GetStateInternal(FPlasticSourceControlChangelist::DefaultChangelist);
			DefaultChangelist->Files.Empty();
		}
		else
		{
			GetProvider().RemoveChangelistFromCache(InChangelist);
		}
	}
	else
	{
		// Update affected changelists if any
		for (const FPlasticSourceControlState& NewState : States)
		{
			TSharedRef<FPlasticSourceControlState, ESPMode::ThreadSafe> State = GetProvider().GetStateInternal(NewState.GetFilename());
			if (State->Changelist.IsInitialized())
			{
				// 1- Remove these files from their previous changelist
				TSharedRef<FPlasticSourceControlChangelistState, ESPMode::ThreadSafe> PreviousChangelist = GetProvider().GetStateInternal(State->Changelist);
				PreviousChangelist->Files.Remove(State);
			}
		}
	}
#endif

	return PlasticSourceControlUtils::UpdateCachedStates(MoveTemp(States));
}

FName FPlasticMarkForAddWorker::GetName() const
{
	return "MarkForAdd";
}

bool FPlasticMarkForAddWorker::Execute(FPlasticSourceControlCommand& InCommand)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPlasticMarkForAddWorker::Execute);

	check(InCommand.Operation->GetName() == GetName());

	if (InCommand.Files.Num() > 0)
	{
		TArray<FString> Parameters;
		Parameters.Add(TEXT("--parents")); // NOTE: deprecated in 8.0.16.3100 when it became the default https://www.plasticscm.com/download/releasenotes/8.0.16.3100
		// Note: using "?" is a workaround to trigger the Plastic's "SkipIgnored" internal flag meaning "don't add file that are ignored":
		//          options.SkipIgnored = cla.GetWildCardArguments().Count > 0;
		//       It's behavior is similar as Subversion:
		//          if you explicitly add one file that is ignored, "cm" will happily accept it and add it,
		//          if you try to add a set of files with a pattern, "cm" will skip the files that are ignored and only add the other ones
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
			InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("add"), Parameters, InCommand.Files, InCommand.InfoMessages, InCommand.ErrorMessages);
		}
		else
		{
			InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("partial add"), Parameters, InCommand.Files, InCommand.InfoMessages, InCommand.ErrorMessages);
		}

		// now update the status of our files
		PlasticSourceControlUtils::RunUpdateStatus(InCommand.Files, false, InCommand.ErrorMessages, States, InCommand.ChangesetNumber, InCommand.BranchName);
	}
	else
	{
		UE_LOG(LogSourceControl, Warning, TEXT("MarkforAdd: No files provided"));
	}

	return InCommand.bCommandSuccessful;
}

bool FPlasticMarkForAddWorker::UpdateStates()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPlasticMarkForAddWorker::UpdateStates);

#if ENGINE_MAJOR_VERSION == 5
	// If files have been added directly to a CL, modify the cached state to reflect it (defaults to the Default changelist).
	UpdateChangelistState(GetProvider(), InChangelist, States);
#endif

	return PlasticSourceControlUtils::UpdateCachedStates(MoveTemp(States));
}

FName FPlasticDeleteWorker::GetName() const
{
	return "Delete";
}

bool FPlasticDeleteWorker::Execute(FPlasticSourceControlCommand& InCommand)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPlasticDeleteWorker::Execute);

	check(InCommand.Operation->GetName() == GetName());

	// Detect special case for a partial checkout (CS:-1 in Gluon mode)!
	if (-1 != InCommand.ChangesetNumber)
	{
		InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("remove"), TArray<FString>(), InCommand.Files, InCommand.InfoMessages, InCommand.ErrorMessages);
	}
	else
	{
		InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("partial remove"), TArray<FString>(), InCommand.Files, InCommand.InfoMessages, InCommand.ErrorMessages);
	}

	// now update the status of our files
	PlasticSourceControlUtils::RunUpdateStatus(InCommand.Files, false, InCommand.ErrorMessages, States, InCommand.ChangesetNumber, InCommand.BranchName);

	return InCommand.bCommandSuccessful;
}

bool FPlasticDeleteWorker::UpdateStates()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPlasticDeleteWorkers::UpdateStates);

#if ENGINE_MAJOR_VERSION == 5
	// If files have been deleted directly to a CL, modify the cached state to reflect it (defaults to the Default changelist).
	UpdateChangelistState(GetProvider(), InChangelist, States);
#endif

	return PlasticSourceControlUtils::UpdateCachedStates(MoveTemp(States));
}

FName FPlasticRevertWorker::GetName() const
{
	return "Revert";
}

bool FPlasticRevertWorker::Execute(FPlasticSourceControlCommand& InCommand)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPlasticRevertWorker::Execute);

	check(InCommand.Operation->GetName() == GetName());

	TArray<FString> Files;
#if ENGINE_MAJOR_VERSION == 5
	if (InCommand.Changelist.IsInitialized() && InCommand.Files.IsEmpty())
	{
		TSharedRef<FPlasticSourceControlChangelistState, ESPMode::ThreadSafe> ChangelistState = GetProvider().GetStateInternal(InCommand.Changelist);
		Files = FileNamesFromFileStates(ChangelistState->Files);
	}
	else
#endif
	{
		Files = InCommand.Files;
	}

	for (int i = 0; i < Files.Num(); i++) // Required for loop on index since we are adding to the Files array as we go
	{
		const FString& File = Files[i];

		const TSharedRef<const FPlasticSourceControlState, ESPMode::ThreadSafe> State = GetProvider().GetStateInternal(File);

		if (State->WorkspaceState == EWorkspaceState::Moved)
		{
			const FString& MovedFrom = State->MovedFrom;
			
			// In case of a file Moved/Renamed, consider the rename Origin (where there is now a Redirector)
			// and add it to the list of files to revert (only if it is not already in) to revert both at once
			if (!Files.FindByPredicate([&MovedFrom](const FString& File) { return File.Equals(MovedFrom, ESearchCase::IgnoreCase); }))
			{
				Files.Add(MovedFrom);
			}

			// and delete the Redirector (else the reverted file will collide with it and create a *.private.0 file)
			IFileManager::Get().Delete(*MovedFrom);
		}
	}

	InCommand.bCommandSuccessful = true;

	if (Files.Num() > 0)
	{
		// revert the checkout and any changes of the given file in workspace
		// Detect special case for a partial checkout (CS:-1 in Gluon mode)!
		if (-1 != InCommand.ChangesetNumber)
		{
			InCommand.bCommandSuccessful &= PlasticSourceControlUtils::RunCommand(TEXT("undo"), TArray<FString>(), Files, InCommand.InfoMessages, InCommand.ErrorMessages);
		}
		else
		{
			InCommand.bCommandSuccessful &= PlasticSourceControlUtils::RunCommand(TEXT("partial undo"), TArray<FString>(), Files, InCommand.InfoMessages, InCommand.ErrorMessages);
		}
	}

	// NOTE: optim, in UE4 there was no need to update the status of our files since this is done immediately after by the Editor, except now that we are using changelists
#if ENGINE_MAJOR_VERSION == 5
	// update the status of our files
	PlasticSourceControlUtils::RunUpdateStatus(Files, false, InCommand.ErrorMessages, States, InCommand.ChangesetNumber, InCommand.BranchName);
#endif

	return InCommand.bCommandSuccessful;
}

bool FPlasticRevertWorker::UpdateStates()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPlasticRevertWorker::UpdateStates);

#if ENGINE_MAJOR_VERSION == 5
	// Update affected changelists if any
	for (const FPlasticSourceControlState& NewState : States)
	{
		TSharedRef<FPlasticSourceControlState, ESPMode::ThreadSafe> State = GetProvider().GetStateInternal(NewState.GetFilename());
		if (State->Changelist.IsInitialized())
		{
			// 1- Remove these files from their previous changelist
			TSharedRef<FPlasticSourceControlChangelistState, ESPMode::ThreadSafe> PreviousChangelist = GetProvider().GetStateInternal(State->Changelist);
			PreviousChangelist->Files.Remove(State);
		}
	}
#endif

	return PlasticSourceControlUtils::UpdateCachedStates(MoveTemp(States));
}

FName FPlasticRevertUnchangedWorker::GetName() const
{
	return "RevertUnchanged";
}

bool FPlasticRevertUnchangedWorker::Execute(FPlasticSourceControlCommand& InCommand)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPlasticRevertUnchangedWorker::Execute);

	check(InCommand.Operation->GetName() == GetName());

	TArray<FString> Parameters;
	Parameters.Add(TEXT("-R"));

	TArray<FString> Files;
#if ENGINE_MAJOR_VERSION == 5
	if (InCommand.Changelist.IsInitialized() && InCommand.Files.IsEmpty())
	{
		TSharedRef<FPlasticSourceControlChangelistState, ESPMode::ThreadSafe> ChangelistState = GetProvider().GetStateInternal(InCommand.Changelist);
		Files = FileNamesFromFileStates(ChangelistState->Files);
	}
	else
#endif
	{
		Files = InCommand.Files;
	}

	// revert the checkout of all unchanged files recursively
	InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("uncounchanged"), Parameters, Files, InCommand.InfoMessages, InCommand.ErrorMessages);

	// Now update the status of either the files, or all assets in the Content directory
	if (Files.Num() == 0)
	{
		Files.Add(FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir()));
	}
	// detect the special case of a Sync of the root folder:
	else if ((InCommand.Files.Num() == 1) && (InCommand.Files[0] == InCommand.PathToWorkspaceRoot))
	{
		// only update the status of assets in the Content directory
		Files[0] = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
	}
	PlasticSourceControlUtils::RunUpdateStatus(Files, false, InCommand.ErrorMessages, States, InCommand.ChangesetNumber, InCommand.BranchName);

	return InCommand.bCommandSuccessful;
}

bool FPlasticRevertUnchangedWorker::UpdateStates()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPlasticRevertUnchangedWorker::UpdateStates);

#if ENGINE_MAJOR_VERSION == 5
	// Update affected changelists if any
	for (const FPlasticSourceControlState& NewState : States)
	{
		if (!NewState.IsModified())
		{
			TSharedRef<FPlasticSourceControlState, ESPMode::ThreadSafe> State = GetProvider().GetStateInternal(NewState.GetFilename());
			if (State->Changelist.IsInitialized())
			{
				// 1- Remove these files from their previous changelist
				TSharedRef<FPlasticSourceControlChangelistState, ESPMode::ThreadSafe> PreviousChangelist = GetProvider().GetStateInternal(State->Changelist);
				PreviousChangelist->Files.Remove(State);
			}
		}
	}
#endif

	return PlasticSourceControlUtils::UpdateCachedStates(MoveTemp(States));
}

FName FPlasticRevertAllWorker::GetName() const
{
	return "RevertAll";
}

bool FPlasticRevertAllWorker::Execute(FPlasticSourceControlCommand& InCommand)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPlasticRevertAllWorker::Execute);

	check(InCommand.Operation->GetName() == GetName());
	TSharedRef<FPlasticRevertAll, ESPMode::ThreadSafe> Operation = StaticCastSharedRef<FPlasticRevertAll>(InCommand.Operation);

	// Start by updating the Status of all Content, find the Moved/Renamed files, and delete the redirectors (else the undocheckout produces some .private.0 files)
	{
		TArray<FPlasticSourceControlState> TempStates;
		TArray<FString> ContentDir;
		ContentDir.Add(FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir()));
		// TODO: here we would just need a fast "status" to only retrieve the local status of files, not server locks etc.
		PlasticSourceControlUtils::RunUpdateStatus(ContentDir, false, InCommand.ErrorMessages, TempStates, InCommand.ChangesetNumber, InCommand.BranchName);
		for (auto& State : TempStates)
		{
			if (State.WorkspaceState == EWorkspaceState::Moved)
			{
				// In case of a file Moved/Renamed, find the rename Origin and delete the Redirector
				if (!State.MovedFrom.IsEmpty())
				{
					IFileManager::Get().Delete(*State.MovedFrom);
				}
			}
		}
	}

	TArray<FString> Results;
	TArray<FString> Parameters;
	Parameters.Add(TEXT("--all"));
	Parameters.Add(TEXT("--machinereadable"));
	// revert the checkout of all files recursively
	// Detect special case for a partial checkout (CS:-1 in Gluon mode)!
	if (-1 != InCommand.ChangesetNumber)
	{
		InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("undocheckout"), Parameters, TArray<FString>(), Results, InCommand.ErrorMessages);
	}
	else
	{
		InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("partial undocheckout"), Parameters, TArray<FString>(), Results, InCommand.ErrorMessages);
	}

	// Parse Results
	for (FString& Result : Results)
	{
		FPaths::NormalizeFilename(Result);

		// Detect special case of rename and split both origin and redirection as two separate updated files:
		// eg "c:/Workspace/UE5PlasticPluginDev/Content/LevelPrototyping/Materials/M_Black.uasset c:/Workspace/UE5PlasticPluginDev/Content/LevelPrototyping/Materials/M_NewMaterial.uasset"
		const int32 IndexRename = Result.Find(InCommand.PathToWorkspaceRoot, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		if (IndexRename > 0)
		{
			Operation->UpdatedFiles.Add(Result.Left(IndexRename - 1));
			Operation->UpdatedFiles.Add(Result.RightChop(IndexRename));
		}
		else
		{
			Operation->UpdatedFiles.Add(MoveTemp(Result));
		}
	}

	// now update the status of the updated files
	if (Operation->UpdatedFiles.Num())
	{
		PlasticSourceControlUtils::RunUpdateStatus(Operation->UpdatedFiles, false, InCommand.ErrorMessages, States, InCommand.ChangesetNumber, InCommand.BranchName);
	}

	return InCommand.bCommandSuccessful;
}

bool FPlasticRevertAllWorker::UpdateStates()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPlasticRevertAllWorker::UpdateStates);

#if ENGINE_MAJOR_VERSION == 5
	// Update affected changelists if any
	for (const FPlasticSourceControlState& NewState : States)
	{
		if (!NewState.IsModified())
		{
			TSharedRef<FPlasticSourceControlState, ESPMode::ThreadSafe> State = GetProvider().GetStateInternal(NewState.GetFilename());
			if (State->Changelist.IsInitialized())
			{
				// 1- Remove these files from their previous changelist
				TSharedRef<FPlasticSourceControlChangelistState, ESPMode::ThreadSafe> PreviousChangelist = GetProvider().GetStateInternal(State->Changelist);
				PreviousChangelist->Files.Remove(State);
			}
		}
	}
#endif

	return PlasticSourceControlUtils::UpdateCachedStates(MoveTemp(States));
}

FName FPlasticMakeWorkspaceWorker::GetName() const
{
	return "MakeWorkspace";
}

bool FPlasticMakeWorkspaceWorker::Execute(FPlasticSourceControlCommand& InCommand)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPlasticMakeWorkspaceWorker::Execute);

	check(InCommand.Operation->GetName() == GetName());
	TSharedRef<FPlasticMakeWorkspace, ESPMode::ThreadSafe> Operation = StaticCastSharedRef<FPlasticMakeWorkspace>(InCommand.Operation);

	{
		TArray<FString> Parameters;
		Parameters.Add(Operation->ServerUrl);
		Parameters.Add(Operation->RepositoryName);
		PlasticSourceControlUtils::RunCommand(TEXT("makerepository"), Parameters, TArray<FString>(), InCommand.InfoMessages, InCommand.ErrorMessages);
	}
	{
		TArray<FString> Parameters;
		Parameters.Add(Operation->WorkspaceName);
		Parameters.Add(TEXT(".")); // current path, ie. ProjectDir
		Parameters.Add(FString::Printf(TEXT("--repository=rep:%s@repserver:%s"), *Operation->RepositoryName, *Operation->ServerUrl));
		InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("makeworkspace"), Parameters, TArray<FString>(), InCommand.InfoMessages, InCommand.ErrorMessages);
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
	TRACE_CPUPROFILER_EVENT_SCOPE(FPlasticUpdateStatusWorker::Execute);

	check(InCommand.Operation->GetName() == GetName());
	TSharedRef<FUpdateStatus, ESPMode::ThreadSafe> Operation = StaticCastSharedRef<FUpdateStatus>(InCommand.Operation);

	// Note: ShouldCheckAllFiles is never set to true (SetCheckingAllFiles)
	UE_LOG(LogSourceControl, Log, TEXT("status (of %d files, ShouldUpdateHistory=%d, ShouldGetOpenedOnly=%d, ShouldUpdateModifiedState=%d)"),
		InCommand.Files.Num(), Operation->ShouldUpdateHistory(), Operation->ShouldGetOpenedOnly(), Operation->ShouldUpdateModifiedState());

	TArray<FString> Files;
#if ENGINE_MAJOR_VERSION == 5
	if (InCommand.Changelist.IsInitialized() && InCommand.Files.IsEmpty())
	{
		TSharedRef<FPlasticSourceControlChangelistState, ESPMode::ThreadSafe> ChangelistState = GetProvider().GetStateInternal(InCommand.Changelist);
		Files = FileNamesFromFileStates(ChangelistState->Files);
	}
	else
#endif
	{
		Files = InCommand.Files;
	}

	if (Files.Num() > 0)
	{
		InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunUpdateStatus(Files, Operation->ShouldUpdateHistory(), InCommand.ErrorMessages, States, InCommand.ChangesetNumber, InCommand.BranchName);
		// Remove all "is not in a workspace" error and convert the result to "success" if there are no other errors
		PlasticSourceControlUtils::RemoveRedundantErrors(InCommand, TEXT("is not in a workspace."));
		if (!InCommand.bCommandSuccessful)
		{
			TArray<FString> ErrorMessages;
			TArray<FString> Parameters;
			FString BranchName, RepositoryName, ServerUrl;
			if (PlasticSourceControlUtils::GetWorkspaceInfo(BranchName, RepositoryName, ServerUrl, ErrorMessages))
			{
				Parameters.Add(FString::Printf(TEXT("--server=%s"), *ServerUrl));
			}
			UE_LOG(LogSourceControl, Error, TEXT("FPlasticUpdateStatusWorker(ErrorMessages.Num()=%d) => checkconnection"), InCommand.ErrorMessages.Num());
			// In case of error, execute a 'checkconnection' command to check the connectivity of the server.
			InCommand.bConnectionDropped = !PlasticSourceControlUtils::RunCommand(TEXT("checkconnection"), Parameters, TArray<FString>(), InCommand.InfoMessages, InCommand.ErrorMessages);
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
			FPlasticSourceControlSettings& PlasticSettings = GetProvider().AccessSettings();
			if (PlasticSettings.GetUpdateStatusOtherBranches() && AreAllFiles(Files))
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
		InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunUpdateStatus(ProjectDirs, Operation->ShouldUpdateHistory(), InCommand.ErrorMessages, States, InCommand.ChangesetNumber, InCommand.BranchName);
	}
	else
	{
		// TODO: workaround for the case of submitting a changelist, calling UpdateStatus with no files nor the changelist.
		// No consequences, and no way to fix it, so let's not show an error.
		InCommand.bCommandSuccessful = true;
	}

	// TODO: re-evaluate how to optimize this heavy operation using some of these hints flags
	// - ShouldGetOpenedOnly hint would be to call for all a whole workspace status update
	// - ShouldUpdateModifiedState hint not used as the above normal Plastic status has already told us this information (like Git and Mercurial)

	return InCommand.bCommandSuccessful;
}

bool FPlasticUpdateStatusWorker::UpdateStates()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPlasticUpdateStatusWorker::UpdateStates);

#if ENGINE_MAJOR_VERSION == 5
	// Update affected changelists if any (in case of a file reverted outside of the Unreal Editor)
	for (const FPlasticSourceControlState& NewState : States)
	{
		if (!NewState.IsModified())
		{
			TSharedRef<FPlasticSourceControlState, ESPMode::ThreadSafe> State = GetProvider().GetStateInternal(NewState.GetFilename());
			if (State->Changelist.IsInitialized())
			{
				// 1- Remove these files from their previous changelist
				TSharedRef<FPlasticSourceControlChangelistState, ESPMode::ThreadSafe> PreviousChangelist = GetProvider().GetStateInternal(State->Changelist);
				PreviousChangelist->Files.Remove(State);
			}
		}
	}
#endif

	return PlasticSourceControlUtils::UpdateCachedStates(MoveTemp(States));
}

#if ENGINE_MAJOR_VERSION == 4 || ENGINE_MINOR_VERSION < 1
/// Detect if the operation is a duplicate/copy or a rename/move, and if it leaved a redirector (ie it was a move of a source controlled asset)
bool IsMoveOperation(const FString& InOrigin)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPlasticCopyWorker::IsMoveOperation);

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
#endif

FName FPlasticCopyWorker::GetName() const
{
	return "Copy";
}

bool FPlasticCopyWorker::Execute(FPlasticSourceControlCommand& InCommand)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPlasticCopyWorker::Execute);

	check(InCommand.Operation->GetName() == GetName());
	TSharedRef<FCopy, ESPMode::ThreadSafe> Operation = StaticCastSharedRef<FCopy>(InCommand.Operation);

	if (InCommand.Files.Num() == 1)
	{
		const FString& Origin = InCommand.Files[0];
		const FString Destination = FPaths::ConvertRelativePathToFull(Operation->GetDestination());

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
		// Branch: The new file is branched from the original file (vs Add: The new file has no relation to the original file)
		const bool bIsMoveOperation = (Operation->CopyMethod == FCopy::ECopyMethod::Branch);
#else
		// Detect if the operation is a duplicate/copy or a rename/move, by checking if it leaved a redirector (ie it was a move of a source controlled asset)
		const bool bIsMoveOperation = IsMoveOperation(Origin);
#endif
		if (bIsMoveOperation)
		{
			UE_LOG(LogSourceControl, Log, TEXT("Moving %s to %s..."), *Origin, *Destination);
			// In case of rename, we have to undo what the Editor (created a redirector and added the dest asset), and then redo it with Plastic SCM
			// - revert the 'cm add' that was applied to the destination by the Editor
			{
				TArray<FString> DestinationFiles;
				DestinationFiles.Add(Destination);
				InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("undochange"), TArray<FString>(), DestinationFiles, InCommand.InfoMessages, InCommand.ErrorMessages);
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
					InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("move"), Parameters, Files, InCommand.InfoMessages, InCommand.ErrorMessages);
				}
				else
				{
					InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("partial move"), Parameters, Files, InCommand.InfoMessages, InCommand.ErrorMessages);
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
					InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("add"), TArray<FString>(), Files, InCommand.InfoMessages, InCommand.ErrorMessages);
				}
				else
				{
					InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("partial add"), TArray<FString>(), Files, InCommand.InfoMessages, InCommand.ErrorMessages);
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
		PlasticSourceControlUtils::RunUpdateStatus(BothFiles, false, InCommand.ErrorMessages, States, InCommand.ChangesetNumber, InCommand.BranchName);
	}
	else
	{
		UE_LOG(LogSourceControl, Error, TEXT("Copy is working for one file only: %d provided!"), InCommand.Files.Num());
	}

	return InCommand.bCommandSuccessful;
}

bool FPlasticCopyWorker::UpdateStates()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPlasticCopyWorkers::UpdateStates);

	return PlasticSourceControlUtils::UpdateCachedStates(MoveTemp(States));
}

FName FPlasticSyncWorker::GetName() const
{
	return "Sync";
}

bool FPlasticSyncWorker::Execute(FPlasticSourceControlCommand& InCommand)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPlasticSyncWorker::Execute);

	TArray<FString> UpdatedFiles;
	InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunUpdate(InCommand.Files, GetProvider().IsPartialWorkspace(), UpdatedFiles, InCommand.ErrorMessages);

	// now update the status of the updated files
	if (UpdatedFiles.Num())
	{
		PlasticSourceControlUtils::RunUpdateStatus(UpdatedFiles, false, InCommand.ErrorMessages, States, InCommand.ChangesetNumber, InCommand.BranchName);
	}

	if ((InCommand.Operation->GetName() == FName("SyncAll")))
	{
		TSharedRef<FPlasticSyncAll, ESPMode::ThreadSafe> Operation = StaticCastSharedRef<FPlasticSyncAll>(InCommand.Operation);
		Operation->UpdatedFiles = MoveTemp(UpdatedFiles);
	}

	return InCommand.bCommandSuccessful;
}

bool FPlasticSyncWorker::UpdateStates()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPlasticSyncWorker::UpdateStates);

	return PlasticSourceControlUtils::UpdateCachedStates(MoveTemp(States));
}

FName FPlasticResolveWorker::GetName() const
{
	return "Resolve";
}

bool FPlasticResolveWorker::Execute(FPlasticSourceControlCommand& InCommand)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPlasticResolveWorker::Execute);

	check(InCommand.Operation->GetName() == GetName());

	// Currently resolve operation is always on one file only, but the following would works for many
	for (const FString& File : InCommand.Files)
	{
		auto State = GetProvider().GetStateInternal(File);

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
		InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("merge"), Parameters, OneFile, InCommand.InfoMessages, InCommand.ErrorMessages);
	}

	// now update the status of our files
	PlasticSourceControlUtils::RunUpdateStatus(InCommand.Files, false, InCommand.ErrorMessages, States, InCommand.ChangesetNumber, InCommand.BranchName);

	return InCommand.bCommandSuccessful;
}

bool FPlasticResolveWorker::UpdateStates()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPlasticResolveWorker::UpdateStates);

	return PlasticSourceControlUtils::UpdateCachedStates(MoveTemp(States));
}


#if ENGINE_MAJOR_VERSION == 5

FName FPlasticGetPendingChangelistsWorker::GetName() const
{
	return "UpdateChangelistsStatus";
}

bool FPlasticGetPendingChangelistsWorker::Execute(FPlasticSourceControlCommand& InCommand)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPlasticGetPendingChangelistsWorker::Execute);

	check(InCommand.Operation->GetName() == GetName());
	TSharedRef<FUpdatePendingChangelistsStatus, ESPMode::ThreadSafe> Operation = StaticCastSharedRef<FUpdatePendingChangelistsStatus>(InCommand.Operation);

	InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunGetChangelists(OutChangelistsStates, OutCLFilesStates, InCommand.ErrorMessages);
	if (InCommand.bCommandSuccessful)
	{
		// Remove the changelist that were not requested by the user.
		if (!Operation->ShouldUpdateAllChangelists())
		{
			const TArray<FSourceControlChangelistRef>& RequestedChangelists = Operation->GetChangelistsToUpdate();
			OutChangelistsStates.RemoveAll([&RequestedChangelists](const FPlasticSourceControlChangelistState& ChangelistState)
				{
					FPlasticSourceControlChangelistRef RemoveChangelistCandidate = StaticCastSharedRef<FPlasticSourceControlChangelist>(ChangelistState.GetChangelist());
					return !RequestedChangelists.ContainsByPredicate([&RemoveChangelistCandidate](const FSourceControlChangelistRef& Requested)
						{
							return StaticCastSharedRef<FPlasticSourceControlChangelist>(Requested)->GetName() == RemoveChangelistCandidate->GetName();
						});
				});
		}
	}

	bCleanupCache = InCommand.bCommandSuccessful;

	return InCommand.bCommandSuccessful;
}

bool FPlasticGetPendingChangelistsWorker::UpdateStates()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPlasticGetPendingChangelistsWorker::UpdateStates);

	bool bUpdated = false;

	const FDateTime Now = FDateTime::Now();

	// first update cached state from 'changes' call
	for (int StatusIndex = 0; StatusIndex < OutChangelistsStates.Num(); StatusIndex++)
	{
		const FPlasticSourceControlChangelistState& CLStatus = OutChangelistsStates[StatusIndex];
		TSharedRef<FPlasticSourceControlChangelistState, ESPMode::ThreadSafe> ChangelistState = GetProvider().GetStateInternal(CLStatus.Changelist);
		// Timestamp is used to throttle status requests, so update it to current time:
		*ChangelistState = CLStatus;
		ChangelistState->TimeStamp = Now;
		bUpdated = true;

		// Update files states for files in the changelist
		bool bUpdateFilesStates = (OutCLFilesStates.Num() == OutChangelistsStates.Num());
		if (bUpdateFilesStates)
		{
			ChangelistState->Files.Reset(OutCLFilesStates[StatusIndex].Num());
			for (const auto& FileState : OutCLFilesStates[StatusIndex])
			{
				TSharedRef<FPlasticSourceControlState, ESPMode::ThreadSafe> CachedFileState = GetProvider().GetStateInternal(FileState.LocalFilename);
				// Don't override "fileinfo" information and the potential LockedByOther state
				if (CachedFileState->WorkspaceState != EWorkspaceState::LockedByOther)
				{
					CachedFileState->WorkspaceState = FileState.WorkspaceState;
				}
				CachedFileState->Changelist = CLStatus.Changelist;
				ChangelistState->Files.AddUnique(CachedFileState);
			}
		}
	}

	if (bCleanupCache)
	{
		TArray<FPlasticSourceControlChangelist> ChangelistsToRemove;
		GetProvider().GetCachedStateByPredicate([this, &ChangelistsToRemove](const FSourceControlChangelistStateRef& InCLState) {
			TSharedRef<FPlasticSourceControlChangelistState, ESPMode::ThreadSafe> CLState = StaticCastSharedRef<FPlasticSourceControlChangelistState>(InCLState);

			if (Algo::NoneOf(OutChangelistsStates, [&CLState](const FPlasticSourceControlChangelistState& UpdatedCLState) {
					return CLState->Changelist == UpdatedCLState.Changelist;
				}))
			{
				ChangelistsToRemove.Add(CLState->Changelist);
			}

			return false;
			});

		for (const FPlasticSourceControlChangelist& ChangelistToRemove : ChangelistsToRemove)
		{
			GetProvider().RemoveChangelistFromCache(ChangelistToRemove);
		}
	}

	return bUpdated;
}

FPlasticSourceControlChangelist GenerateUniqueChangelistName(FPlasticSourceControlProvider& PlasticSourceControlProvider)
{
	FPlasticSourceControlChangelist NewChangelist;

	// Generate a unique number for the name of the new changelist: start from current changeset and increment until a number is available as a new changelist number
	int32 ChangelistNumber = PlasticSourceControlProvider.GetChangesetNumber();
	bool bNewNumberOk = false;
	do
	{
		ChangelistNumber++;
		NewChangelist = FPlasticSourceControlChangelist(FString::FromInt(ChangelistNumber));
		TSharedRef<FPlasticSourceControlChangelistState, ESPMode::ThreadSafe> ChangelistState = PlasticSourceControlProvider.GetStateInternal(NewChangelist);
		bNewNumberOk = !ChangelistState->Changelist.IsInitialized();
	} while (!bNewNumberOk);
	NewChangelist.SetInitialized();

	return NewChangelist;
}

FPlasticSourceControlChangelist CreatePendingChangelist(FPlasticSourceControlProvider& PlasticSourceControlProvider, const FString& InDescription, TArray<FString>& InInfoMessages, TArray<FString>& InErrorMessages)
{
	FPlasticSourceControlChangelist NewChangelist = GenerateUniqueChangelistName(PlasticSourceControlProvider);

	bool bCommandSuccessful;
	TArray<FString> Parameters;
	if (PlasticSourceControlProvider.GetPlasticScmVersion() < PlasticSourceControlVersions::NewChangelistFileArgs)
	{
		Parameters.Add(TEXT("add"));
		Parameters.Add(TEXT("\"") + NewChangelist.GetName() + TEXT("\""));
		Parameters.Add(TEXT("\"") + InDescription + TEXT("\""));
		Parameters.Add(TEXT("--persistent")); // Create a persistent changelist to stay close to Perforce behavior
		bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("changelist"), Parameters, TArray<FString>(), InInfoMessages, InErrorMessages);
	}
	else
	{
		Parameters.Add(TEXT("create"));
		const FScopedTempFile ChangelistNameFile(NewChangelist.GetName());
		Parameters.Add(FString::Printf(TEXT("--namefile=\"%s\""), *FPaths::ConvertRelativePathToFull(ChangelistNameFile.GetFilename())));
		const FScopedTempFile ChangelistDescriptionFile(InDescription);
		Parameters.Add(FString::Printf(TEXT("--descriptionfile=\"%s\""), *FPaths::ConvertRelativePathToFull(ChangelistDescriptionFile.GetFilename())));
		Parameters.Add(TEXT("--persistent")); // Create a persistent changelist to stay close to Perforce behavior
		UE_LOG(LogSourceControl, Verbose, TEXT("CreatePendingChangelist(%s):\n\"%s\""), *NewChangelist.GetName(), *InDescription);
		bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("changelist"), Parameters, TArray<FString>(), InInfoMessages, InErrorMessages);
	}
	if (!bCommandSuccessful)
	{
		NewChangelist.Reset();
	}

	return NewChangelist;
}

bool EditChangelistDescription(const FPlasticSourceControlProvider& PlasticSourceControlProvider, const FPlasticSourceControlChangelist& InChangelist, const FString& InDescription, TArray<FString>& InInfoMessages, TArray<FString>& InErrorMessages)
{
	TArray<FString> Parameters;
	Parameters.Add(TEXT("edit"));
	if (PlasticSourceControlProvider.GetPlasticScmVersion() < PlasticSourceControlVersions::NewChangelistFileArgs)
	{
		Parameters.Add(TEXT("\"") + InChangelist.GetName() + TEXT("\""));
		Parameters.Add(TEXT("description"));
		Parameters.Add(TEXT("\"") + InDescription + TEXT("\""));
		return PlasticSourceControlUtils::RunCommand(TEXT("changelist"), Parameters, TArray<FString>(), InInfoMessages, InErrorMessages);
	}
	else
	{
		const FScopedTempFile ChangelistNameFile(InChangelist.GetName());
		Parameters.Add(FString::Printf(TEXT("--namefile=\"%s\""), *FPaths::ConvertRelativePathToFull(ChangelistNameFile.GetFilename())));
		Parameters.Add(TEXT("description"));
		const FScopedTempFile ChangelistDescriptionFile(InDescription);
		Parameters.Add(FString::Printf(TEXT("--descriptionfile=\"%s\""), *FPaths::ConvertRelativePathToFull(ChangelistDescriptionFile.GetFilename())));
		UE_LOG(LogSourceControl, Verbose, TEXT("EditChangelistDescription(%s\n%s)"), *InChangelist.GetName(), *InDescription);
		return PlasticSourceControlUtils::RunCommand(TEXT("changelist"), Parameters, TArray<FString>(), InInfoMessages, InErrorMessages);
	}
}

bool MoveFilesToChangelist(const FPlasticSourceControlProvider& PlasticSourceControlProvider, const FPlasticSourceControlChangelist& InChangelist, const TArray<FString>& InFiles, TArray<FString>& OutResults, TArray<FString>& OutErrorMessages)
{
	if (InFiles.Num() > 0)
	{
		TArray<FString> Parameters;
		if (PlasticSourceControlProvider.GetPlasticScmVersion() < PlasticSourceControlVersions::NewChangelistFileArgs)
		{
			Parameters.Add(TEXT("\"") + InChangelist.GetName() + TEXT("\""));
			Parameters.Add(TEXT("add"));
			return PlasticSourceControlUtils::RunCommand(TEXT("changelist"), Parameters, InFiles, OutResults, OutErrorMessages);
		}
		else
		{
			const FScopedTempFile ChangelistNameFile(InChangelist.GetName());
			Parameters.Add(FString::Printf(TEXT("--namefile=\"%s\""), *FPaths::ConvertRelativePathToFull(ChangelistNameFile.GetFilename())));
			Parameters.Add(TEXT("add"));
			UE_LOG(LogSourceControl, Verbose, TEXT("MoveFilesToChangelist(%s)"), *InChangelist.GetName());
			return PlasticSourceControlUtils::RunCommand(TEXT("changelist"), Parameters, InFiles, OutResults, OutErrorMessages);
		}
	}
	return true;
}

FPlasticNewChangelistWorker::FPlasticNewChangelistWorker(FPlasticSourceControlProvider& InSourceControlProvider)
	: IPlasticSourceControlWorker(InSourceControlProvider)
	, NewChangelistState(NewChangelist)
{
}

FName FPlasticNewChangelistWorker::GetName() const
{
	return "NewChangelist";
}

bool FPlasticNewChangelistWorker::Execute(class FPlasticSourceControlCommand& InCommand)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPlasticNewChangelistWorker::Execute);

	check(InCommand.Operation->GetName() == GetName());
	TSharedRef<FNewChangelist, ESPMode::ThreadSafe> Operation = StaticCastSharedRef<FNewChangelist>(InCommand.Operation);

	FString Description = Operation->GetDescription().ToString();
	// Note: old "cm" doesn't support newlines, quotes, and question marks on changelist's name or description
	if (GetProvider().GetPlasticScmVersion() < PlasticSourceControlVersions::NewChangelistFileArgs)
	{
		Description.ReplaceInline(TEXT("\r\n"), TEXT(" "), ESearchCase::CaseSensitive);
		Description.ReplaceCharInline(TEXT('\n'), TEXT(' '), ESearchCase::CaseSensitive);
		Description.ReplaceCharInline(TEXT('\"'), TEXT('\''), ESearchCase::CaseSensitive);
		Description.ReplaceCharInline(TEXT('?'), TEXT('.'), ESearchCase::CaseSensitive);
		Description.ReplaceCharInline(TEXT('*'), TEXT('.'), ESearchCase::CaseSensitive);
	}

	// Create a new numbered persistent changelist ala Perforce
	NewChangelist = CreatePendingChangelist(GetProvider(), Description, InCommand.InfoMessages, InCommand.ErrorMessages);

	// Successfully created new changelist
	if (NewChangelist.IsInitialized())
	{
		InCommand.bCommandSuccessful = true;

		NewChangelistState.Changelist = NewChangelist;
		NewChangelistState.Description = MoveTemp(Description);

		Operation->SetNewChangelist(MakeShared<FPlasticSourceControlChangelist>(NewChangelist));

		if (InCommand.Files.Num() > 0)
		{
			InCommand.bCommandSuccessful = MoveFilesToChangelist(GetProvider(), NewChangelist, InCommand.Files, InCommand.InfoMessages, InCommand.ErrorMessages);
			if (InCommand.bCommandSuccessful)
			{
				MovedFiles = InCommand.Files;
			}
		}
	}

	return InCommand.bCommandSuccessful;
}

bool FPlasticNewChangelistWorker::UpdateStates()
{
	if (NewChangelist.IsInitialized())
	{
		const FDateTime Now = FDateTime::Now();

		TSharedRef<FPlasticSourceControlChangelistState, ESPMode::ThreadSafe> ChangelistState = GetProvider().GetStateInternal(NewChangelist);
		*ChangelistState = NewChangelistState;
		ChangelistState->TimeStamp = Now;

		// 3 things to do here:
		for (const FString& MovedFile : MovedFiles)
		{
			TSharedRef<FPlasticSourceControlState, ESPMode::ThreadSafe> FileState = GetProvider().GetStateInternal(MovedFile);

			// 1- Remove these files from their previous changelist
			TSharedRef<FPlasticSourceControlChangelistState, ESPMode::ThreadSafe> PreviousChangelist = GetProvider().GetStateInternal(FileState->Changelist);
			PreviousChangelist->Files.Remove(FileState);

			// 2- Add to the new changelist
			ChangelistState->Files.Add(FileState);

			// 3- Update changelist in file state
			FileState->Changelist = NewChangelist;
			FileState->TimeStamp = Now;
		}

		return true;
	}
	else
	{
		return false;
	}
}


FName FPlasticDeleteChangelistWorker::GetName() const
{
	return "DeleteChangelist";
}

bool FPlasticDeleteChangelistWorker::Execute(class FPlasticSourceControlCommand& InCommand)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPlasticDeleteChangelistWorker::Execute);

	// Can't delete the default changelist
	if (InCommand.Changelist.IsDefault())
	{
		InCommand.bCommandSuccessful = false;
	}
	else
	{
		check(InCommand.Operation->GetName() == GetName());
		TSharedRef<FDeleteChangelist, ESPMode::ThreadSafe> Operation = StaticCastSharedRef<FDeleteChangelist>(InCommand.Operation);

		InCommand.bCommandSuccessful = DeleteChangelist(GetProvider(), InCommand.Changelist, InCommand.InfoMessages, InCommand.ErrorMessages);

		// NOTE: for now it is not possible to delete a changelist with files through the Editor
		if (InCommand.Files.Num() > 0 && InCommand.bCommandSuccessful)
		{
			TSharedRef<FPlasticSourceControlChangelistState, ESPMode::ThreadSafe> ChangelistState = GetProvider().GetStateInternal(InCommand.Changelist);
			const TArray<FString> Files = FileNamesFromFileStates(ChangelistState->Files);
			InCommand.bCommandSuccessful = MoveFilesToChangelist(GetProvider(), FPlasticSourceControlChangelist::DefaultChangelist, Files, InCommand.InfoMessages, InCommand.ErrorMessages);
		}

		// Keep track of changelist to update the cache
		if (InCommand.bCommandSuccessful)
		{
			DeletedChangelist = InCommand.Changelist;
		}
	}

	return InCommand.bCommandSuccessful;
}

bool FPlasticDeleteChangelistWorker::UpdateStates()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPlasticDeleteChangelistWorker::UpdateStates);

	if (DeletedChangelist.IsInitialized())
	{
		return GetProvider().RemoveChangelistFromCache(DeletedChangelist);
	}

	return false;
}


FName FPlasticEditChangelistWorker::GetName() const
{
	return "EditChangelist";
}

bool FPlasticEditChangelistWorker::Execute(class FPlasticSourceControlCommand& InCommand)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPlasticEditChangelistWorker::Execute);

	check(InCommand.Operation->GetName() == GetName());
	TSharedRef<FEditChangelist, ESPMode::ThreadSafe> Operation = StaticCastSharedRef<FEditChangelist>(InCommand.Operation);

	EditedDescription = Operation->GetDescription().ToString();
	// Note: old "cm" doesn't support newlines, quotes, and question marks on changelist's name or description
	if (GetProvider().GetPlasticScmVersion() < PlasticSourceControlVersions::NewChangelistFileArgs)
	{
		EditedDescription.ReplaceInline(TEXT("\r\n"), TEXT(" "), ESearchCase::CaseSensitive);
		EditedDescription.ReplaceCharInline(TEXT('\n'), TEXT(' '), ESearchCase::CaseSensitive);
		EditedDescription.ReplaceCharInline(TEXT('\"'), TEXT('\''), ESearchCase::CaseSensitive);
		EditedDescription.ReplaceCharInline(TEXT('?'), TEXT('.'), ESearchCase::CaseSensitive);
		EditedDescription.ReplaceCharInline(TEXT('*'), TEXT('.'), ESearchCase::CaseSensitive);
	}

	if (InCommand.Changelist.IsDefault())
	{
		// Create a new numbered persistent changelist since we cannot edit the default changelist
		EditedChangelist = CreatePendingChangelist(GetProvider(), EditedDescription, InCommand.InfoMessages, InCommand.ErrorMessages);
		if (EditedChangelist.IsInitialized())
		{
			// And then move all its files to the new changelist
			TSharedRef<FPlasticSourceControlChangelistState, ESPMode::ThreadSafe> ChangelistState = GetProvider().GetStateInternal(InCommand.Changelist);
			ReopenedFiles = FileNamesFromFileStates(ChangelistState->Files);
			InCommand.bCommandSuccessful = MoveFilesToChangelist(GetProvider(), EditedChangelist, ReopenedFiles, InCommand.InfoMessages, InCommand.ErrorMessages);
		}
	}
	else
	{
		InCommand.bCommandSuccessful = EditChangelistDescription(GetProvider(), InCommand.Changelist, EditedDescription, InCommand.InfoMessages, InCommand.ErrorMessages);
		if (InCommand.bCommandSuccessful)
		{
			EditedChangelist = InCommand.Changelist;
		}
	}

	return InCommand.bCommandSuccessful;
}

bool FPlasticEditChangelistWorker::UpdateStates()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPlasticEditChangelistWorker::UpdateStates);

	if (EditedChangelist.IsInitialized())
	{
		const FDateTime Now = FDateTime::Now();
		TSharedRef<FPlasticSourceControlChangelistState, ESPMode::ThreadSafe> EditedChangelistState = GetProvider().GetStateInternal(EditedChangelist);
		EditedChangelistState->Description = EditedDescription;
		EditedChangelistState->Changelist = EditedChangelist;
		EditedChangelistState->TimeStamp = Now;

		// 3 things to do here:
		for (const FString& ReopenedFile : ReopenedFiles)
		{
			TSharedRef<FPlasticSourceControlState, ESPMode::ThreadSafe> FileState = GetProvider().GetStateInternal(ReopenedFile);

			// 1- Remove these files from their previous changelist
			TSharedRef<FPlasticSourceControlChangelistState, ESPMode::ThreadSafe> PreviousChangelist = GetProvider().GetStateInternal(FileState->Changelist);
			PreviousChangelist->Files.Remove(FileState);

			// 2- Add to the new changelist
			EditedChangelistState->Files.Add(FileState);

			// 3- Update changelist in file state
			FileState->Changelist = EditedChangelist;
			FileState->TimeStamp = Now;
		}

		return true;
	}
	else
	{
		return false;
	}
}


FName FPlasticReopenWorker::GetName() const
{
	return "MoveToChangelist";
}

bool FPlasticReopenWorker::Execute(FPlasticSourceControlCommand& InCommand)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPlasticReopenWorker::Execute);

	check(InCommand.Operation->GetName() == GetName());

	InCommand.bCommandSuccessful = MoveFilesToChangelist(GetProvider(), InCommand.Changelist, InCommand.Files, InCommand.InfoMessages, InCommand.ErrorMessages);
	if (InCommand.bCommandSuccessful)
	{
		ReopenedFiles = InCommand.Files;
		DestinationChangelist = InCommand.Changelist;
	}

	return InCommand.bCommandSuccessful;
}

bool FPlasticReopenWorker::UpdateStates()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPlasticReopenWorker::UpdateStates);

	if (DestinationChangelist.IsInitialized())
	{
		const FDateTime Now = FDateTime::Now();
		TSharedRef<FPlasticSourceControlChangelistState, ESPMode::ThreadSafe> DestinationChangelistState = GetProvider().GetStateInternal(DestinationChangelist);

		// 3 things to do here:
		for (const FString& ReopenedFile : ReopenedFiles)
		{
			TSharedRef<FPlasticSourceControlState, ESPMode::ThreadSafe> FileState = GetProvider().GetStateInternal(ReopenedFile);

			// 1- Remove these files from their previous changelist
			TSharedRef<FPlasticSourceControlChangelistState, ESPMode::ThreadSafe> PreviousChangelist = GetProvider().GetStateInternal(FileState->Changelist);
			PreviousChangelist->Files.Remove(FileState);

			// 2- Add to the new changelist
			DestinationChangelistState->Files.Add(FileState);

			// 3- Update changelist in file state
			FileState->Changelist = DestinationChangelist;
			FileState->TimeStamp = Now;
		}

		return ReopenedFiles.Num() > 0;
	}
	else
	{
		return false;
	}
}

#endif

#undef LOCTEXT_NAMESPACE
