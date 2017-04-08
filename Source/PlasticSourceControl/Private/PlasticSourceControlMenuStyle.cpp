// Copyright (c) 2016-2017 Codice Software - Sebastien Rombauts (sebastien.rombauts@gmail.com)

#include "PlasticSourceControlPrivatePCH.h"

#include "PlasticSourceControlMenuStyle.h"
#include "SlateGameResources.h"
#include "Styling/ISlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Framework/Application/SlateApplication.h"
#include "IPluginManager.h"

TSharedPtr< FSlateStyleSet > FPlasticSourceControlMenuStyle::StyleInstance = NULL;

void FPlasticSourceControlMenuStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FPlasticSourceControlMenuStyle::Shutdown()
{
	if (StyleInstance.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
		ensure(StyleInstance.IsUnique());
		StyleInstance.Reset();
	}
}

FName FPlasticSourceControlMenuStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("PlasticSourceControlMenuStyle"));
	return StyleSetName;
}

#define IMAGE_BRUSH( RelativePath, ... ) FSlateImageBrush( Style->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )

const FVector2D Icon16x16(16.0f, 16.0f);

TSharedRef< FSlateStyleSet > FPlasticSourceControlMenuStyle::Create()
{
	TSharedRef< FSlateStyleSet > Style = MakeShareable(new FSlateStyleSet("PlasticSourceControlMenuStyle"));
	Style->SetContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate"));

	// Add icons to the source control menu (using existing Editor Source Control icons)
	Style->Set("PlasticSourceControlMenu.SyncProject",		new IMAGE_BRUSH(TEXT("Icons/icon_SCC_Sync_16x"),	Icon16x16));
	Style->Set("PlasticSourceControlMenu.RevertUnchanged",	new IMAGE_BRUSH(TEXT("Icons/icon_SCC_Revert_16x"),	Icon16x16));
	Style->Set("PlasticSourceControlMenu.RevertAll",		new IMAGE_BRUSH(TEXT("Icons/icon_SCC_Revert_16x"),	Icon16x16));

	return Style;
}

#undef IMAGE_BRUSH

void FPlasticSourceControlMenuStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

const ISlateStyle& FPlasticSourceControlMenuStyle::Get()
{
	return *StyleInstance;
}
