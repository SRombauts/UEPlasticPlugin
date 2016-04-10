// Copyright (c) 2016 Codice Software - Sebastien Rombauts (sebastien.rombauts@gmail.com)

#include "PlasticSourceControlPrivatePCH.h"
#include "PlasticSourceControlOperations.h"
#include "PlasticSourceControlState.h"
#include "PlasticSourceControlCommand.h"
#include "PlasticSourceControlModule.h"
#include "PlasticSourceControlUtils.h"

#define LOCTEXT_NAMESPACE "PlasticSourceControl"

FName FPlasticConnectWorker::GetName() const
{
	return "Connect";
}

bool FPlasticConnectWorker::Execute(FPlasticSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	UE_LOG(LogSourceControl, Log, TEXT("status"));

	TArray<FString> Parameters;
	Parameters.Add(TEXT("--nochanges"));
	TArray<FString> Files;
	Files.Add(InCommand.PathToRepositoryRoot);

	InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("status"), Parameters, Files, InCommand.InfoMessages, InCommand.ErrorMessages);
	if(!InCommand.bCommandSuccessful || InCommand.ErrorMessages.Num() > 0 || InCommand.InfoMessages.Num() == 0)
	{
		StaticCastSharedRef<FConnect>(InCommand.Operation)->SetErrorText(LOCTEXT("NotAPlasticRepository", "Failed to enable Plastic source control. You need to initialize the project as a Plastic repository first."));
		InCommand.bCommandSuccessful = false;
	}

	return InCommand.bCommandSuccessful;
}

bool FPlasticConnectWorker::UpdateStates() const
{
	return false;
}

FName FPlasticCheckOutWorker::GetName() const
{
	return "CheckOut";
}

bool FPlasticCheckOutWorker::Execute(FPlasticSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	UE_LOG(LogSourceControl, Log, TEXT("checkout"));

	InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("checkout"), TArray<FString>(), InCommand.Files, InCommand.InfoMessages, InCommand.ErrorMessages);

	// now update the status of our files
	PlasticSourceControlUtils::RunUpdateStatus(InCommand.Files, InCommand.ErrorMessages, States);

	return InCommand.bCommandSuccessful;
}

bool FPlasticCheckOutWorker::UpdateStates() const
{
	return PlasticSourceControlUtils::UpdateCachedStates(States);
}


static FText ParseCheckInResults(const TArray<FString>& InResults)
{
	if (InResults.Num() >= 1)
	{
		const FString& LastLine = InResults[InResults.Num()-1];
		return FText::FromString(LastLine);
	}
	return LOCTEXT("CommitMessageUnknown", "Submitted revision.");
}

FName FPlasticCheckInWorker::GetName() const
{
	return "CheckIn";
}

bool FPlasticCheckInWorker::Execute(FPlasticSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	TSharedRef<FCheckIn, ESPMode::ThreadSafe> Operation = StaticCastSharedRef<FCheckIn>(InCommand.Operation);

	// make a temp file to place our commit message in
	FScopedTempFile CommitMsgFile(Operation->GetDescription());
	if (CommitMsgFile.GetFilename().Len() > 0)
	{
		TArray<FString> Parameters;
		Parameters.Add(TEXT("--all")); // Also files Changed (not CheckedOut) and Moved/Deleted Localy
		FString ParamCommitMsgFilename = TEXT("--commentsfile=\"");
		ParamCommitMsgFilename += FPaths::ConvertRelativePathToFull(CommitMsgFile.GetFilename());
		ParamCommitMsgFilename += TEXT("\"");
		Parameters.Add(ParamCommitMsgFilename);

		InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("checkin"), Parameters, InCommand.Files, InCommand.InfoMessages, InCommand.ErrorMessages);
		if (InCommand.bCommandSuccessful)
		{
			// Remove any deleted files from status cache
			FPlasticSourceControlModule& PlasticSourceControl = FModuleManager::LoadModuleChecked<FPlasticSourceControlModule>("PlasticSourceControl");
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
			UE_LOG(LogSourceControl, Log, TEXT("FPlasticCheckInWorker: CheckIn successful"));
		}
	}

	// now update the status of our files
	PlasticSourceControlUtils::RunUpdateStatus(InCommand.Files, InCommand.ErrorMessages, States);

	return InCommand.bCommandSuccessful;
}

bool FPlasticCheckInWorker::UpdateStates() const
{
	return PlasticSourceControlUtils::UpdateCachedStates(States);
}


FName FPlasticUpdateStatusWorker::GetName() const
{
	return "UpdateStatus";
}

