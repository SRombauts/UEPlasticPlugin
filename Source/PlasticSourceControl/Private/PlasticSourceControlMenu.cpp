// Copyright (c) 2016-2022 Codice Software

#include "PlasticSourceControlMenu.h"

#include "PlasticSourceControlModule.h"
#include "PlasticSourceControlProvider.h"
#include "PlasticSourceControlOperations.h"

#include "ISourceControlModule.h"
#include "ISourceControlOperation.h"
#include "SourceControlOperations.h"

#include "Interfaces/IPluginManager.h"
#include "LevelEditor.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Misc/MessageDialog.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "Styling/AppStyle.h"
#else
#include "EditorStyleSet.h"
#endif

#include "PackageTools.h"
#include "FileHelpers.h"

#include "Logging/MessageLog.h"

#if ENGINE_MAJOR_VERSION == 5
#include "ToolMenus.h"
#include "ToolMenuContext.h"
#include "ToolMenuMisc.h"
#endif

#define LOCTEXT_NAMESPACE "PlasticSourceControl"

void FPlasticSourceControlMenu::Register()
{
	// Register the menu extension with the level editor
#if ENGINE_MAJOR_VERSION == 4
	if (FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>("LevelEditor"))
	{
		FLevelEditorModule::FLevelEditorMenuExtender ViewMenuExtender = FLevelEditorModule::FLevelEditorMenuExtender::CreateRaw(this, &FPlasticSourceControlMenu::OnExtendLevelEditorViewMenu);
		auto& MenuExtenders = LevelEditorModule->GetAllLevelEditorToolbarSourceControlMenuExtenders();
		MenuExtenders.Add(ViewMenuExtender);
		ViewMenuExtenderHandle = MenuExtenders.Last().GetHandle();
	}
#elif ENGINE_MAJOR_VERSION == 5
	FToolMenuOwnerScoped SourceControlMenuOwner("PlasticSourceControlMenu");
	if (UToolMenus* ToolMenus = UToolMenus::Get())
	{
		UToolMenu* SourceControlMenu = ToolMenus->ExtendMenu("StatusBar.ToolBar.SourceControl");
		FToolMenuSection& Section = SourceControlMenu->AddSection("PlasticSourceControlActions", LOCTEXT("PlasticSourceControlMenuHeadingActions", "Plastic SCM"), FToolMenuInsert(NAME_None, EToolMenuInsertType::First));

		AddMenuExtension(Section);
	}
#endif
}

void FPlasticSourceControlMenu::Unregister()
{
	// Unregister the menu extension from the level editor
#if ENGINE_MAJOR_VERSION == 4
	if (FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>("LevelEditor"))
	{
		LevelEditorModule->GetAllLevelEditorToolbarSourceControlMenuExtenders().RemoveAll([=](const FLevelEditorModule::FLevelEditorMenuExtender& Extender) { return Extender.GetHandle() == ViewMenuExtenderHandle; });
	}
#elif ENGINE_MAJOR_VERSION == 5
	if (UToolMenus* ToolMenus = UToolMenus::Get())
	{
		UToolMenus::Get()->UnregisterOwnerByName("PlasticSourceControlMenu");
	}
#endif
}

bool FPlasticSourceControlMenu::IsSourceControlConnected() const
{
	const ISourceControlProvider& Provider = ISourceControlModule::Get().GetProvider();
	return Provider.IsEnabled() && Provider.IsAvailable();
}

