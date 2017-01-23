// Copyright (c) 2016 Codice Software - Sebastien Rombauts (sebastien.rombauts@gmail.com)

#pragma once

class SPlasticSourceControlSettings : public SCompoundWidget
{
public:
	
	SLATE_BEGIN_ARGS(SPlasticSourceControlSettings) {}
	
	SLATE_END_ARGS()

public:

	void Construct(const FArguments& InArgs);

private:

	/** Delegate to get cm binary path from settings */
	FText GetBinaryPathText() const;

	/** Delegate to commit cm binary path to settings */
	void OnBinaryPathTextCommited(const FText& InText, ETextCommit::Type InCommitType) const;

	/** Delegate to get workspace root and user name from provider */
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
	void OnCheckedCreateIgnoreFile(ECheckBoxState NewCheckedState);
	bool bAutoCreateIgnoreFile;

	void OnCheckedInitialCommit(ECheckBoxState NewCheckedState);
	bool bAutoInitialCommit;
	void OnInitialCommitMessageCommited(const FText& InText, ETextCommit::Type InCommitType);
	FText GetInitialCommitMessage() const;
	FText InitialCommitMessage;

	/** Initial checkin asynchronous operation, and progress notification */
	TSharedPtr<FCheckIn, ESPMode::ThreadSafe> CheckInOperation;
	TWeakPtr<SNotificationItem> OperationInProgressNotification;

	void DisplayInProgressNotification(const FSourceControlOperationRef& InOperation);
	void RemoveInProgressNotification();
	void DisplaySucessNotification(const FName& InOperationName);
	void DisplayFailureNotification(const FName& InOperationName);

	/** Delegate called when a source control operation has completed */
	void OnSourceControlOperationComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult);


	FReply OnClickedInitializePlasticWorkspace();

	/** Delegate to add a Plastic ignore.conf file to an existing Plastic workspace */
	EVisibility CanAddIgnoreFile() const;
	FReply OnClickedAddIgnoreFile() const;

	const FString& GetIgnoreFileName() const;
	bool CreateIgnoreFile() const;
};