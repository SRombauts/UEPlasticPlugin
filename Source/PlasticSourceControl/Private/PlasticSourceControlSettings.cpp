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

const FString FPlasticSourceControlSettings::GetBinaryPath() const
{
	FScopeLock ScopeLock(&CriticalSection);
	return BinaryPath;
}

bool FPlasticSourceControlSettings::SetBinaryPath(const FString& InString)
{
	FScopeLock ScopeLock(&CriticalSection);
	const bool bChanged = (BinaryPath != InString);
	if(bChanged)
	{
		BinaryPath = InString;
	}
	return bChanged;
}

const FString& FPlasticSourceControlSettings::GetWorkspaceRoot() const
{
	FScopeLock ScopeLock(&CriticalSection);
	return WorkspaceRoot;
}

bool FPlasticSourceControlSettings::SetWorkspaceRoot(const FString& InString)
{
	FScopeLock ScopeLock(&CriticalSection);
	const bool bChanged = (WorkspaceRoot != InString);
	if(bChanged)
	{
		WorkspaceRoot = InString;
	}
	return bChanged;
}

// This is called at startup nearly before anything else in our module: BinaryPath will then be used by the provider
void FPlasticSourceControlSettings::LoadSettings()
{
	FScopeLock ScopeLock(&CriticalSection);
	const FString& IniFile = SourceControlHelpers::GetSettingsIni();
	GConfig->GetString(*PlasticSettingsConstants::SettingsSection, TEXT("BinaryPath"), BinaryPath, IniFile);
}

void FPlasticSourceControlSettings::SaveSettings() const
{
	FScopeLock ScopeLock(&CriticalSection);
	const FString& IniFile = SourceControlHelpers::GetSettingsIni();
	GConfig->SetString(*PlasticSettingsConstants::SettingsSection, TEXT("BinaryPath"), *BinaryPath, IniFile);
}