/// Prompt to save or discard all packages
bool FPlasticSourceControlMenu::SaveDirtyPackages()
{
	const bool bPromptUserToSave = true;
	const bool bSaveMapPackages = true;
	const bool bSaveContentPackages = true;
	const bool bFastSave = false;
	const bool bNotifyNoPackagesSaved = false;
	const bool bCanBeDeclined = true; // If the user clicks "don't save" this will continue and lose their changes
	bool bHadPackagesToSave = false;

	bool bSaved = FEditorFileUtils::SaveDirtyPackages(bPromptUserToSave, bSaveMapPackages, bSaveContentPackages, bFastSave, bNotifyNoPackagesSaved, bCanBeDeclined, &bHadPackagesToSave);

	// bSaved can be true if the user selects to not save an asset by un-checking it and clicking "save"
	if (bSaved)
	{
		TArray<UPackage*> DirtyPackages;
		FEditorFileUtils::GetDirtyWorldPackages(DirtyPackages);
		FEditorFileUtils::GetDirtyContentPackages(DirtyPackages);
		bSaved = DirtyPackages.Num() == 0;
	}

	return bSaved;
}

/// Find all packages in Content directory
TArray<FString> FPlasticSourceControlMenu::ListAllPackages()
{
	TArray<FString> PackageRelativePaths;
	FPackageName::FindPackagesInDirectory(PackageRelativePaths, FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir()));

	TArray<FString> PackageNames;
	PackageNames.Reserve(PackageRelativePaths.Num());
	for (const FString& Path : PackageRelativePaths)
	{
		FString PackageName;
		FString FailureReason;
		if (FPackageName::TryConvertFilenameToLongPackageName(Path, PackageName, &FailureReason))
		{
			PackageNames.Add(PackageName);
		}
		else
		{
			FMessageLog("SourceControl").Error(FText::FromString(FailureReason));
		}
	}

	return PackageNames;
}

/// Unkink all loaded packages to allow to update them
TArray<UPackage*> FPlasticSourceControlMenu::UnlinkPackages(const TArray<FString>& InPackageNames)
{
	TArray<UPackage*> LoadedPackages;

	// Inspired from ContentBrowserUtils::SyncPathsFromSourceControl()
	if (InPackageNames.Num() > 0)
	{
		// Form a list of loaded packages to reload...
		LoadedPackages.Reserve(InPackageNames.Num());
		for (const FString& PackageName : InPackageNames)
		{
			UPackage* Package = FindPackage(nullptr, *PackageName);
			if (Package)
			{
				LoadedPackages.Emplace(Package);

				// Detach the linkers of any loaded packages so that SCC can overwrite the files...
				if (!Package->IsFullyLoaded())
				{
					FlushAsyncLoading();
					Package->FullyLoad();
				}
				ResetLoaders(Package);
			}
		}
		UE_LOG(LogSourceControl, Log, TEXT("Reseted Loader for %d Packages"), LoadedPackages.Num());
	}

	return LoadedPackages;
}

void FPlasticSourceControlMenu::ReloadPackages(TArray<UPackage*>& InPackagesToReload)
{
	UE_LOG(LogSourceControl, Log, TEXT("Reloading %d Packages..."), InPackagesToReload.Num());

	// Syncing may have deleted some packages, so we need to unload those rather than re-load them...
	TArray<UPackage*> PackagesToUnload;
	InPackagesToReload.RemoveAll([&](UPackage* InPackage) -> bool
	{
		const FString PackageExtension = InPackage->ContainsMap() ? FPackageName::GetMapPackageExtension() : FPackageName::GetAssetPackageExtension();
		const FString PackageFilename = FPackageName::LongPackageNameToFilename(InPackage->GetName(), PackageExtension);
		if (!FPaths::FileExists(PackageFilename))
		{
			PackagesToUnload.Emplace(InPackage);
			return true; // remove package
		}
		return false; // keep package
	});

	// Hot-reload the new packages...
	UPackageTools::ReloadPackages(InPackagesToReload);

	// Unload any deleted packages...
	UPackageTools::UnloadPackages(PackagesToUnload);
}

