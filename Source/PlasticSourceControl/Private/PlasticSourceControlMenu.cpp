// Copyright Unity Technologies

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
#include "Misc/MessageDialog.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "Styling/AppStyle.h"
#else
#include "EditorStyleSet.h"
#endif

#include "PackageUtils.h"
#include "FileHelpers.h"
#include "ISettingsModule.h"

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
	TArray<FString> PackageFilePaths;
	FPackageName::FindPackagesInDirectory(PackageFilePaths, FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir()));
	return PackageFilePaths;
}

void FPlasticSourceControlMenu::SyncProjectClicked()
{
	if (!OperationInProgressNotification.IsValid())
	{
		const bool bSaved = SaveDirtyPackages();
		if (bSaved)
		{
			// Find and Unlink all packages in Content directory to allow to update them
			PackageUtils::UnlinkPackages(ListAllPackages());

			// Launch a custom "SyncAll" operation
			FPlasticSourceControlProvider& Provider = FPlasticSourceControlModule::Get().GetProvider();
			TSharedRef<FPlasticSyncAll, ESPMode::ThreadSafe> SyncOperation = ISourceControlOperation::Create<FPlasticSyncAll>();
			const ECommandResult::Type Result = Provider.Execute(SyncOperation, TArray<FString>(), EConcurrency::Asynchronous, FSourceControlOperationComplete::CreateRaw(this, &FPlasticSourceControlMenu::OnSourceControlOperationComplete));
			if (Result == ECommandResult::Succeeded)
			{
				// Display an ongoing notification during the whole operation (packages will be reloaded at the completion of the operation)
				DisplayInProgressNotification(SyncOperation->GetInProgressString());
			}
			else
			{
				// Report failure with a notification (but nothing need to be reloaded since no local change is expected)
				DisplayFailureNotification(SyncOperation->GetName());
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
		FPlasticSourceControlProvider& Provider = FPlasticSourceControlModule::Get().GetProvider();
		TSharedRef<FPlasticRevertUnchanged, ESPMode::ThreadSafe> RevertUnchangedOperation = ISourceControlOperation::Create<FPlasticRevertUnchanged>();
		const ECommandResult::Type Result = Provider.Execute(RevertUnchangedOperation, TArray<FString>(), EConcurrency::Asynchronous, FSourceControlOperationComplete::CreateRaw(this, &FPlasticSourceControlMenu::OnSourceControlOperationComplete));
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
				PackageUtils::UnlinkPackages(ListAllPackages());

				// Launch a "RevertAll" Operation
				FPlasticSourceControlProvider& Provider = FPlasticSourceControlModule::Get().GetProvider();
				TSharedRef<FPlasticRevertAll, ESPMode::ThreadSafe> RevertAllOperation = ISourceControlOperation::Create<FPlasticRevertAll>();
				const ECommandResult::Type Result = Provider.Execute(RevertAllOperation, TArray<FString>(), EConcurrency::Asynchronous, FSourceControlOperationComplete::CreateRaw(this, &FPlasticSourceControlMenu::OnSourceControlOperationComplete));
				if (Result == ECommandResult::Succeeded)
				{
					// Display an ongoing notification during the whole operation
					DisplayInProgressNotification(RevertAllOperation->GetInProgressString());
				}
				else
				{
					// Report failure with a notification (but nothing need to be reloaded since no local change is expected)
					DisplayFailureNotification(RevertAllOperation->GetName());
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
		FPlasticSourceControlProvider& Provider = FPlasticSourceControlModule::Get().GetProvider();
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

void FPlasticSourceControlMenu::ShowSourceControlEditorPreferences() const
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->ShowViewer("Editor", "General", "LoadingSaving");
	}
}

void FPlasticSourceControlMenu::ShowSourceControlProjectSettings() const
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->ShowViewer("Project", "Editor", "SourceControlPreferences");
	}
}

void FPlasticSourceControlMenu::ShowSourceControlPlasticScmProjectSettings() const
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->ShowViewer("Project", "Editor", "PlasticSourceControlProjectSettings");
	}
}

void FPlasticSourceControlMenu::VisitDocsURLClicked() const
{
	// Grab the URL from the uplugin file
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("PlasticSourceControl"));
	if (Plugin.IsValid())
	{
		FPlatformProcess::LaunchURL(*Plugin->GetDescriptor().DocsURL, NULL, NULL);
	}
}

