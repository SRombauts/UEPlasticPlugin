// Copyright (c) 2016 Codice Software - Sebastien Rombauts (sebastien.rombauts@gmail.com)

#pragma once

class FPlasticSourceControlSettings
{
public:
	/** Get the Path to the Plastic SCM command line Binary */
	const FString& GetBinaryPath() const;

	/** Set the Plastic Binary Path */
	void SetBinaryPath(const FString& InString);

	/** Get the Root of the Plastic SCM Workspace */
	const FString& GetWorkspaceRoot() const;

	/** Set the Plastic Binary Path */
	void SetWorkspaceRoot(const FString& InString);

	/** Load settings from ini file */
	void LoadSettings();

	/** Save settings to ini file */
	void SaveSettings() const;

private:
	/** A critical section for settings access */
	mutable FCriticalSection CriticalSection;

	/** Plastic SCM path to binary */
	FString BinaryPath;

	/** Root of the Plastic SCM workspace */
	FString WorkspaceRoot;
};