void FPlasticSourceControlMenu::SyncProjectClicked()
{
	if (!OperationInProgressNotification.IsValid())
	{
		const bool bSaved = SaveDirtyPackages();
		if (bSaved)
		{
			// Find and Unlink all packages in Content directory to allow to update them
			PackagesToReload = UnlinkPackages(ListAllPackages());

			// Launch a "Sync" operation
			FPlasticSourceControlModule& PlasticSourceControl = FPlasticSourceControlModule::Get();
			FPlasticSourceControlProvider& Provider = PlasticSourceControl.GetProvider();
			TSharedRef<FSync, ESPMode::ThreadSafe> SyncOperation = ISourceControlOperation::Create<FSync>();
			const ECommandResult::Type Result = Provider.Execute(SyncOperation, TArray<FString>(), EConcurrency::Asynchronous, FSourceControlOperationComplete::CreateRaw(this, &FPlasticSourceControlMenu::OnSourceControlOperationComplete));
			if (Result == ECommandResult::Succeeded)
			{
				// Display an ongoing notification during the whole operation (packages will be reloaded at the completion of the operation)
				DisplayInProgressNotification(SyncOperation->GetInProgressString());
			}
			else
			{
				// Report failure with a notification and Reload all packages
				DisplayFailureNotification(SyncOperation->GetName());
				ReloadPackages(PackagesToReload);
			}
		}
		else
		{
			FMessageLog SourceControlLog("SourceControl");
			SourceControlLog.Warning(LOCTEXT("SourceControlMenu_Sync_Unsaved", "Save All Assets before attempting to Sync!"));
			SourceControlLog.Notify();
		}
	}
	else
	{
		FMessageLog SourceControlLog("SourceControl");
		SourceControlLog.Warning(LOCTEXT("SourceControlMenu_InProgress", "Source control operation already in progress"));
		SourceControlLog.Notify();
	}
}

void FPlasticSourceControlMenu::RevertUnchangedClicked()
{
	if (!OperationInProgressNotification.IsValid())
	{
		// Launch a "RevertUnchanged" Operation
		FPlasticSourceControlModule& PlasticSourceControl = FPlasticSourceControlModule::Get();
		FPlasticSourceControlProvider& Provider = PlasticSourceControl.GetProvider();
		TSharedRef<FPlasticRevertUnchanged, ESPMode::ThreadSafe> RevertUnchangedOperation = ISourceControlOperation::Create<FPlasticRevertUnchanged>();
		TArray<FString> WorkspaceRoot;
		WorkspaceRoot.Add(Provider.GetPathToWorkspaceRoot()); // Revert the root of the workspace (needs an absolute path)
		const ECommandResult::Type Result = Provider.Execute(RevertUnchangedOperation, WorkspaceRoot, EConcurrency::Asynchronous, FSourceControlOperationComplete::CreateRaw(this, &FPlasticSourceControlMenu::OnSourceControlOperationComplete));
		if (Result == ECommandResult::Succeeded)
		{
			// Display an ongoing notification during the whole operation
			DisplayInProgressNotification(RevertUnchangedOperation->GetInProgressString());
		}
		else
		{
			// Report failure with a notification
			DisplayFailureNotification(RevertUnchangedOperation->GetName());
		}
	}
	else
	{
		FMessageLog SourceControlLog("SourceControl");
		SourceControlLog.Warning(LOCTEXT("SourceControlMenu_InProgress", "Source control operation already in progress"));
		SourceControlLog.Notify();
	}
}

