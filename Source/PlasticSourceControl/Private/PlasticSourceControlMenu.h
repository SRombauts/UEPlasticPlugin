// Copyright (c) 2016 Codice Software - Sebastien Rombauts (sebastien.rombauts@gmail.com)

#pragma once


class FToolBarBuilder;
class FMenuBuilder;

class FPlasticSourceControlMenu
{
public:
	void Register();
	void Unregister();
	
	/** This functions will be bound to appropriate Command. */
	void SyncProjectClicked();
	void RevertUnchangedClicked();
	void RevertAllClicked();

private:
	void AddMenuExtension(FMenuBuilder& Builder);

	TSharedRef<FExtender> OnExtendLevelEditorViewMenu(const TSharedRef<FUICommandList> CommandList);

	void DisplayInProgressNotification(const FSourceControlOperationRef& InOperation);
	void RemoveInProgressNotification();
	void DisplaySucessNotification(const FSourceControlOperationRef& InOperation);
	void DisplayFailureNotification(const FSourceControlOperationRef& InOperation);

private:
	TSharedPtr<class FUICommandList> PluginCommands;

	FDelegateHandle ViewMenuExtenderHandle;

	/** Current source control operation from menu if any */
	TSharedPtr<FSync, ESPMode::ThreadSafe> SyncOperation;
	TSharedPtr<class FPlasticRevertUnchanged, ESPMode::ThreadSafe> RevertUnchangedOperation;
	TSharedPtr<class FPlasticRevertAll, ESPMode::ThreadSafe> RevertAllOperation;
	TWeakPtr<SNotificationItem> OperationInProgressNotification;
	/** Delegate called when a source control operation has completed */
	void OnSourceControlOperationComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult);
};