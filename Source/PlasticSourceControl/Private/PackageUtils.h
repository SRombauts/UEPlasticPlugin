// Copyright Unity Technologies

#pragma once

#include "CoreMinimal.h"

namespace PackageUtils
{
	TArray<FString> AssetDateToFileNames(const TArray<FAssetData>& InAssetObjectPaths);
	void UnlinkPackages(const TArray<FString>& InFiles);
	void UnlinkPackagesInMainThread(const TArray<FString>& InFiles);

	void ReloadPackages(const TArray<FString>& InFiles);
	void ReloadPackagesInMainThread(const TArray<FString>& InFiles);
} // namespace PackageUtils
