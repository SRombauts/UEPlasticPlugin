// Copyright (c) 2016-2017 Codice Software - Sebastien Rombauts (sebastien.rombauts@gmail.com)

#pragma once

#include "CoreMinimal.h"
#include "ISourceControlProvider.h"

#include "ISourceControlOperation.h"
#include "SourceControlOperations.h"
#include "PlasticSourceControlOperations.h"

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
	void RefreshClicked();

private:
	bool IsSourceControlConnected() const;

	void AddMenuExtension(FMenuBuilder& Builder);

	TSharedRef<class FExtender> OnExtendLevelEditorViewMenu(const TSharedRef<class FUICommandList> CommandList);

	void DisplayInProgressNotification(const FSourceControlOperationRef& InOperation);
	void RemoveInProgressNotification();
	void DisplaySucessNotification(const FSourceControlOperationRef& InOperation);
	void DisplayFailureNotification(const FSourceControlOperationRef& InOperation);

private:
	FDelegateHandle ViewMenuExtenderHandle;

	/** Current source control operation from menu if any */
	TSharedPtr<FSync, ESPMode::ThreadSafe> SyncOperation;
	TSharedPtr<FPlasticRevertUnchanged, ESPMode::ThreadSafe> RevertUnchangedOperation;
	TSharedPtr<FPlasticRevertAll, ESPMode::ThreadSafe> RevertAllOperation;
	TSharedPtr<FUpdateStatus, ESPMode::ThreadSafe> RefreshOperation;
	TWeakPtr<class SNotificationItem> OperationInProgressNotification;
	/** Delegate called when a source control operation has completed */
	void OnSourceControlOperationComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult);
};