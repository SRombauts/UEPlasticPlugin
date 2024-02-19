// Copyright (c) 2024 Unity Technologies

#include "PlasticSourceControlBranchesWindow.h"

#include "Widgets/Docking/SDockTab.h"

#include "PlasticSourceControlStyle.h"
#include "SPlasticSourceControlBranchesWidget.h"

#define LOCTEXT_NAMESPACE "PlasticSourceControlBranchesWindow"

static const FName PlasticSourceControlBranchesWindowTabName("PlasticSourceControlBranchesWindow");

void FPlasticSourceControlBranchesWindow::Register()
{
	FPlasticSourceControlStyle::Initialize();
	FPlasticSourceControlStyle::ReloadTextures();

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(PlasticSourceControlBranchesWindowTabName, FOnSpawnTab::CreateRaw(this, &FPlasticSourceControlBranchesWindow::OnSpawnTab))
		.SetDisplayName(LOCTEXT("PlasticSourceControlBranchesWindowTabTitle", "View Branches"))
		.SetMenuType(ETabSpawnerMenuType::Hidden)
		.SetIcon(FSlateIcon(FPlasticSourceControlStyle::Get().GetStyleSetName(), "PlasticSourceControl.PluginIcon.Small"));
}

void FPlasticSourceControlBranchesWindow::Unregister()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(PlasticSourceControlBranchesWindowTabName);

	FPlasticSourceControlStyle::Shutdown();
}

TSharedRef<SDockTab> FPlasticSourceControlBranchesWindow::OnSpawnTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			CreateBranchesWidget().ToSharedRef()
		];
}

void FPlasticSourceControlBranchesWindow::OpenTab()
{
	FGlobalTabmanager::Get()->TryInvokeTab(PlasticSourceControlBranchesWindowTabName);
}

TSharedPtr<SWidget> FPlasticSourceControlBranchesWindow::CreateBranchesWidget()
{
	return SNew(SPlasticSourceControlBranchesWidget);
}

#undef LOCTEXT_NAMESPACE