void FPlasticSourceControlMenu::RevertAllClicked()
{
	if (!OperationInProgressNotification.IsValid())
	{
		// Ask the user before reverting all!
		const FText DialogText(LOCTEXT("SourceControlMenu_AskRevertAll", "Revert all modifications into the workspace?"));
		const EAppReturnType::Type Choice = FMessageDialog::Open(EAppMsgType::OkCancel, DialogText);
		if (Choice == EAppReturnType::Ok)
		{
			const bool bSaved = SaveDirtyPackages();
			if (bSaved)
			{
				// Find and Unlink all packages in Content directory to allow to update them
				PackagesToReload = UnlinkPackages(ListAllPackages());

				// Launch a "RevertAll" Operation
				FPlasticSourceControlModule& PlasticSourceControl = FPlasticSourceControlModule::Get();
				FPlasticSourceControlProvider& Provider = PlasticSourceControl.GetProvider();
				TSharedRef<FPlasticRevertAll, ESPMode::ThreadSafe> RevertAllOperation = ISourceControlOperation::Create<FPlasticRevertAll>();
				TArray<FString> WorkspaceRoot;
				WorkspaceRoot.Add(Provider.GetPathToWorkspaceRoot()); // Revert the root of the workspace (needs an absolute path)
				const ECommandResult::Type Result = Provider.Execute(RevertAllOperation, WorkspaceRoot, EConcurrency::Asynchronous, FSourceControlOperationComplete::CreateRaw(this, &FPlasticSourceControlMenu::OnSourceControlOperationComplete));
				if (Result == ECommandResult::Succeeded)
				{
					// Display an ongoing notification during the whole operation
					DisplayInProgressNotification(RevertAllOperation->GetInProgressString());
				}
				else
				{
					// Report failure with a notification and Reload all packages
					DisplayFailureNotification(RevertAllOperation->GetName());
					ReloadPackages(PackagesToReload);
				}
			}
			else
			{
				FMessageLog SourceControlLog("SourceControl");
				SourceControlLog.Warning(LOCTEXT("SourceControlMenu_Sync_Unsaved", "Save All Assets before attempting to Sync!"));
				SourceControlLog.Notify();
			}
		}
	}
	else
	{
		FMessageLog SourceControlLog("SourceControl");
		SourceControlLog.Warning(LOCTEXT("SourceControlMenu_InProgress", "Source control operation already in progress"));
		SourceControlLog.Notify();
	}
}

void FPlasticSourceControlMenu::RefreshClicked()
{
	if (!OperationInProgressNotification.IsValid())
	{
		// Launch an "UpdateStatus" Operation
		FPlasticSourceControlModule& PlasticSourceControl = FPlasticSourceControlModule::Get();
		FPlasticSourceControlProvider& Provider = PlasticSourceControl.GetProvider();
		TSharedRef<FUpdateStatus, ESPMode::ThreadSafe> RefreshOperation = ISourceControlOperation::Create<FUpdateStatus>();
		// This is the flag used by the Content Browser's "Checkout" filter to trigger a full status update
		RefreshOperation->SetGetOpenedOnly(true);
		const ECommandResult::Type Result = Provider.Execute(RefreshOperation, TArray<FString>(), EConcurrency::Asynchronous, FSourceControlOperationComplete::CreateRaw(this, &FPlasticSourceControlMenu::OnSourceControlOperationComplete));
		if (Result == ECommandResult::Succeeded)
		{
			// Display an ongoing notification during the whole operation
			DisplayInProgressNotification(RefreshOperation->GetInProgressString());
		}
		else
		{
			// Report failure with a notification
			DisplayFailureNotification(RefreshOperation->GetName());
		}
	}
	else
	{
		FMessageLog SourceControlLog("SourceControl");
		SourceControlLog.Warning(LOCTEXT("SourceControlMenu_InProgress", "Source control operation already in progress"));
		SourceControlLog.Notify();
	}
}

void FPlasticSourceControlMenu::VisitDocsURLClicked()
{
	// Grab the URL from the uplugin file
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("PlasticSourceControl"));
	if (Plugin.IsValid())
	{
		FPlatformProcess::LaunchURL(*Plugin->GetDescriptor().DocsURL, NULL, NULL);
	}
}

void FPlasticSourceControlMenu::VisitSupportURLClicked()
{
	// Grab the URL from the uplugin file
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("PlasticSourceControl"));
	if (Plugin.IsValid())
	{
		FPlatformProcess::LaunchURL(*Plugin->GetDescriptor().SupportURL, NULL, NULL);
	}
}

