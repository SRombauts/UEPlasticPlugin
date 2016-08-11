// Copyright (c) 2016 Codice Software - Sebastien Rombauts (sebastien.rombauts@gmail.com)

#include "PlasticSourceControlPrivatePCH.h"
#include "PlasticSourceControlMenuCommands.h"

#define LOCTEXT_NAMESPACE "PlasticSourceControl"

void FPlasticSourceControlMenuCommands::RegisterCommands()
{
	UI_COMMAND(SyncProject,		"Sync",				"Update all file in the workspace to the latest version.", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(RevertUnchanged,	"Revert Unchanged",	"Revert checked-out but unchanged files in the workspace.",	EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(RevertAll,		"Revert All",		"Revert all files in the workspace to the state before they where checked out.", EUserInterfaceActionType::Button, FInputGesture());
}

#undef LOCTEXT_NAMESPACE
