// Copyright (c) 2024 Unity Technologies

#include "SPlasticSourceControlChangesetRow.h"

#include "PlasticSourceControlChangeset.h"
#include "PlasticSourceControlUtils.h"

#include "Widgets/Text/STextBlock.h"

#include "Runtime/Launch/Resources/Version.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "Styling/AppStyle.h"
#else
#include "EditorStyleSet.h"
#endif

#define LOCTEXT_NAMESPACE "PlasticSourceControlChangesetWindow"

FName PlasticSourceControlChangesetsListViewColumn::ChangesetId::Id() { return TEXT("ChangesetId"); }
FText PlasticSourceControlChangesetsListViewColumn::ChangesetId::GetDisplayText() { return LOCTEXT("ChangesetId_Column", "Name"); }
FText PlasticSourceControlChangesetsListViewColumn::ChangesetId::GetToolTipText() { return LOCTEXT("ChangesetId_Column_Tooltip", "Id of the changeset"); }

FName PlasticSourceControlChangesetsListViewColumn::CreatedBy::Id() { return TEXT("CreatedBy"); }
FText PlasticSourceControlChangesetsListViewColumn::CreatedBy::GetDisplayText() { return LOCTEXT("CreatedBy_Column", "Created By"); }
FText PlasticSourceControlChangesetsListViewColumn::CreatedBy::GetToolTipText() { return LOCTEXT("CreatedBy_Column_Tooltip", "Creator of the changeset"); }

FName PlasticSourceControlChangesetsListViewColumn::Date::Id() { return TEXT("Date"); }
FText PlasticSourceControlChangesetsListViewColumn::Date::GetDisplayText() { return LOCTEXT("Date_Column", "Creation Date"); }
FText PlasticSourceControlChangesetsListViewColumn::Date::GetToolTipText() { return LOCTEXT("Date_Column_Tooltip", "Date of creation of the changeset"); }

FName PlasticSourceControlChangesetsListViewColumn::Comment::Id() { return TEXT("Comment"); }
FText PlasticSourceControlChangesetsListViewColumn::Comment::GetDisplayText() { return LOCTEXT("Comment_Column", "Comment"); }
FText PlasticSourceControlChangesetsListViewColumn::Comment::GetToolTipText() { return LOCTEXT("Comment_Column_Tooltip", "Comment describing the changeset"); }

FName PlasticSourceControlChangesetsListViewColumn::Branch::Id() { return TEXT("Branch"); }
FText PlasticSourceControlChangesetsListViewColumn::Branch::GetDisplayText() { return LOCTEXT("Branch_Column", "Branch"); }
FText PlasticSourceControlChangesetsListViewColumn::Branch::GetToolTipText() { return LOCTEXT("Branch_Column_Tooltip", "Branch where the changeset was created"); }

void SPlasticSourceControlChangesetRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwner)
{
	ChangesetToVisualize = InArgs._ChangesetToVisualize.Get();
	HighlightText = InArgs._HighlightText;

	FSuperRowType::FArguments Args = FSuperRowType::FArguments()
		.ShowSelection(true);
	FSuperRowType::Construct(Args, InOwner);
}

TSharedRef<SWidget> SPlasticSourceControlChangesetRow::GenerateWidgetForColumn(const FName& InColumnId)
{
	if (InColumnId == PlasticSourceControlChangesetsListViewColumn::ChangesetId::Id())
	{
		return SNew(STextBlock)
			.Text(FText::AsNumber(ChangesetToVisualize->ChangesetId))
			.ToolTipText(FText::AsNumber(ChangesetToVisualize->ChangesetId))
			.Margin(FMargin(6.f, 1.f))
			.HighlightText(HighlightText);
	}
	else if (InColumnId == PlasticSourceControlChangesetsListViewColumn::CreatedBy::Id())
	{
		return SNew(STextBlock)
			.Text(FText::FromString(ChangesetToVisualize->CreatedBy))
			.ToolTipText(FText::FromString(ChangesetToVisualize->CreatedBy))
			.Margin(FMargin(6.f, 1.f))
			.HighlightText(HighlightText);
	}
	else if (InColumnId == PlasticSourceControlChangesetsListViewColumn::Date::Id())
	{
		return SNew(STextBlock)
			.Text(FText::AsDateTime(ChangesetToVisualize->Date))
			.ToolTipText(FText::AsDateTime(ChangesetToVisualize->Date))
			.Margin(FMargin(6.f, 1.f));
	}
	else if (InColumnId == PlasticSourceControlChangesetsListViewColumn::Comment::Id())
	{
		return SNew(STextBlock)
			.Text(FText::FromString(PlasticSourceControlUtils::UserNameToDisplayName(ChangesetToVisualize->Comment)))
			.ToolTipText(FText::FromString(ChangesetToVisualize->Comment))
			.Margin(FMargin(6.f, 1.f))
			.HighlightText(HighlightText);
	}
	else if (InColumnId == PlasticSourceControlChangesetsListViewColumn::Branch::Id())
	{
		return SNew(STextBlock)
			.Text(FText::FromString(ChangesetToVisualize->Branch))
			.ToolTipText(FText::FromString(ChangesetToVisualize->Branch))
			.Margin(FMargin(6.f, 1.f))
#if ENGINE_MAJOR_VERSION >= 5
			.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
#endif
			.HighlightText(HighlightText);
	}
	else
	{
		return SNullWidget::NullWidget;
	}
}

#undef LOCTEXT_NAMESPACE
