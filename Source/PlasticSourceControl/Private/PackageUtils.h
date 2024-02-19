// Copyright (c) 2024 Unity Technologies

#pragma once

#include "CoreMinimal.h"

namespace PackageUtils
{
	TArray<FString> AssetDataToFileNames(const TArray<FAssetData>& InAssetObjectPaths);

	bool SaveDirtyPackages();
	TArray<FString> ListAllPackages();

	void UnlinkPackages(const TArray<FString>& InFiles);
	void UnlinkPackagesInMainThread(const TArray<FString>& InFiles);

	void ReloadPackages(const TArray<FString>& InFiles);
	void ReloadPackagesInMainThread(const TArray<FString>& InFiles);
} // namespace PackageUtils
