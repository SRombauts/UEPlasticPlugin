// Copyright (c) 2016 Codice Software - Sebastien Rombauts (sebastien.rombauts@gmail.com)

#include "PlasticSourceControlPrivatePCH.h"
#include "PlasticSourceControlSettings.h"
#include "PlasticSourceControlModule.h"
#include "PlasticSourceControlProvider.h"
#include "PlasticSourceControlUtils.h"
#include "SourceControlHelpers.h"

namespace PlasticSettingsConstants
{

/** The section of the ini file we load our settings from */
static const FString SettingsSection = TEXT("PlasticSourceControl.PlasticSourceControlSettings");

}

const FString& FPlasticSourceControlSettings::GetBinaryPath() const
{
	FScopeLock ScopeLock(&CriticalSection);
	return BinaryPath;
}

void FPlasticSourceControlSettings::SetBinaryPath(const FString& InString)
{
	FScopeLock ScopeLock(&CriticalSection);
	BinaryPath = InString;
}

const FString& FPlasticSourceControlSettings::GetWorkspaceRoot() const
{
	FScopeLock ScopeLock(&CriticalSection);
	return WorkspaceRoot;
}

void FPlasticSourceControlSettings::SetWorkspaceRoot(const FString& InString)
{
	FScopeLock ScopeLock(&CriticalSection);
	WorkspaceRoot = InString;
}

// This is called at startup nearly before anything else in our module: BinaryPath will then be used by the provider.
void FPlasticSourceControlSettings::LoadSettings()
{
	FScopeLock ScopeLock(&CriticalSection);
	const FString& IniFile = SourceControlHelpers::GetSettingsIni();
	bool bLoaded = GConfig->GetString(*PlasticSettingsConstants::SettingsSection, TEXT("BinaryPath"), BinaryPath, IniFile);
	if(!bLoaded || BinaryPath.IsEmpty())
	{
		BinaryPath = PlasticSourceControlUtils::FindPlasticBinaryPath();
	}
	bLoaded = GConfig->GetString(*PlasticSettingsConstants::SettingsSection, TEXT("WorkspaceRoot"), WorkspaceRoot, IniFile);
	if (!bLoaded || WorkspaceRoot.IsEmpty())
	{
		// Find the path to the root directory of the Plastic SCM workpsace (if any, else uses the GameDir)
		const FString PathToGameDir = FPaths::ConvertRelativePathToFull(FPaths::GameDir());
		PlasticSourceControlUtils::FindRootDirectory(PathToGameDir, WorkspaceRoot);
	}
}

void FPlasticSourceControlSettings::SaveSettings() const
{
	FScopeLock ScopeLock(&CriticalSection);

	FPlasticSourceControlModule& PlasticSourceControl = FModuleManager::LoadModuleChecked<FPlasticSourceControlModule>("PlasticSourceControl");

	// Re-Init cm shell and Re-Check provided Plastic binary path for each change
	PlasticSourceControl.GetProvider().Close();
	PlasticSourceControl.GetProvider().CheckPlasticAvailability();
	if (PlasticSourceControl.GetProvider().IsEnabled())
	{
		const FString& IniFile = SourceControlHelpers::GetSettingsIni();
		GConfig->SetString(*PlasticSettingsConstants::SettingsSection, TEXT("BinaryPath"), *BinaryPath, IniFile);
		GConfig->SetString(*PlasticSettingsConstants::SettingsSection, TEXT("WorkspaceRoot"), *WorkspaceRoot, IniFile);
	}
}