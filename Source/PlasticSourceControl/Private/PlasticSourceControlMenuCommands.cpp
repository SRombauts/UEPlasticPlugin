// Copyright (c) 2016-2017 Codice Software - Sebastien Rombauts (sebastien.rombauts@gmail.com)

#include "PlasticSourceControlPrivatePCH.h"
#include "PlasticSourceControlMenuCommands.h"

#define LOCTEXT_NAMESPACE "PlasticSourceControl"

void FPlasticSourceControlMenuCommands::RegisterCommands()
{
	UI_COMMAND(SyncProject,		"Sync/Update Workspace",	"Update all files in the workspace to the latest version.", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(RevertUnchanged,	"Revert Unchanged",			"Revert checked-out but unchanged files in the workspace.",	EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(RevertAll,		"Revert All",				"Revert all files in the workspace to their controlled/unchanged state.", EUserInterfaceActionType::Button, FInputGesture());
}

#undef LOCTEXT_NAMESPACE
