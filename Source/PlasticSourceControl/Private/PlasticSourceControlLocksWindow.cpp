// Copyright (c) 2024 Unity Technologies

#include "PlasticSourceControlLocksWindow.h"

#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"

#include "PlasticSourceControlStyle.h"
#include "SPlasticSourceControlLocksWidget.h"

#define LOCTEXT_NAMESPACE "PlasticSourceControlLocksWindow"

static const FName PlasticSourceControlLocksWindowTabName("PlasticSourceControlLocksWindow");

void FPlasticSourceControlLocksWindow::Register()
{
	FPlasticSourceControlStyle::Initialize();
	FPlasticSourceControlStyle::ReloadTextures();

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(PlasticSourceControlLocksWindowTabName, FOnSpawnTab::CreateRaw(this, &FPlasticSourceControlLocksWindow::OnSpawnTab))
		.SetDisplayName(LOCTEXT("PlasticSourceControlLocksWindowTabTitle", "View Locks"))
		.SetMenuType(ETabSpawnerMenuType::Hidden)
		.SetIcon(FSlateIcon(FPlasticSourceControlStyle::Get().GetStyleSetName(), "PlasticSourceControl.PluginIcon.Small"));
}

void FPlasticSourceControlLocksWindow::Unregister()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(PlasticSourceControlLocksWindowTabName);

	FPlasticSourceControlStyle::Shutdown();
}

TSharedRef<SDockTab> FPlasticSourceControlLocksWindow::OnSpawnTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			CreateLocksWidget().ToSharedRef()
		];
}

void FPlasticSourceControlLocksWindow::OpenTab()
{
	FGlobalTabmanager::Get()->TryInvokeTab(PlasticSourceControlLocksWindowTabName);
}

TSharedPtr<SWidget> FPlasticSourceControlLocksWindow::CreateLocksWidget()
{
	return SNew(SPlasticSourceControlLocksWidget);
}

#undef LOCTEXT_NAMESPACE
