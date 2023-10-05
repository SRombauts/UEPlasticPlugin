// Copyright Unity Technologies

#pragma once

#include "CoreMinimal.h"
#include "ISourceControlProvider.h"
#include "Runtime/Launch/Resources/Version.h"

class FMenuBuilder;
struct FToolMenuSection;

/** Unity Version Control extension of the Source Control toolbar menu */
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
	void SwitchToPartialWorkspaceClicked();
	bool CanSwitchToPartialWorkspace() const;
	void ShowSourceControlEditorPreferences() const;
	void ShowSourceControlProjectSettings() const;
	void ShowSourceControlPlasticScmProjectSettings() const;
	void VisitDocsURLClicked() const;
	void VisitSupportURLClicked() const;
	void VisitLockRulesURLClicked(const FString InOrganizationName) const;

private:
	bool IsSourceControlConnected() const;

	bool				SaveDirtyPackages();
	TArray<FString>		ListAllPackages();

#if ENGINE_MAJOR_VERSION == 4
	void AddMenuExtension(FMenuBuilder& Menu);

	TSharedRef<class FExtender> OnExtendLevelEditorViewMenu(const TSharedRef<class FUICommandList> CommandList);
#elif ENGINE_MAJOR_VERSION == 5
	void AddMenuExtension(FToolMenuSection& Menu);
#endif

	/** Extends the main Revision Control menu from the toolbar at the bottom-right. */
	void ExtendRevisionControlMenu();
	/** Extends the content browser asset context menu with Admin revision control options. */
	void ExtendAssetContextMenu();
	/** Called to generate concert asset context menu. */
	void GeneratePlasticAssetContextMenu(FMenuBuilder& MenuBuilder, TArray<FAssetData> InAssetObjectPaths);

	bool CanRemoveLocks(TArray<FAssetData> InAssetObjectPaths) const;
	bool CanReleaseLocks(TArray<FAssetData> InAssetObjectPaths) const;
	void ExecuteRemoveLocks(TArray<FAssetData> InAssetObjectPaths);
	void ExecuteReleaseLocks(TArray<FAssetData> InAssetObjectPaths);
	void ExecuteUnlock(const TArray<FAssetData>& InAssetObjectPaths, const bool bInRemove);

	void DisplayInProgressNotification(const FText& InOperationInProgressString);
	void RemoveInProgressNotification();
	void DisplaySucessNotification(const FName& InOperationName);
	void DisplayFailureNotification(const FName& InOperationName);

private:
	/** Tracks if the menu extension has been registered with the editor or not */
	bool bHasRegistered = false;

#if ENGINE_MAJOR_VERSION == 4
	FDelegateHandle ViewMenuExtenderHandle;
#endif

	/** Current source control operation from extended menu if any */
	TWeakPtr<class SNotificationItem> OperationInProgressNotification;

	/** Name of the menu extension going into the global Revision Control (on the toolbar at the bottom-right) */
	static FName UnityVersionControlMainMenuOwnerName;
	/** Name of the asset context menu extension for admin actions over Locks */
	static FName UnityVersionControlAssetContextLocksMenuOwnerName;

	/** Delegate called when a source control operation has completed */
	void OnSourceControlOperationComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult);
};
