// Copyright (c) 2016-2017 Codice Software - Sebastien Rombauts (sebastien.rombauts@gmail.com)

#pragma once

class FPlasticSourceControlSettings
{
public:
	/** Get the Plastic Binary Path */
	const FString& GetBinaryPath() const;

	/** Set the Plastic Binary Path */
	void SetBinaryPath(const FString& InString);

	/** Load settings from ini file */
	void LoadSettings();

	/** Save settings to ini file */
	void SaveSettings() const;

private:
	/** A critical section for settings access */
	mutable FCriticalSection CriticalSection;

	/** Plastic binary path */
	FString BinaryPath;
};