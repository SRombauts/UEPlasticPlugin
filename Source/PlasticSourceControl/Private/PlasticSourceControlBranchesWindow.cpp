// Copyright (c) 2023 Unity Technologies

#include "PlasticSourceControlBranchesWindow.h"

#include "Widgets/Docking/SDockTab.h"

#include "PlasticSourceControlStyle.h"
#include "SPlasticSourceControlBranchesWidget.h"

#define LOCTEXT_NAMESPACE "PlasticSourceControlWindow"

static const FName PlasticSourceControlWindowTabName("PlasticSourceControlWindow");

void FPlasticSourceControlBranchesWindow::Register()
{
	FPlasticSourceControlStyle::Initialize();
	FPlasticSourceControlStyle::ReloadTextures();

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(PlasticSourceControlWindowTabName, FOnSpawnTab::CreateRaw(this, &FPlasticSourceControlBranchesWindow::OnSpawnTab))
		.SetDisplayName(LOCTEXT("PlasticSourceControlWindowTabTitle", "View Branches"))
		.SetMenuType(ETabSpawnerMenuType::Hidden)
	.SetIcon(FSlateIcon(FPlasticSourceControlStyle::Get().GetStyleSetName(), "PlasticSourceControl.PluginIcon.Small"));
}

void FPlasticSourceControlBranchesWindow::Unregister()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(PlasticSourceControlWindowTabName);

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
	FGlobalTabmanager::Get()->TryInvokeTab(PlasticSourceControlWindowTabName);
}

TSharedPtr<SWidget> FPlasticSourceControlBranchesWindow::CreateBranchesWidget()
{
	return SNew(SPlasticSourceControlBranchesWidget);
}

#undef LOCTEXT_NAMESPACE
