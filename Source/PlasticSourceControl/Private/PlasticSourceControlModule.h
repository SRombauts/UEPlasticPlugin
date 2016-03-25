// Copyright (c) 2016 Codice Software - Sebastien Rombauts (sebastien.rombauts@gmail.com)

#pragma once

#include "ISourceControlModule.h"
#include "PlasticSourceControlProvider.h"

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

	/** Access the Plastic source control provider */
	FPlasticSourceControlProvider& GetProvider()
	{
		return PlasticSourceControlProvider;
	}

private:
	/** The Plastic source control provider */
	FPlasticSourceControlProvider PlasticSourceControlProvider;
};