// Display an ongoing notification during the whole operation
void FPlasticSourceControlMenu::DisplayInProgressNotification(const FText& InOperationInProgressString)
{
	if (!OperationInProgressNotification.IsValid())
	{
		FNotificationInfo Info(InOperationInProgressString);
		Info.bFireAndForget = false;
		Info.ExpireDuration = 0.0f;
		Info.FadeOutDuration = 1.0f;
		OperationInProgressNotification = FSlateNotificationManager::Get().AddNotification(Info);
		if (OperationInProgressNotification.IsValid())
		{
			OperationInProgressNotification.Pin()->SetCompletionState(SNotificationItem::CS_Pending);
		}
	}
}

// Remove the ongoing notification at the end of the operation
void FPlasticSourceControlMenu::RemoveInProgressNotification()
{
	if (OperationInProgressNotification.IsValid())
	{
		OperationInProgressNotification.Pin()->ExpireAndFadeout();
		OperationInProgressNotification.Reset();
	}
}

// Display a temporary success notification at the end of the operation
void FPlasticSourceControlMenu::DisplaySucessNotification(const FName& InOperationName)
{
	const FText NotificationText = FText::Format(
		LOCTEXT("SourceControlMenu_Success", "{0} operation was successful!"),
		FText::FromName(InOperationName)
	);
	FNotificationInfo Info(NotificationText);
	Info.bUseSuccessFailIcons = true;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
	Info.Image = FAppStyle::GetBrush(TEXT("NotificationList.SuccessImage"));
#else
	Info.Image = FEditorStyle::GetBrush(TEXT("NotificationList.SuccessImage"));
#endif
	FSlateNotificationManager::Get().AddNotification(Info);
	UE_LOG(LogSourceControl, Log, TEXT("%s"), *NotificationText.ToString());
}

// Display a temporary failure notification at the end of the operation
void FPlasticSourceControlMenu::DisplayFailureNotification(const FName& InOperationName)
{
	const FText NotificationText = FText::Format(
		LOCTEXT("SourceControlMenu_Failure", "Error: {0} operation failed!"),
		FText::FromName(InOperationName)
	);
	FNotificationInfo Info(NotificationText);
	Info.ExpireDuration = 8.0f;
	FSlateNotificationManager::Get().AddNotification(Info);
	UE_LOG(LogSourceControl, Error, TEXT("%s"), *NotificationText.ToString());
}

void FPlasticSourceControlMenu::OnSourceControlOperationComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult)
{
	RemoveInProgressNotification();

	if ((InOperation->GetName() == "Sync") || (InOperation->GetName() == "RevertAll"))
	{
		// Reload packages that where unlinked at the beginning of the Sync operation
		ReloadPackages(PackagesToReload);
	}

	// Report result with a notification
	if (InResult == ECommandResult::Succeeded)
	{
		DisplaySucessNotification(InOperation->GetName());
	}
	else
	{
		DisplayFailureNotification(InOperation->GetName());
	}
}

#if ENGINE_MAJOR_VERSION == 4
void FPlasticSourceControlMenu::AddMenuExtension(FMenuBuilder& Menu)
#elif ENGINE_MAJOR_VERSION == 5
void FPlasticSourceControlMenu::AddMenuExtension(FToolMenuSection& Menu)
#endif
{
	Menu.AddMenuEntry(
#if ENGINE_MAJOR_VERSION == 5
		"PlasticSync",
#endif
		LOCTEXT("PlasticSync",			"Sync/Update Workspace"),
		LOCTEXT("PlasticSyncTooltip",	"Update all files in the workspace to the latest version."),
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "SourceControl.Actions.Sync"),
#else
		FSlateIcon(FEditorStyle::GetStyleSetName(), "SourceControl.Actions.Sync"),
#endif
		FUIAction(
			FExecuteAction::CreateRaw(this, &FPlasticSourceControlMenu::SyncProjectClicked),
			FCanExecuteAction::CreateRaw(this, &FPlasticSourceControlMenu::IsSourceControlConnected)
		)
	);

	Menu.AddMenuEntry(
#if ENGINE_MAJOR_VERSION == 5
		"PlasticRevertUnchanged",
#endif
		LOCTEXT("PlasticRevertUnchanged",			"Revert Unchanged"),
		LOCTEXT("PlasticRevertUnchangedTooltip",	"Revert checked-out but unchanged files in the workspace."),
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "SourceControl.Actions.Revert"),
#else
		FSlateIcon(FEditorStyle::GetStyleSetName(), "SourceControl.Actions.Revert"),
