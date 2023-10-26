// Copyright (c) 2023 Unity Technologies

#include "SPlasticSourceControlStatusBar.h"

#include "PlasticSourceControlModule.h"

#include "Runtime/Launch/Resources/Version.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "Styling/AppStyle.h"
#else
#include "EditorStyleSet.h"
#endif

#define LOCTEXT_NAMESPACE "PlasticSourceControl"

void SPlasticSourceControlStatusBar::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SButton)
		.ContentPadding(FMargin(6.0f, 0.0f))
		.ToolTipText(LOCTEXT("PlasticBranches_Tooltip", "Current branch"))
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
		.OnClicked(this, &SPlasticSourceControlStatusBar::OnClicked)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(SImage)	
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
				.Image(FAppStyle::GetBrush("SourceControl.Branch"))
#else
				.Image(FEditorStyle::GetBrush("SourceControl.Branch"))
#endif
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(5, 0, 0, 0))
			[
				SNew(STextBlock)
				.TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText"))
				.Text_Lambda([this]() { return GetStatusBarText(); })
			]
		]
	];
}

FText SPlasticSourceControlStatusBar::GetStatusBarText() const
{;
	return FText::FromString(FPlasticSourceControlModule::Get().GetProvider().GetBranchName());
}

FReply SPlasticSourceControlStatusBar::OnClicked()
{
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
