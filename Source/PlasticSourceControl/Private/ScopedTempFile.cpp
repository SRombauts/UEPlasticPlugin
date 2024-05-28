// Copyright (c) 2024 Unity Technologies

#include "ScopedTempFile.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

#include "ISourceControlModule.h" // LogSourceControl

FScopedTempFile::FScopedTempFile() :
	FScopedTempFile(TEXT("Temp-"), TEXT(".xml"))
{
}

FScopedTempFile::FScopedTempFile(const FString& InPrefix, const FString& InExtension)
{
	Filename = FPaths::CreateTempFilename(*FPaths::ConvertRelativePathToFull(FPaths::ProjectLogDir()), *InPrefix, *InExtension);
}

FScopedTempFile::FScopedTempFile(const FString& InText)
{
	Filename = FPaths::CreateTempFilename(*FPaths::ConvertRelativePathToFull(FPaths::ProjectLogDir()), TEXT("Temp-"), TEXT(".txt"));
	if (!FFileHelper::SaveStringToFile(InText, *Filename, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogSourceControl, Error, TEXT("Failed to write to temp file: %s"), *Filename);
	}
}

FScopedTempFile::FScopedTempFile(const FText& InText) :
	FScopedTempFile(InText.ToString())
{
}

FScopedTempFile::~FScopedTempFile()
{
	if (FPaths::FileExists(Filename))
	{
		if (!FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*Filename))
		{
			UE_LOG(LogSourceControl, Error, TEXT("Failed to delete temp file: %s"), *Filename);
		}
	}
}

const FString& FScopedTempFile::GetFilename() const
{
	return Filename;
}
