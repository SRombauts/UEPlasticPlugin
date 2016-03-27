// Copyright (c) 2016 Codice Software - Sebastien Rombauts (sebastien.rombauts@gmail.com)

#include "PlasticSourceControlPrivatePCH.h"
#include "SPlasticSourceControlSettings.h"
#include "PlasticSourceControlModule.h"
#include "PlasticSourceControlUtils.h"

#define LOCTEXT_NAMESPACE "SPlasticSourceControlSettings"

void SPlasticSourceControlSettings::Construct(const FArguments& InArgs)
{
	FSlateFontInfo Font = FEditorStyle::GetFontStyle(TEXT("SourceControl.LoginWindow.Font"));

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage( FEditorStyle::GetBrush("DetailsView.CategoryBottom"))
		.Padding(FMargin(0.0f, 3.0f, 0.0f, 0.0f))
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(2.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(2.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("BinaryPathLabel", "Plastic Path"))
						.ToolTipText(LOCTEXT("BinaryPathLabel_Tooltip", "Path to Plastic binary"))
						.Font(Font)
					]
				]
				+SHorizontalBox::Slot()
				.FillWidth(2.0f)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(2.0f)
					[
						SNew(SEditableTextBox)
						.Text(this, &SPlasticSourceControlSettings::GetBinaryPathText)
						.ToolTipText(LOCTEXT("BinaryPathLabel_Tooltip", "Path to Plastic binary"))
						.OnTextCommitted(this, &SPlasticSourceControlSettings::OnBinaryPathTextCommited)
						.Font(Font)
					]
				]
			]
		]
	];
}

FText SPlasticSourceControlSettings::GetBinaryPathText() const
{
	FPlasticSourceControlModule& PlasticSourceControl = FModuleManager::LoadModuleChecked<FPlasticSourceControlModule>("PlasticSourceControl");
	return FText::FromString(PlasticSourceControl.AccessSettings().GetBinaryPath());
}

void SPlasticSourceControlSettings::OnBinaryPathTextCommited(const FText& InText, ETextCommit::Type InCommitType) const
{
	FPlasticSourceControlModule& PlasticSourceControl = FModuleManager::LoadModuleChecked<FPlasticSourceControlModule>("PlasticSourceControl");
	PlasticSourceControl.AccessSettings().SetBinaryPath(InText.ToString());
	PlasticSourceControl.SaveSettings();
}

#undef LOCTEXT_NAMESPACE
