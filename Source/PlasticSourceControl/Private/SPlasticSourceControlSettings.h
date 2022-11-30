// Copyright Unity Technologies

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Layout/Visibility.h"
#include "Input/Reply.h"
#include "Styling/SlateTypes.h"

#include "ISourceControlProvider.h"
#include "ISourceControlOperation.h"

class SPlasticSourceControlSettings : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPlasticSourceControlSettings) {}

	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs);

private:
	EVisibility PlasticNotAvailable() const;

	/** Delegate to get cm binary path from settings */
	FText GetBinaryPathText() const;

	/** Delegate to commit cm binary path to settings */
	void OnBinaryPathTextCommited(const FText& InText, ETextCommit::Type InCommitType) const;

	/** Delegate to get workspace root and user name from provider */
	FText GetVersions() const;
	FText GetPathToWorkspaceRoot() const;
	FText GetUserName() const;

	/** Delegate to initialize a new Plastic workspace and repository */
	EVisibility CanInitializePlasticWorkspace() const;
	bool IsReadyToInitializePlasticWorkspace() const;
	void OnWorkspaceNameCommited(const FText& InText, ETextCommit::Type InCommitType);
	FText GetWorkspaceName() const;
	FText WorkspaceName;
	void OnRepositoryNameCommited(const FText& InText, ETextCommit::Type InCommitType);
	FText GetRepositoryName() const;
	FText RepositoryName;
	void OnServerUrlCommited(const FText& InText, ETextCommit::Type InCommitType);
	FText GetServerUrl() const;
	FText ServerUrl;
	bool CanAutoCreateIgnoreFile() const;
	void OnCheckedCreateIgnoreFile(ECheckBoxState NewCheckedState);
	bool bAutoCreateIgnoreFile;

	void OnCheckedUpdateStatusAtStartup(ECheckBoxState NewCheckedState);
	ECheckBoxState IsUpdateStatusAtStartupChecked() const;

	void OnCheckedUpdateStatusOtherBranches(ECheckBoxState NewCheckedState);
	ECheckBoxState IsUpdateStatusOtherBranchesChecked() const;

	void OnCheckedEnableVerboseLogs(ECheckBoxState NewCheckedState);
	ECheckBoxState IsEnableVerboseLogsChecked() const;

	void OnCheckedInitialCommit(ECheckBoxState NewCheckedState);
	bool bAutoInitialCommit;
	void OnInitialCommitMessageCommited(const FText& InText, ETextCommit::Type InCommitType);
	FText GetInitialCommitMessage() const;
	FText InitialCommitMessage;

	/** Launch initial asynchronous add and commit operations */
	void LaunchMakeWorkspaceOperation();
	void LaunchMarkForAddOperation();
	void LaunchCheckInOperation();

	/** Delegate called when a source control operation has completed */
	void OnSourceControlOperationComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult);

	/** Asynchronous operation progress notifications */
	TWeakPtr<class SNotificationItem> OperationInProgressNotification;

	void DisplayInProgressNotification(const FText& InOperationInProgressString);
	void RemoveInProgressNotification();
	void DisplaySuccessNotification(const FName& InOperationName);
	void DisplayFailureNotification(const FName& InOperationName);

	FReply OnClickedInitializePlasticWorkspace();

	/** Delegate to add a Plastic ignore.conf file to an existing Plastic workspace */
	EVisibility CanAddIgnoreFile() const;
	FReply OnClickedAddIgnoreFile() const;

	const FString GetIgnoreFileName() const;
	bool CreateIgnoreFile() const;

	TArray<FString> GetProjectFiles() const;
};
