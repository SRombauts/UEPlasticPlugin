// Copyright (c) 2016-2017 Codice Software - Sebastien Rombauts (sebastien.rombauts@gmail.com)

#include "PlasticSourceControlPrivatePCH.h"
#include "PlasticSourceControlSettings.h"
#include "PlasticSourceControlModule.h"
#include "PlasticSourceControlProvider.h"
#include "PlasticSourceControlUtils.h"
#include "SourceControlHelpers.h"

#include "Misc/ScopeLock.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/PlatformTime.h"

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

// This is called at startup nearly before anything else in our module: BinaryPath will then be used by the provider
void FPlasticSourceControlSettings::LoadSettings()
{
	FScopeLock ScopeLock(&CriticalSection);
	const FString& IniFile = SourceControlHelpers::GetSettingsIni();
	bool bLoaded = GConfig->GetString(*PlasticSettingsConstants::SettingsSection, TEXT("BinaryPath"), BinaryPath, IniFile);
	if(!bLoaded || BinaryPath.IsEmpty())
	{
		BinaryPath = PlasticSourceControlUtils::FindPlasticBinaryPath();
	}
}

void FPlasticSourceControlSettings::SaveSettings() const
{
	FScopeLock ScopeLock(&CriticalSection);

	// Re-Check provided Plastic binary path for each change
	FPlasticSourceControlModule& PlasticSourceControl = FModuleManager::LoadModuleChecked<FPlasticSourceControlModule>("PlasticSourceControl");
	PlasticSourceControl.GetProvider().CheckPlasticAvailability();
	if (PlasticSourceControl.GetProvider().IsPlasticAvailable())
	{
		const FString& IniFile = SourceControlHelpers::GetSettingsIni();
		GConfig->SetString(*PlasticSettingsConstants::SettingsSection, TEXT("BinaryPath"), *BinaryPath, IniFile);
	}
}