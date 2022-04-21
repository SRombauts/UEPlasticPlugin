// Copyright (c) 2016-2022 Codice Software

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "PlasticSourceControlSettings.h"
#include "PlasticSourceControlProvider.h"

/**
 * PlasticSourceControl is a simple Plastic SCM Source Control Plugin for Unreal Engine
 *
 * Written and contributed by Sebastien Rombauts (sebastien.rombauts@gmail.com) for Codice Software
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
	const FPlasticSourceControlSettings& AccessSettings() const
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
	const FPlasticSourceControlProvider& GetProvider() const
	{
		return PlasticSourceControlProvider;
	}

private:
	/** The Plastic source control provider */
	FPlasticSourceControlProvider PlasticSourceControlProvider;

	/** The settings for Plastic source control */
	FPlasticSourceControlSettings PlasticSourceControlSettings;
};