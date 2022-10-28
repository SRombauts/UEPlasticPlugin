// Copyright (c) 2016-2022 Codice Software

#pragma once

#include "CoreMinimal.h"
#include "ISourceControlProvider.h"
#include "Runtime/Launch/Resources/Version.h"

class FMenuBuilder;
struct FToolMenuSection;

/** Plastic SCM extension of the Source Control toolbar menu */
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
	void ShowSourceControlEditorPreferences() const;
	void ShowSourceControlProjectSettings() const;
	void ShowSourceControlPlasticScmProjectSettings() const;
	void VisitDocsURLClicked() const;
	void VisitSupportURLClicked() const;

private:
	bool False() const;
	bool IsSourceControlConnected() const;

	bool				SaveDirtyPackages();
	TArray<FString>		ListAllPackages();
	TArray<UPackage*>	UnlinkPackages(const TArray<FString>& InPackageNames);
	void				ReloadPackages(TArray<UPackage*>& InPackagesToReload);

#if ENGINE_MAJOR_VERSION == 4
	void AddMenuExtension(FMenuBuilder& Menu);

	TSharedRef<class FExtender> OnExtendLevelEditorViewMenu(const TSharedRef<class FUICommandList> CommandList);
#elif ENGINE_MAJOR_VERSION == 5
	void AddMenuExtension(FToolMenuSection& Menu);
#endif

	void DisplayInProgressNotification(const FText& InOperationInProgressString);
	void RemoveInProgressNotification();
	void DisplaySucessNotification(const FName& InOperationName);
	void DisplayFailureNotification(const FName& InOperationName);

private:
#if ENGINE_MAJOR_VERSION == 4
	FDelegateHandle ViewMenuExtenderHandle;
#endif

	/** Loaded packages we unlinked, to reload after a Sync or Revert operation */
	TArray<UPackage*> UnlinkedPackages;

	/** Current source control operation from extended menu if any */
	TWeakPtr<class SNotificationItem> OperationInProgressNotification;

	/** Delegate called when a source control operation has completed */
	void OnSourceControlOperationComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult);
};
