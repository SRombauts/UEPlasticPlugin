// Copyright (c) 2016 Codice Software - Sebastien Rombauts (sebastien.rombauts@gmail.com)

#pragma once

#include "ISourceControlModule.h"

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

private:
};