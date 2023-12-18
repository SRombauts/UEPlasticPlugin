// Copyright (c) 2023 Unity Technologies

#include "SPlasticSourceControlRenameBranch.h"

#include "SPlasticSourceControlBranchesWidget.h"

#include "Runtime/Launch/Resources/Version.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "Styling/AppStyle.h"
#else
#include "EditorStyleSet.h"
#endif
#include "Input/Reply.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"

#define LOCTEXT_NAMESPACE "PlasticSourceControlWindow"

void SPlasticSourceControlRenameBranch::Construct(const FArguments& InArgs)
{
	BranchesWidget = InArgs._BranchesWidget;
	ParentWindow = InArgs._ParentWindow;
	OldBranchName = InArgs._OldBranchName;

	// Extract the short name of the branch, after the last slash
	NewBranchName = OldBranchName;
	int32 LastSlashIndex;
	if (OldBranchName.FindLastChar(TEXT('/'), LastSlashIndex))
	{
		NewBranchName = OldBranchName.RightChop(LastSlashIndex + 1);
	}

	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(10.0f, 5.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			[
				SNew(STextBlock)
				.Text(FText::Format(LOCTEXT("PlasticRenameBrancheDetails", "Rename branch {0}"), FText::FromString(OldBranchName)))
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(10.0f, 5.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)
			.ToolTipText(LOCTEXT("PlasticRenameBrancheNameTooltip", "Enter a new name for the branch"))
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("PlasticRenameBrancheNameLabel", "New name: "))
			]
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.FillWidth(1.0f)
			[
				SAssignNew(BranchNameTextBox, SEditableTextBox)
				.Text(FText::FromString(NewBranchName))
				.OnTextChanged_Lambda([this](const FText& InText)
				{
					NewBranchName = InText.ToString();
				})
				.OnTextCommitted_Lambda([this](const FText& InText, ETextCommit::Type TextCommitType)
				{
					NewBranchName = InText.ToString();
					if (TextCommitType == ETextCommit::OnEnter)
					{
						RenamedClicked();
					}
				})
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(5.0f)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Right)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
				.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
#else
				.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
#endif
				.Text(LOCTEXT("Rename", "Rename"))
				.IsEnabled(this, &SPlasticSourceControlRenameBranch::IsNewBranchNameValid)
				.ToolTipText(this, &SPlasticSourceControlRenameBranch::RenameButtonTooltip)
				.OnClicked(this, &SPlasticSourceControlRenameBranch::RenamedClicked)
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
				.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
#else
				.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
#endif
				.Text(LOCTEXT("Cancel", "Cancel"))
				.OnClicked(this, &SPlasticSourceControlRenameBranch::CancelClicked)
			]
		]
	];

	ParentWindow.Pin()->SetWidgetToFocusOnActivate(BranchNameTextBox);
}

bool SPlasticSourceControlRenameBranch::IsNewBranchNameValid() const
{
	if (NewBranchName.IsEmpty())
	{
		return false;
	}

	return SPlasticSourceControlBranchesWidget::IsBranchNameValid(NewBranchName);
}

FText SPlasticSourceControlRenameBranch::RenameButtonTooltip() const
{
	if (NewBranchName.IsEmpty())
	{
		return LOCTEXT("RenameEmpty_Tooltip", "Enter a name for the new branch.");
	}

	if (!SPlasticSourceControlBranchesWidget::IsBranchNameValid(NewBranchName))
	{
		return LOCTEXT("RenameInvalid_Tooltip", "Branch name cannot contain any of the following characters: @#/:\"?'\\n\\r\\t");
	}

	return FText::Format(LOCTEXT("RenameBranch_Tooltip", "Rename branch to {0}."),
		FText::FromString(OldBranchName + TEXT("/") + NewBranchName));
}

FReply SPlasticSourceControlRenameBranch::RenamedClicked()
{
	if (TSharedPtr<SPlasticSourceControlBranchesWidget> Branches = BranchesWidget.Pin())
	{
		Branches->RenameBranch(OldBranchName, NewBranchName);
	}

	if (TSharedPtr<SWindow> ParentWindowPin = ParentWindow.Pin())
	{
		ParentWindowPin->RequestDestroyWindow();
	}

	return FReply::Handled();
}

FReply SPlasticSourceControlRenameBranch::CancelClicked()
{
	if (TSharedPtr<SWindow> ParentWindowPin = ParentWindow.Pin())
	{
		ParentWindowPin->RequestDestroyWindow();
	}

	return FReply::Handled();
}

FReply SPlasticSourceControlRenameBranch::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		// Pressing Escape returns as if the user clicked Cancel
		return CancelClicked();
	}

	return FReply::Unhandled();
}

#undef LOCTEXT_NAMESPACE
