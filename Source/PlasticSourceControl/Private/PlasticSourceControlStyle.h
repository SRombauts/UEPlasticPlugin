// Copyright (c) 2024 Unity Technologies

#pragma once

#include "CoreMinimal.h"

class FSlateStyleSet;
class ISlateStyle;

class FPlasticSourceControlStyle
{
public:
	static void Initialize();

	static void Shutdown();

	/** reloads textures used by slate renderer */
	static void ReloadTextures();

	/** @return The Slate style set */
	static const ISlateStyle& Get();

	static FName GetStyleSetName();

private:
	static TSharedRef<FSlateStyleSet> Create();

private:
	static FString InContent(const FString& RelativePath, const ANSICHAR* Extension);

private:
	static TSharedPtr<class FSlateStyleSet> StyleInstance;
};
