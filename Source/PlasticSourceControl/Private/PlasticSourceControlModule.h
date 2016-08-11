// Copyright (c) 2016 Codice Software - Sebastien Rombauts (sebastien.rombauts@gmail.com)

#pragma once

#include "ModuleInterface.h"
#include "PlasticSourceControlSettings.h"
#include "PlasticSourceControlProvider.h"
#include "PlasticSourceControlMenu.h"

/**
 * UE4PlasticPlugin is a simple Plastic SCM Source Control Plugin for Unreal Engine 4
 *
 * Written and contributed by Sebastien Rombauts (sebastien.rombauts@gmail.com)
 */
class FPlasticSourceControlModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Access the Plastic source control settings */
	FPlasticSourceControlSettings& AccessSettings()
	{
		return PlasticSourceControlSettings;
	}

	/** Save the Plastic source control settings */
	void SaveSettings();

	/** Access the Plastic source control provider */
	FPlasticSourceControlProvider& GetProvider()
	{
		return PlasticSourceControlProvider;
	}

private:
	/** The Plastic source control provider */
	FPlasticSourceControlProvider PlasticSourceControlProvider;

	/** The settings for Plastic source control */
	FPlasticSourceControlSettings PlasticSourceControlSettings;

	/** Source Control Menu Extension */
	FPlasticSourceControlMenu PlasticSourceControlMenu;
};