void FPlasticSourceControlMenu::VisitSupportURLClicked() const
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

	if (InOperation->GetName() == "SyncAll")
	{
		// Reload packages that where updated by the Sync operation (and the current map if needed)
		TSharedRef<FPlasticSyncAll, ESPMode::ThreadSafe> Operation = StaticCastSharedRef<FPlasticSyncAll>(InOperation);
		PackageUtils::ReloadPackages(Operation->UpdatedFiles);
	}
	else if (InOperation->GetName() == "RevertAll")
	{
		// Reload packages that where updated by the Revert operation (and the current map if needed)
		TSharedRef<FPlasticRevertAll, ESPMode::ThreadSafe> Operation = StaticCastSharedRef<FPlasticRevertAll>(InOperation);
		PackageUtils::ReloadPackages(Operation->UpdatedFiles);
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
		LOCTEXT("PlasticSyncTooltip",	"Update the workspace to the latest changeset of the branch, and reload all affected assets."),
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "SourceControl.Actions.Sync"),
#else
		FSlateIcon(FEditorStyle::GetStyleSetName(), "SourceControl.Actions.Sync"),
#endif
		FUIAction(
			FExecuteAction::CreateRaw(this, &FPlasticSourceControlMenu::SyncProjectClicked),
			FCanExecuteAction()
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
		LOCTEXT("PlasticRefreshTooltip",	"Update the local source control status of all files in the workspace (no expensive checks for locks or changes on other branches)."),
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

#if ENGINE_MAJOR_VERSION == 5
	Menu.AddMenuEntry(
		"SourceControlEditorPreferences",
		LOCTEXT("SourceControlEditorPreferences", "Editor Preferences - Source Control"),
		LOCTEXT("SourceControlEditorPreferencesTooltip", "Open the Load & Save section with Source Control in the Editor Preferences."),
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorPreferences.TabIcon"),
#else
		FSlateIcon(FEditorStyle::GetStyleSetName(), "EditorPreferences.TabIcon"),
#endif
		FUIAction(
			FExecuteAction::CreateRaw(this, &FPlasticSourceControlMenu::ShowSourceControlEditorPreferences),
			FCanExecuteAction()
		)
	);
#endif

#if ENGINE_MAJOR_VERSION == 5
	Menu.AddMenuEntry(
		"SourceControlProjectSettings",
		LOCTEXT("SourceControlProjectSettings",			"Project Settings - Source Control"),
		LOCTEXT("SourceControlProjectSettingsTooltip",	"Open the Source Control section in the Project Settings."),
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "ProjectSettings.TabIcon"),
#else
		FSlateIcon(FEditorStyle::GetStyleSetName(), "ProjectSettings.TabIcon"),
#endif
		FUIAction(
			FExecuteAction::CreateRaw(this, &FPlasticSourceControlMenu::ShowSourceControlProjectSettings),
			FCanExecuteAction()
		)
	);
#endif

	Menu.AddMenuEntry(
#if ENGINE_MAJOR_VERSION == 5
		"PlasticProjectSettings",
#endif
		LOCTEXT("PlasticProjectSettings",			"Project Settings - Source Control - Plastic SCM"),
		LOCTEXT("PlasticProjectSettingsTooltip",	"Open the Plastic SCM section in the Project Settings."),
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "ProjectSettings.TabIcon"),
#else
		FSlateIcon(FEditorStyle::GetStyleSetName(), "ProjectSettings.TabIcon"),
#endif
		FUIAction(
			FExecuteAction::CreateRaw(this, &FPlasticSourceControlMenu::ShowSourceControlPlasticScmProjectSettings),
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
#elif ENGINE_MAJOR_VERSION == 5
		FSlateIcon(FEditorStyle::GetStyleSetName(), "Icons.Documentation"),
#elif ENGINE_MAJOR_VERSION == 4
		FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.BrowseDocumentation"),
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
#elif ENGINE_MAJOR_VERSION == 5
		FSlateIcon(FEditorStyle::GetStyleSetName(), "Icons.Support"),
#elif ENGINE_MAJOR_VERSION == 4
		FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.BrowseDocumentation"),
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
