// Copyright (c) 2024 Unity Technologies

#include "SPlasticSourceControlStatusBar.h"

#include "PlasticSourceControlModule.h"

#include "Runtime/Launch/Resources/Version.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "Styling/AppStyle.h"
#else
#include "EditorStyleSet.h"
#endif
#include "Input/Reply.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Input/SButton.h"

#define LOCTEXT_NAMESPACE "PlasticSourceControl"

void SPlasticSourceControlStatusBar::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SButton)
		.ContentPadding(FMargin(6.0f, 0.0f))
		.ToolTipText(LOCTEXT("PlasticBranchesWindowTooltip", "Open the Branches window."))
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
#else
		.ButtonStyle(FEditorStyle::Get(), "SimpleButton")
#endif
		.OnClicked(this, &SPlasticSourceControlStatusBar::OnClicked)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
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
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(5.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
				.TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText"))
#else
				.TextStyle(&FEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText"))
#endif
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
	FPlasticSourceControlModule::Get().GetBranchesWindow().OpenTab();

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