#endif
		FUIAction(
			FExecuteAction::CreateRaw(this, &FPlasticSourceControlMenu::RevertUnchangedClicked),
			FCanExecuteAction()
		)
	);

	Menu.AddMenuEntry(
#if ENGINE_MAJOR_VERSION == 5
		"PlasticRevertAll",
#endif
		LOCTEXT("PlasticRevertAll",			"Revert All"),
		LOCTEXT("PlasticRevertAllTooltip",	"Revert all files in the workspace to their controlled/unchanged state."),
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "SourceControl.Actions.Revert"),
#else
		FSlateIcon(FEditorStyle::GetStyleSetName(), "SourceControl.Actions.Revert"),
#endif
		FUIAction(
			FExecuteAction::CreateRaw(this, &FPlasticSourceControlMenu::RevertAllClicked),
			FCanExecuteAction()
		)
	);

	Menu.AddMenuEntry(
#if ENGINE_MAJOR_VERSION == 5
		"PlasticRefresh",
#endif
		LOCTEXT("PlasticRefresh",			"Refresh"),
		LOCTEXT("PlasticRefreshTooltip",	"Update the source control status of all files in the workspace."),
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "SourceControl.Actions.Refresh"),
#else
		FSlateIcon(FEditorStyle::GetStyleSetName(), "SourceControl.Actions.Refresh"),
#endif
		FUIAction(
			FExecuteAction::CreateRaw(this, &FPlasticSourceControlMenu::RefreshClicked),
			FCanExecuteAction()
		)
	);

	Menu.AddMenuEntry(
#if ENGINE_MAJOR_VERSION == 5
		"PlasticDocsURL",
#endif
		LOCTEXT("PlasticDocsURL",			"Plugin's Documentation"),
		LOCTEXT("PlasticDocsURLTooltip",	"Visit documentation of the plugin on Github."),
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Documentation"),
#else
		FSlateIcon(FEditorStyle::GetStyleSetName(), "Icons.Documentation"),
#endif
		FUIAction(
			FExecuteAction::CreateRaw(this, &FPlasticSourceControlMenu::VisitDocsURLClicked),
			FCanExecuteAction()
		)
	);

	Menu.AddMenuEntry(
#if ENGINE_MAJOR_VERSION == 5
		"PlasticSupportURL",
#endif
		LOCTEXT("PlasticSupportURL",		"Plastic SCM Support"),
		LOCTEXT("PlasticSupportURLTooltip",	"Visit official support for Plastic SCM."),
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Support"),
#else
		FSlateIcon(FEditorStyle::GetStyleSetName(), "Icons.Support"),
#endif
		FUIAction(
			FExecuteAction::CreateRaw(this, &FPlasticSourceControlMenu::VisitSupportURLClicked),
			FCanExecuteAction()
		)
	);
}

#if ENGINE_MAJOR_VERSION == 4
TSharedRef<FExtender> FPlasticSourceControlMenu::OnExtendLevelEditorViewMenu(const TSharedRef<FUICommandList> CommandList)
{
	TSharedRef<FExtender> Extender(new FExtender());

	Extender->AddMenuExtension(
		"SourceControlActions",
		EExtensionHook::After,
		nullptr,
		FMenuExtensionDelegate::CreateRaw(this, &FPlasticSourceControlMenu::AddMenuExtension));

	return Extender;
}
#endif

#undef LOCTEXT_NAMESPACE