bool FPlasticUpdateStatusWorker::Execute(FPlasticSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	TSharedRef<FUpdateStatus, ESPMode::ThreadSafe> Operation = StaticCastSharedRef<FUpdateStatus>(InCommand.Operation);

	UE_LOG(LogSourceControl, Log, TEXT("status"));

	if (InCommand.Files.Num() > 0)
	{
		InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunUpdateStatus(InCommand.Files, InCommand.ErrorMessages, States);
		PlasticSourceControlUtils::RemoveRedundantErrors(InCommand, TEXT("' is not in a workspace"));

		if (Operation->ShouldUpdateHistory())
		{
			for (int32 Index = 0; Index < States.Num(); Index++)
			{
				FString& File = InCommand.Files[Index];
				TPlasticSourceControlHistory History;

				if (States[Index].IsConflicted())
				{
					// In case of a merge conflict, we first need to get the tip of the "remote branch" (MERGE_HEAD)
// TODO					PlasticSourceControlUtils::RunGetHistory(File, true, InCommand.ErrorMessages, History);
				}
				// Get the history of the file in the current branch
// TODO				InCommand.bCommandSuccessful &= PlasticSourceControlUtils::RunGetHistory(File, false, InCommand.ErrorMessages, History);
				Histories.Add(*File, History);
			}
		}
	}
	else
	{
		// Perforce "opened files" are those that have been modified (or added/deleted): that is what we get with a simple Plastic status from the root
		if (Operation->ShouldGetOpenedOnly())
		{
			TArray<FString> Files;
			Files.Add(FPaths::ConvertRelativePathToFull(FPaths::GameDir()));
			InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunUpdateStatus(Files, InCommand.ErrorMessages, States);
		}
	}

	// TODO don't use the ShouldUpdateModifiedState() hint here as it is specific to Perforce: the above normal Plastic status has already told us this information (like Plastic and Mercurial) ?

	return InCommand.bCommandSuccessful;
}

bool FPlasticUpdateStatusWorker::UpdateStates() const
{
	bool bUpdated = PlasticSourceControlUtils::UpdateCachedStates(States);

	FPlasticSourceControlModule& PlasticSourceControl = FModuleManager::LoadModuleChecked<FPlasticSourceControlModule>("PlasticSourceControl");
	FPlasticSourceControlProvider& Provider = PlasticSourceControl.GetProvider();

	// add history, if any
	for (const auto& History : Histories)
	{
		TSharedRef<FPlasticSourceControlState, ESPMode::ThreadSafe> State = Provider.GetStateInternal(History.Key);
		State->History = History.Value;
		State->TimeStamp = FDateTime::Now();
		bUpdated = true;
	}

	return bUpdated;
}

FName FPlasticMarkForAddWorker::GetName() const
{
	return "MarkForAdd";
}

bool FPlasticMarkForAddWorker::Execute(FPlasticSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	UE_LOG(LogSourceControl, Log, TEXT("add"));

	TArray<FString> Parameters;
	Parameters.Add(TEXT("--parents"));
	InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("add"), Parameters, InCommand.Files, InCommand.InfoMessages, InCommand.ErrorMessages);

	// now update the status of our files
	PlasticSourceControlUtils::RunUpdateStatus(InCommand.Files, InCommand.ErrorMessages, States);

	return InCommand.bCommandSuccessful;
}

bool FPlasticMarkForAddWorker::UpdateStates() const
{
	return PlasticSourceControlUtils::UpdateCachedStates(States);
}

FName FPlasticDeleteWorker::GetName() const
{
	return "Delete";
}

bool FPlasticDeleteWorker::Execute(FPlasticSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	UE_LOG(LogSourceControl, Log, TEXT("Delete"));

	InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("remove"), TArray<FString>(), InCommand.Files, InCommand.InfoMessages, InCommand.ErrorMessages);

	// now update the status of our files
	PlasticSourceControlUtils::RunUpdateStatus(InCommand.Files, InCommand.ErrorMessages, States);

	return InCommand.bCommandSuccessful;
}

bool FPlasticDeleteWorker::UpdateStates() const
{
	return PlasticSourceControlUtils::UpdateCachedStates(States);
}

FName FPlasticRevertWorker::GetName() const
{
	return "Revert";
}

bool FPlasticRevertWorker::Execute(FPlasticSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	UE_LOG(LogSourceControl, Log, TEXT("Revert"));

	// revert any changes in working copy
	{
		InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("undochange"), TArray<FString>(), InCommand.Files, InCommand.InfoMessages, InCommand.ErrorMessages);
	}

	// now update the status of our files
	PlasticSourceControlUtils::RunUpdateStatus(InCommand.Files, InCommand.ErrorMessages, States);

	return InCommand.bCommandSuccessful;
}

bool FPlasticRevertWorker::UpdateStates() const
{
	return PlasticSourceControlUtils::UpdateCachedStates(States);
}

#undef LOCTEXT_NAMESPACE
