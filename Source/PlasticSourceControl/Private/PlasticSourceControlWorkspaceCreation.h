// Copyright (c) 2023 Unity Technologies

#pragma once

#include "CoreMinimal.h"

#include "ISourceControlProvider.h"
#include "ISourceControlOperation.h"

class FPlasticSourceControlWorkspaceCreation
{
public:
	struct FParameters
	{
		FText WorkspaceName;
		FText RepositoryName;
		FText ServerUrl;
		bool bCreatePartialWorkspace = false;
		bool bAutoInitialCommit = true;
		FText InitialCommitMessage;
	};

	FParameters WorkspaceParams;

	void MakeWorkspace(const FParameters& InParameters);

private:
	/** Launch initial asynchronous add and commit operations */
	void LaunchMakeWorkspaceOperation();
	void LaunchMarkForAddOperation();
	void LaunchCheckInOperation();

	/** Delegates called when a source control operation has completed */
	void OnMakeWorkspaceOperationComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult);
	void OnMarkForAddOperationComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult);
	void OnCheckInOperationComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult);
	/** Generic notification handler */
	void OnSourceControlOperationComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult);

	/** Asynchronous operation progress notifications */
	TWeakPtr<class SNotificationItem> OperationInProgressNotification;

	void DisplayInProgressNotification(const FText& InOperationInProgressString);
	void RemoveInProgressNotification();
	void DisplaySuccessNotification(const FName& InOperationName);
	void DisplayFailureNotification(const FName& InOperationName);

	const FString GetIgnoreFileName() const;
	TArray<FString> GetProjectFiles() const;
};
