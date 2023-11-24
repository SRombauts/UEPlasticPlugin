// Copyright (c) 2023 Unity Technologies

#include "SPlasticSourceControlCreateBranch.h"

#include "SPlasticSourceControlBranchesWidget.h"

#include "Runtime/Launch/Resources/Version.h"
#include "Styling/AppStyle.h"
#include "Input/Reply.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"

#define LOCTEXT_NAMESPACE "PlasticSourceControlWindow"

void SPlasticSourceControlCreateBranch::Construct(const FArguments& InArgs)
{
	BranchesWidget = InArgs._BranchesWidget;
	ParentWindow = InArgs._ParentWindow;
	ParentBranchName = InArgs._ParentBranchName;

	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(5.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Text(FText::Format(LOCTEXT("PlasticCreateBrancheDetails", "Create a new child branch from last changeset on br:{0}"), FText::FromString(ParentBranchName)))
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(5.0f))
		.VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)
			.ToolTipText(LOCTEXT("PlasticCreateBrancheNameTooltip", "Enter a name for the new branch to create"))
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("PlasticCreateBrancheNameLabel", "Branch name:"))
			]
			+SHorizontalBox::Slot()
			.FillWidth(6.0f)
			[
				SNew(SEditableTextBox)
				.HintText(LOCTEXT("PlasticCreateBrancheNameHint", "Name of the new branch"))
				.OnTextChanged_Lambda([this](const FText& InExpressionText)
				{
					NewBranchName = InExpressionText.ToString();
				})
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(5.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)
			.ToolTipText(LOCTEXT("PlasticCreateBrancheCommentTooltip", "Enter optional comments for the new branch"))
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("PlasticCreateBrancheCommentLabel", "Comments:"))
			]
			+SHorizontalBox::Slot()
			.FillWidth(6.0f)
			[
				SNew(SBox)
				.MinDesiredHeight(120)
				.WidthOverride(520)
				[
					SNew(SMultiLineEditableTextBox)
					.AutoWrapText(true)
					.HintText(LOCTEXT("PlasticCreateBrancheCommentHing", "Comments for the new branch"))
					.OnTextCommitted_Lambda([this](const FText& InExpressionText, ETextCommit::Type)
					{
						NewBranchComment = InExpressionText.ToString();
					})
				]
			]
		]
		// Option to switch workspace to this branch
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(5.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this]() { return bSwitchWorkspace ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged(this, &SPlasticSourceControlCreateBranch::OnCheckedSwitchWorkspace)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("PlasticSwitchWorkspace", "Switch workspace to this branch"))
				]
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.0f)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Right)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
				.Text(NSLOCTEXT("CreateBranch", "Create", "Create"))
				.IsEnabled_Lambda([this]() { return !NewBranchName.IsEmpty(); })
				.ToolTipText(NSLOCTEXT("SourceControl.SubmitPanel", "Save_Tooltip", "Create the branch."))
				.OnClicked(this, &SPlasticSourceControlCreateBranch::CreateClicked)
			]
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
				.Text(NSLOCTEXT("CreateBranch", "Cancel", "Cancel"))
				.OnClicked(this, &SPlasticSourceControlCreateBranch::CancelClicked)
			]
		]
	];
}

FReply SPlasticSourceControlCreateBranch::CreateClicked()
{
	if (TSharedPtr<SPlasticSourceControlBranchesWidget> Branches = BranchesWidget.Pin())
	{
		Branches->CreateBranch(ParentBranchName, NewBranchName, NewBranchComment, bSwitchWorkspace);
	}

	if (TSharedPtr<SWindow> ParentWindowPin = ParentWindow.Pin())
	{
		ParentWindowPin->RequestDestroyWindow();
	}

	return FReply::Handled();
}

FReply SPlasticSourceControlCreateBranch::CancelClicked()
{
	if (TSharedPtr<SWindow> ParentWindowPin = ParentWindow.Pin())
	{
		ParentWindowPin->RequestDestroyWindow();
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
