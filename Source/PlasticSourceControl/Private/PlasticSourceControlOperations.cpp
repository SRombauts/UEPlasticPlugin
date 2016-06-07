// Copyright (c) 2016 Codice Software - Sebastien Rombauts (sebastien.rombauts@gmail.com)

#include "PlasticSourceControlPrivatePCH.h"
#include "PlasticSourceControlOperations.h"
#include "PlasticSourceControlState.h"
#include "PlasticSourceControlCommand.h"
#include "PlasticSourceControlModule.h"
#include "PlasticSourceControlUtils.h"
#include "AssetRegistryModule.h"

#define LOCTEXT_NAMESPACE "PlasticSourceControl"

FName FPlasticConnectWorker::GetName() const
{
	return "Connect";
}

bool FPlasticConnectWorker::Execute(FPlasticSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());
	TSharedRef<FConnect, ESPMode::ThreadSafe> Operation = StaticCastSharedRef<FConnect>(InCommand.Operation);

	UE_LOG(LogSourceControl, Log, TEXT("connect"));

	// Execute a 'status' command to check for the workspace
	TArray<FString> Parameters;
	Parameters.Add(TEXT("--nochanges"));
	TArray<FString> Files;
	Files.Add(InCommand.PathToWorkspaceRoot);

	InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("status"), Parameters, Files, InCommand.InfoMessages, InCommand.ErrorMessages);
	if(!InCommand.bCommandSuccessful || InCommand.ErrorMessages.Num() > 0 || InCommand.InfoMessages.Num() == 0)
	{
		Operation->SetErrorText(LOCTEXT("NotAPlasticRepository", "Failed to enable Plastic source control. You need to initialize the project as a Plastic repository first."));
		InCommand.bCommandSuccessful = false;
	}
	else
	{
		// Execute a 'checkconnection' command to check the connectivity of the server.
		InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("checkconnection"), TArray<FString>(), Files, InCommand.InfoMessages, InCommand.ErrorMessages);
		if (!InCommand.bCommandSuccessful || InCommand.ErrorMessages.Num() > 0 || InCommand.InfoMessages.Num() == 0)
		{
			Operation->SetErrorText(FText::FromString(InCommand.ErrorMessages[0]));
		}
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

	UE_LOG(LogSourceControl, Log, TEXT("checkin"));

	// make a temp file to place our commit message in
	FScopedTempFile CommitMsgFile(Operation->GetDescription());
	if (CommitMsgFile.GetFilename().Len() > 0)
	{
		TArray<FString> Parameters;
		Parameters.Add(TEXT("--all")); // Also files Changed (not CheckedOut) and Moved/Deleted Locally
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

	// revert any changes in workspace
	InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("undochange"), TArray<FString>(), InCommand.Files, InCommand.InfoMessages, InCommand.ErrorMessages);

	// now update the status of our files
	PlasticSourceControlUtils::RunUpdateStatus(InCommand.Files, InCommand.ErrorMessages, States);

	return InCommand.bCommandSuccessful;
}

bool FPlasticRevertWorker::UpdateStates() const
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

	// @todo: temporary debug log
	UE_LOG(LogSourceControl, Log, TEXT("status (of %d files)"), InCommand.Files.Num());

	if (InCommand.Files.Num() > 0)
	{
		{
			// Execute beforehand a 'checkconnection' command to check the connectivity of the server.
			TArray<FString> Files;
			Files.Add(InCommand.PathToWorkspaceRoot);
			InCommand.bConnectionDropped = !PlasticSourceControlUtils::RunCommand(TEXT("checkconnection"), TArray<FString>(), Files, InCommand.InfoMessages, InCommand.ErrorMessages);
		}
		if (!InCommand.bConnectionDropped)
		{
			InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunUpdateStatus(InCommand.Files, InCommand.ErrorMessages, States);
			PlasticSourceControlUtils::RemoveRedundantErrors(InCommand, TEXT("is not in a workspace."));
		}
		if (!InCommand.bCommandSuccessful)
		{
			// In case of error, execute a 'checkconnection' command to check the connectivity of the server.
			TArray<FString> Files;
			Files.Add(InCommand.PathToWorkspaceRoot);
			InCommand.bConnectionDropped = !PlasticSourceControlUtils::RunCommand(TEXT("checkconnection"), TArray<FString>(), Files, InCommand.InfoMessages, InCommand.ErrorMessages);
		}
		else
		{
			if (Operation->ShouldUpdateHistory())
			{
				for (int32 Index = 0; Index < States.Num(); Index++)
				{
					FString& File = InCommand.Files[Index];
					TPlasticSourceControlHistory History;

					if (States[Index].IsSourceControlled())
					{
						// Get the history of the file in the current branch
						InCommand.bCommandSuccessful &= PlasticSourceControlUtils::RunGetHistory(File, InCommand.ErrorMessages, History);
						Histories.Add(*File, History);
					}
				}
			}
		}
	}
	else
	{
		UE_LOG(LogSourceControl, Log, TEXT("status (with no files)"));
		InCommand.bCommandSuccessful = true;
		// Perforce "opened files" are those that have been modified (or added/deleted): that is what we get with a simple Plastic status from the root
		if (Operation->ShouldGetOpenedOnly())
		{
			/** TODO: UpdateStatus() from the ContentDir()
			TArray<FString> Files;
			Files.Add(FPaths::ConvertRelativePathToFull(FPaths::GameDir()));
			InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunUpdateStatus(Files, InCommand.ErrorMessages, States);
			*/
		}
	}

	// don't use the ShouldUpdateModifiedState() hint here as it is specific to Perforce: the above normal Plastic status has already told us this information (like Git and Mercurial)

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

