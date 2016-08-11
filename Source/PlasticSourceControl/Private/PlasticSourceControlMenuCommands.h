// Copyright (c) 2016 Codice Software - Sebastien Rombauts (sebastien.rombauts@gmail.com)

#pragma once

#include "SlateBasics.h"
#include "PlasticSourceControlMenuStyle.h"

class FPlasticSourceControlMenuCommands : public TCommands<FPlasticSourceControlMenuCommands>
{
public:

	FPlasticSourceControlMenuCommands()
		: TCommands<FPlasticSourceControlMenuCommands>(
			TEXT("PlasticSourceControlMenu"), // Context name for fast lookup
			NSLOCTEXT("Contexts", "PlasticSourceControlMenu", "PlasticSCM Plugin Menu"), // Localized context name for displaying
			NAME_None, // Parent
			FPlasticSourceControlMenuStyle::GetStyleSetName() // Icon Style Set
			)
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr<FUICommandInfo> SyncProject;
	TSharedPtr<FUICommandInfo> RevertUnchanged;
	TSharedPtr<FUICommandInfo> RevertAll;
};
