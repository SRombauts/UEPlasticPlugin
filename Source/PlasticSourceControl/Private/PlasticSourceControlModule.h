// Copyright (c) 2023 Unity Technologies

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "PlasticSourceControlProvider.h"
#include "PlasticSourceControlWorkspaceCreation.h"

/**
 * PlasticSourceControl is the official Unity Version Control Plugin for Unreal Engine
 *
 * Written and contributed by Sebastien Rombauts (sebastien.rombauts@gmail.com) for Codice Software
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
	const FPlasticSourceControlProvider& GetProvider() const
	{
		return PlasticSourceControlProvider;
	}

	/** Access the controller to create a new workspace */
	FPlasticSourceControlWorkspaceCreation& GetWorkspaceCreation()
	{
		return PlasticSourceControlWorkspaceCreation;
	}

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, asserts if the module is not loaded yet or unloaded already.
	 */
	static inline FPlasticSourceControlModule& Get()
	{
		return FModuleManager::GetModuleChecked<FPlasticSourceControlModule>("PlasticSourceControl");
	}

	/**
	 * Checks whether the module is currently loaded.
	 */
	static inline bool IsLoaded()
	{
		return FModuleManager::Get().IsModuleLoaded("PlasticSourceControl");
	}

private:
	/** The Plastic source control provider */
	FPlasticSourceControlProvider PlasticSourceControlProvider;

	/** Logic to create a new workspace */
	FPlasticSourceControlWorkspaceCreation PlasticSourceControlWorkspaceCreation;
};