FName FPlasticCopyWorker::GetName() const
{
	return "Copy";
}

bool FPlasticCopyWorker::Execute(FPlasticSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());
	TSharedRef<FCopy, ESPMode::ThreadSafe> Operation = StaticCastSharedRef<FCopy>(InCommand.Operation);

	if (InCommand.Files.Num() > 0)
	{
		const FString& Origin = InCommand.Files[0];
		const FString Destination = FPaths::ConvertRelativePathToFull(Operation->GetDestination());

		// Detect if the copy leaved a redirector (ie it was a rename/move) or not (it was a duplicate/copy)
		bool bIsCopyOperation = false;
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		FString PackageName;
		if (FPackageName::TryConvertFilenameToLongPackageName(Origin, PackageName))
		{
			TArray<FAssetData> AssetsData;
			AssetRegistryModule.Get().GetAssetsByPackageName(FName(*PackageName), AssetsData);
			UE_LOG(LogSourceControl, Log, TEXT("PackageName: %s, AssetsData: Num=%d"), *PackageName, AssetsData.Num());
			if (AssetsData.Num() > 0)
			{
				const FAssetData& AssetData = AssetsData[0];
				if (!AssetData.IsRedirector())
				{
					// @todo temporary debug log
					UE_LOG(LogSourceControl, Log, TEXT("%s is a plain asset, so it's a duplicate/copy"), *Origin);
					bIsCopyOperation = true;
				}
				else
				{
					// @todo temporary debug log
					UE_LOG(LogSourceControl, Log, TEXT("%s is a redirector, so it's a move/rename"), *Origin);
				}
			}
			else
			{
				// @todo temporary debug log
				// no asset in package (no redirector) so it should be a rename/move of a just added file
				UE_LOG(LogSourceControl, Log, TEXT("%s does not have asset in package (ie. no redirector) so it's a move/rename of a newly added file"), *Origin);
			}
		}

		// Now start the real work: 
		if (!bIsCopyOperation)
		{
			UE_LOG(LogSourceControl, Log, TEXT("Moving %s to %s..."), *Origin, *Destination);
			// In case of rename, we have to undo what the Editor (created a redirector and added the dest asset), and then redo it with Plastic SCM
			// - backup the redirector (if it exists) to a temp file
			const bool bReplace = true;
			const bool bEvenIfReadOnly = true;
			const FString TempFileName = FPaths::CreateTempFilename(*FPaths::GameLogDir(), TEXT("Plastic-MoveTemp"), TEXT(".uasset"));
			InCommand.bCommandSuccessful = IFileManager::Get().Move(*TempFileName, *Origin, bReplace, bEvenIfReadOnly);
			// - revert the 'cm add' that was applied to the destination by the Editor
			if (InCommand.bCommandSuccessful)
			{
				TArray<FString> DestinationFiles;
				DestinationFiles.Add(Destination);
				InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("undochange"), TArray<FString>(), DestinationFiles, InCommand.InfoMessages, InCommand.ErrorMessages);
			}
			// - move back the asset from the destination to it's original location
			if (InCommand.bCommandSuccessful)
			{
				InCommand.bCommandSuccessful = IFileManager::Get().Move(*Origin, *Destination, bReplace, bEvenIfReadOnly);
			}
			// - execute a 'cm move' command to the destination
			if (InCommand.bCommandSuccessful)
			{
				TArray<FString> Files;
				Files.Add(Origin);
				Files.Add(Destination);
				InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("move"), TArray<FString>(), Files, InCommand.InfoMessages, InCommand.ErrorMessages);
			}
			// - restore the redirector file (if it exists) to it's former location
			if (InCommand.bCommandSuccessful)
			{
				InCommand.bCommandSuccessful = IFileManager::Get().Move(*Origin, *TempFileName, bReplace, bEvenIfReadOnly);
			}
			// - add the redirector file (if it exists) to source control
			if (InCommand.bCommandSuccessful)
			{
				TArray<FString> Files;
				Files.Add(Origin);
				InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("add"), TArray<FString>(), Files, InCommand.InfoMessages, InCommand.ErrorMessages);
			}
		}
		else
		{
			// copy operation: destination file already added to Source Control, and original asset not changed, so nothing to do
			InCommand.bCommandSuccessful = true;
		}

		// now update the status of our files
		PlasticSourceControlUtils::RunUpdateStatus(InCommand.Files, InCommand.ErrorMessages, States);
	}

	return InCommand.bCommandSuccessful;
}

bool FPlasticCopyWorker::UpdateStates() const
{
	return PlasticSourceControlUtils::UpdateCachedStates(States);
}

FName FPlasticSyncWorker::GetName() const
{
	return "Sync";
}

bool FPlasticSyncWorker::Execute(FPlasticSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	UE_LOG(LogSourceControl, Log, TEXT("Sync"));
	TArray<FString> Files;
	Files.Add(InCommand.PathToWorkspaceRoot); // Always update the Root of the Workspace

	// revert any changes in workspace
	InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("update"), TArray<FString>(), Files, InCommand.InfoMessages, InCommand.ErrorMessages);

	// now update the status of our files
	PlasticSourceControlUtils::RunUpdateStatus(InCommand.Files, InCommand.ErrorMessages, States);

	return InCommand.bCommandSuccessful;
}

bool FPlasticSyncWorker::UpdateStates() const
{
	return PlasticSourceControlUtils::UpdateCachedStates(States);
}

#undef LOCTEXT_NAMESPACE
