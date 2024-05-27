// Copyright (c) 2024 Unity Technologies

#include "PlasticSourceControlBranchesWindow.h"

#include "Framework/Docking/TabManager.h"
#include "RevisionControlStyle/RevisionControlStyle.h"
#include "Widgets/Docking/SDockTab.h"

#include "SPlasticSourceControlBranchesWidget.h"

#define LOCTEXT_NAMESPACE "PlasticSourceControlBranchesWindow"

static const FName PlasticSourceControlBranchesWindowTabName("PlasticSourceControlBranchesWindow");

void FPlasticSourceControlBranchesWindow::Register()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(PlasticSourceControlBranchesWindowTabName, FOnSpawnTab::CreateRaw(this, &FPlasticSourceControlBranchesWindow::OnSpawnTab))
		.SetDisplayName(LOCTEXT("PlasticSourceControlBranchesWindowTabTitle", "View Branches"))
		.SetMenuType(ETabSpawnerMenuType::Hidden)
		.SetIcon(FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.Branched"));
}

void FPlasticSourceControlBranchesWindow::Unregister()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(PlasticSourceControlBranchesWindowTabName);
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
