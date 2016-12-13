// Copyright (c) 2016-2017 Codice Software - Sebastien Rombauts (sebastien.rombauts@gmail.com)

#pragma once

#include "CoreMinimal.h"

class FPlasticSourceControlSettings
{
public:
	/** Get the Plastic Binary Path */
	const FString GetBinaryPath() const;

	/** Set the Plastic Binary Path */
	bool SetBinaryPath(const FString& InString);

	/** Get the Root of the Plastic SCM Workspace */
	const FString& GetWorkspaceRoot() const;

	/** Set the Root of the Plastic SCM Workspace */
	bool SetWorkspaceRoot(const FString& InString);

	/** Load settings from ini file */
	void LoadSettings();

	/** Save settings to ini file */
	void SaveSettings() const;

private:
	/** A critical section for settings access */
	mutable FCriticalSection CriticalSection;

	/** Path to the Plastic SCM cli executable */
	FString BinaryPath;

	/** Root of the Plastic SCM workspace: can be the GameDir itself, any parent directory, or a configured subdirectory */
	FString WorkspaceRoot;
};