// Copyright (c) 2023 Unity Technologies

#include "SPlasticSourceControlBranchesWidget.h"

#include "PlasticSourceControlProjectSettings.h"

#include "Misc/ComparisonUtility.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PlasticSourceControlWindow"

void SPlasticSourceControlBranchesWidget::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot() // For the toolbar (Search box and Refresh button)
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.MaxWidth(300)
			[
				SAssignNew(FileSearchBox, SSearchBox)
				.HintText(LOCTEXT("SearchBranches", "Search Branches"))
				.ToolTipText(LOCTEXT("PlasticBranchesSearch_Tooltip", "Filter the list of branches by keyword."))
				.OnTextChanged(this, &SPlasticSourceControlBranchesWidget::OnSearchTextChanged)
			]
		]
		+SVerticalBox::Slot() // The main content: the list of branches
		[
			CreateContentPanel()
		]
	];
}

TSharedRef<SWidget> SPlasticSourceControlBranchesWidget::CreateContentPanel()
{
	// Inspired by Engine\Source\Editor\SourceControlWindows\Private\SSourceControlChangelists.cpp
	// TSharedRef<SListView<FChangelistTreeItemPtr>> SSourceControlChangelistsWidget::CreateChangelistFilesView()

	UPlasticSourceControlProjectSettings* Settings = GetMutableDefault<UPlasticSourceControlProjectSettings>();
	if (!Settings->bShowBranchRepositoryColumn)
	{
		HiddenColumnsList.Add(PlasticSourceControlBranchesListViewColumn::Repository::Id());
	}
	if (!Settings->bShowBranchCreatedByColumn)
	{
		HiddenColumnsList.Add(PlasticSourceControlBranchesListViewColumn::CreatedBy::Id());
	}
	if (!Settings->bShowBranchDateColumn)
	{
		HiddenColumnsList.Add(PlasticSourceControlBranchesListViewColumn::Date::Id());
	}
	if (!Settings->bShowBranchCommentColumn)
	{
		HiddenColumnsList.Add(PlasticSourceControlBranchesListViewColumn::Comment::Id());
	}

	TSharedRef<SListView<FPlasticSourceControlBranchRef>> BranchView = SNew(SListView<FPlasticSourceControlBranchRef>)
		.ItemHeight(24.0f)
		.ListItemsSource(&BranchRows)
		.OnGenerateRow(this, &SPlasticSourceControlBranchesWidget::OnGenerateRow)
		.SelectionMode(ESelectionMode::Single)
		// TODO: context menu (to be implementer in a future task)
		// .OnContextMenuOpening(this, &SPlasticSourceControlBranchesWidget::OnOpenContextMenu)
		.OnItemToString_Debug_Lambda([this](FPlasticSourceControlBranchRef Branch) { return Branch->Name; })
		.HeaderRow
		(
			SNew(SHeaderRow)
			.CanSelectGeneratedColumn(true)
			.HiddenColumnsList(HiddenColumnsList)
			.OnHiddenColumnsListChanged(this, &SPlasticSourceControlBranchesWidget::OnHiddenColumnsListChanged)

			+SHeaderRow::Column(PlasticSourceControlBranchesListViewColumn::Name::Id())
			.DefaultLabel(PlasticSourceControlBranchesListViewColumn::Name::GetDisplayText())
			.DefaultTooltip(PlasticSourceControlBranchesListViewColumn::Name::GetToolTipText())
			.ShouldGenerateWidget(true) // Ensure the column cannot be hidden (grayed out in the show/hide drop down menu)
			.FillWidth(2.0f)
			.SortPriority(this, &SPlasticSourceControlBranchesWidget::GetColumnSortPriority, PlasticSourceControlBranchesListViewColumn::Name::Id())
			.SortMode(this, &SPlasticSourceControlBranchesWidget::GetColumnSortMode, PlasticSourceControlBranchesListViewColumn::Name::Id())
			.OnSort(this, &SPlasticSourceControlBranchesWidget::OnColumnSortModeChanged)

			+SHeaderRow::Column(PlasticSourceControlBranchesListViewColumn::Repository::Id())
			.DefaultLabel(PlasticSourceControlBranchesListViewColumn::Repository::GetDisplayText())
			.DefaultTooltip(PlasticSourceControlBranchesListViewColumn::Repository::GetToolTipText())
			.FillWidth(1.5f)
			.SortPriority(this, &SPlasticSourceControlBranchesWidget::GetColumnSortPriority, PlasticSourceControlBranchesListViewColumn::Repository::Id())
			.SortMode(this, &SPlasticSourceControlBranchesWidget::GetColumnSortMode, PlasticSourceControlBranchesListViewColumn::Repository::Id())
			.OnSort(this, &SPlasticSourceControlBranchesWidget::OnColumnSortModeChanged)

			+SHeaderRow::Column(PlasticSourceControlBranchesListViewColumn::CreatedBy::Id())
			.DefaultLabel(PlasticSourceControlBranchesListViewColumn::CreatedBy::GetDisplayText())
			.DefaultTooltip(PlasticSourceControlBranchesListViewColumn::CreatedBy::GetToolTipText())
			.FillWidth(2.5f)
			.SortPriority(this, &SPlasticSourceControlBranchesWidget::GetColumnSortPriority, PlasticSourceControlBranchesListViewColumn::CreatedBy::Id())
			.SortMode(this, &SPlasticSourceControlBranchesWidget::GetColumnSortMode, PlasticSourceControlBranchesListViewColumn::CreatedBy::Id())
			.OnSort(this, &SPlasticSourceControlBranchesWidget::OnColumnSortModeChanged)

			+SHeaderRow::Column(PlasticSourceControlBranchesListViewColumn::Date::Id())
			.DefaultLabel(PlasticSourceControlBranchesListViewColumn::Date::GetDisplayText())
			.DefaultTooltip(PlasticSourceControlBranchesListViewColumn::Date::GetToolTipText())
			.FillWidth(1.5f)
			.SortPriority(this, &SPlasticSourceControlBranchesWidget::GetColumnSortPriority, PlasticSourceControlBranchesListViewColumn::Date::Id())
			.SortMode(this, &SPlasticSourceControlBranchesWidget::GetColumnSortMode, PlasticSourceControlBranchesListViewColumn::Date::Id())
			.OnSort(this, &SPlasticSourceControlBranchesWidget::OnColumnSortModeChanged)

			+SHeaderRow::Column(PlasticSourceControlBranchesListViewColumn::Comment::Id())
			.DefaultLabel(PlasticSourceControlBranchesListViewColumn::Comment::GetDisplayText())
			.DefaultTooltip(PlasticSourceControlBranchesListViewColumn::Comment::GetToolTipText())
			.FillWidth(5.0f)
			.SortPriority(this, &SPlasticSourceControlBranchesWidget::GetColumnSortPriority, PlasticSourceControlBranchesListViewColumn::Comment::Id())
			.SortMode(this, &SPlasticSourceControlBranchesWidget::GetColumnSortMode, PlasticSourceControlBranchesListViewColumn::Comment::Id())
			.OnSort(this, &SPlasticSourceControlBranchesWidget::OnColumnSortModeChanged)
		);

	BranchesListView = BranchView;

	return BranchView;
}

TSharedRef<ITableRow> SPlasticSourceControlBranchesWidget::OnGenerateRow(FPlasticSourceControlBranchRef InBranch, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SBranchTableRow, OwnerTable)
		.BranchToVisualize(InBranch)
		.HighlightText_Lambda([this]() { return FileSearchBox->GetText(); });
}

void SPlasticSourceControlBranchesWidget::OnHiddenColumnsListChanged()
{
	// Update and save config to reload it on the next Editor sessions
	if (BranchesListView && BranchesListView->GetHeaderRow())
	{
		UPlasticSourceControlProjectSettings* Settings = GetMutableDefault<UPlasticSourceControlProjectSettings>();
		Settings->bShowBranchRepositoryColumn = false;
		Settings->bShowBranchCreatedByColumn = true;
		Settings->bShowBranchDateColumn = true;
		Settings->bShowBranchCommentColumn = true;

		for (const FName& ColumnId : BranchesListView->GetHeaderRow()->GetHiddenColumnIds())
		{
			if (ColumnId == PlasticSourceControlBranchesListViewColumn::Repository::Id())
			{
				Settings->bShowBranchRepositoryColumn = false;
			}
			else if (ColumnId == PlasticSourceControlBranchesListViewColumn::CreatedBy::Id())
			{
				Settings->bShowBranchCreatedByColumn = false;
			}
			else if (ColumnId == PlasticSourceControlBranchesListViewColumn::Date::Id())
			{
				Settings->bShowBranchDateColumn = false;
			}
			else if (ColumnId == PlasticSourceControlBranchesListViewColumn::Comment::Id())
			{
				Settings->bShowBranchCommentColumn = false;
			}
		}
		Settings->SaveConfig();
	}
}

void SPlasticSourceControlBranchesWidget::OnSearchTextChanged(const FText& InFilterText)
{
	// TODO implement filtering with the SearchTextFilter
	BranchRows = SourceControlBranches;
}

EColumnSortPriority::Type SPlasticSourceControlBranchesWidget::GetColumnSortPriority(const FName InColumnId) const
{
	if (InColumnId == PrimarySortedColumn)
	{
		return EColumnSortPriority::Primary;
	}
	else if (InColumnId == SecondarySortedColumn)
	{
		return EColumnSortPriority::Secondary;
	}

	return EColumnSortPriority::Max; // No specific priority.
}

EColumnSortMode::Type SPlasticSourceControlBranchesWidget::GetColumnSortMode(const FName InColumnId) const
{
	if (InColumnId == PrimarySortedColumn)
	{
		return PrimarySortMode;
	}
	else if (InColumnId == SecondarySortedColumn)
	{
		return SecondarySortMode;
	}

	return EColumnSortMode::None;
}

void SPlasticSourceControlBranchesWidget::OnColumnSortModeChanged(const EColumnSortPriority::Type InSortPriority, const FName& InColumnId, const EColumnSortMode::Type InSortMode)
{
	if (InSortPriority == EColumnSortPriority::Primary)
	{
		PrimarySortedColumn = InColumnId;
		PrimarySortMode = InSortMode;

		if (InColumnId == SecondarySortedColumn) // Cannot be primary and secondary at the same time.
		{
			SecondarySortedColumn = FName();
			SecondarySortMode = EColumnSortMode::None;
		}
	}
	else if (InSortPriority == EColumnSortPriority::Secondary)
	{
		SecondarySortedColumn = InColumnId;
		SecondarySortMode = InSortMode;
	}

	if (GetListView())
	{
		SortBranchView();
		GetListView()->RequestListRefresh();
	}
}

void SPlasticSourceControlBranchesWidget::SortBranchView()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SPlasticSourceControlBranchesWidget::SortBranchView);

	if (PrimarySortedColumn.IsNone() || BranchRows.IsEmpty())
	{
		return; // No column selected for sorting or nothing to sort.
	}

	auto CompareNames = [](const FPlasticSourceControlBranch* Lhs, const FPlasticSourceControlBranch* Rhs)
	{
		return UE::ComparisonUtility::CompareNaturalOrder(*Lhs->Name, *Rhs->Name);
	};

	auto CompareRepository = [](const FPlasticSourceControlBranch* Lhs, const FPlasticSourceControlBranch* Rhs)
	{
		return FCString::Stricmp(*Lhs->Repository, *Rhs->Repository);
	};

	auto CompareCreatedBy = [](const FPlasticSourceControlBranch* Lhs, const FPlasticSourceControlBranch* Rhs)
	{
		return FCString::Stricmp(*Lhs->CreatedBy, *Rhs->CreatedBy);
	};

	auto CompareDate = [](const FPlasticSourceControlBranch* Lhs, const FPlasticSourceControlBranch* Rhs)
	{
		return Lhs->Date < Rhs->Date ? -1 : (Lhs->Date == Rhs->Date ? 0 : 1);
	};

	auto CompareComment = [](const FPlasticSourceControlBranch* Lhs, const FPlasticSourceControlBranch* Rhs)
	{
		return FCString::Stricmp(*Lhs->Comment, *Rhs->Comment);
	};

	auto GetCompareFunc = [&](const FName& ColumnId)
	{
		if (ColumnId == PlasticSourceControlBranchesListViewColumn::Name::Id())
		{
			return TFunction<int32(const FPlasticSourceControlBranch*, const FPlasticSourceControlBranch*)>(CompareNames);
		}
		else if (ColumnId == PlasticSourceControlBranchesListViewColumn::Repository::Id())
		{
			return TFunction<int32(const FPlasticSourceControlBranch*, const FPlasticSourceControlBranch*)>(CompareRepository);
		}
		else if (ColumnId == PlasticSourceControlBranchesListViewColumn::CreatedBy::Id())
		{
			return TFunction<int32(const FPlasticSourceControlBranch*, const FPlasticSourceControlBranch*)>(CompareCreatedBy);
		}
		else if (ColumnId == PlasticSourceControlBranchesListViewColumn::Date::Id())
		{
			return TFunction<int32(const FPlasticSourceControlBranch*, const FPlasticSourceControlBranch*)>(CompareDate);
		}
		else if (ColumnId == PlasticSourceControlBranchesListViewColumn::Comment::Id())
		{
			return TFunction<int32(const FPlasticSourceControlBranch*, const FPlasticSourceControlBranch*)>(CompareComment);
		}
		else
		{
			checkNoEntry();
			return TFunction<int32(const FPlasticSourceControlBranch*, const FPlasticSourceControlBranch*)>();
		};
	};

	TFunction<int32(const FPlasticSourceControlBranch*, const FPlasticSourceControlBranch*)> PrimaryCompare = GetCompareFunc(PrimarySortedColumn);
	TFunction<int32(const FPlasticSourceControlBranch*, const FPlasticSourceControlBranch*)> SecondaryCompare;
	if (!SecondarySortedColumn.IsNone())
	{
		SecondaryCompare = GetCompareFunc(SecondarySortedColumn);
	}

	if (PrimarySortMode == EColumnSortMode::Ascending)
	{
		// NOTE: StableSort() would give a better experience when the sorted columns(s) has the same values and new values gets added, but it is slower
		//       with large changelists (7600 items was about 1.8x slower in average measured with Unreal Insight). Because this code runs in the main
		//       thread and can be invoked a lot, the trade off went if favor of speed.
		BranchRows.Sort([this, &PrimaryCompare, &SecondaryCompare](const TSharedPtr<FPlasticSourceControlBranch>& Lhs, const TSharedPtr<FPlasticSourceControlBranch>& Rhs)
		{
			int32 Result = PrimaryCompare(static_cast<FPlasticSourceControlBranch*>(Lhs.Get()), static_cast<FPlasticSourceControlBranch*>(Rhs.Get()));
			if (Result < 0)
			{
				return true;
			}
			else if (Result > 0 || !SecondaryCompare)
			{
				return false;
			}
			else if (SecondarySortMode == EColumnSortMode::Ascending)
			{
				return SecondaryCompare(static_cast<FPlasticSourceControlBranch*>(Lhs.Get()), static_cast<FPlasticSourceControlBranch*>(Rhs.Get())) < 0;
			}
			else
			{
				return SecondaryCompare(static_cast<FPlasticSourceControlBranch*>(Lhs.Get()), static_cast<FPlasticSourceControlBranch*>(Rhs.Get())) > 0;
			}
		});
	}
	else
	{
		BranchRows.Sort([this, &PrimaryCompare, &SecondaryCompare](const TSharedPtr<FPlasticSourceControlBranch>& Lhs, const TSharedPtr<FPlasticSourceControlBranch>& Rhs)
		{
			int32 Result = PrimaryCompare(static_cast<FPlasticSourceControlBranch*>(Lhs.Get()), static_cast<FPlasticSourceControlBranch*>(Rhs.Get()));
			if (Result > 0)
			{
				return true;
			}
			else if (Result < 0 || !SecondaryCompare)
			{
				return false;
			}
			else if (SecondarySortMode == EColumnSortMode::Ascending)
			{
				return SecondaryCompare(static_cast<FPlasticSourceControlBranch*>(Lhs.Get()), static_cast<FPlasticSourceControlBranch*>(Rhs.Get())) < 0;
			}
			else
			{
				return SecondaryCompare(static_cast<FPlasticSourceControlBranch*>(Lhs.Get()), static_cast<FPlasticSourceControlBranch*>(Rhs.Get())) > 0;
			}
		});
	}
}


FName PlasticSourceControlBranchesListViewColumn::Name::Id() { return TEXT("Name"); }
FText PlasticSourceControlBranchesListViewColumn::Name::GetDisplayText() { return LOCTEXT("Name_Column", "Name"); }
FText PlasticSourceControlBranchesListViewColumn::Name::GetToolTipText() { return LOCTEXT("Name_Column_Tooltip", "Displays the asset/file name"); }

FName PlasticSourceControlBranchesListViewColumn::Repository::Id() { return TEXT("Repository"); }
FText PlasticSourceControlBranchesListViewColumn::Repository::GetDisplayText() { return LOCTEXT("Repository_Column", "Repository"); }
FText PlasticSourceControlBranchesListViewColumn::Repository::GetToolTipText() { return LOCTEXT("Repository_Column_Tooltip", "Displays the repository where the branch has been created"); }

FName PlasticSourceControlBranchesListViewColumn::CreatedBy::Id() { return TEXT("CreatedBy"); }
FText PlasticSourceControlBranchesListViewColumn::CreatedBy::GetDisplayText() { return LOCTEXT("CreatedBy_Column", "Created by"); }
FText PlasticSourceControlBranchesListViewColumn::CreatedBy::GetToolTipText() { return LOCTEXT("CreatedBy_Column_Tooltip", "Displays the name of the creator of the branch"); }

FName PlasticSourceControlBranchesListViewColumn::Date::Id() { return TEXT("Date"); }
FText PlasticSourceControlBranchesListViewColumn::Date::GetDisplayText() { return LOCTEXT("Date_Column", "Creation date"); }
FText PlasticSourceControlBranchesListViewColumn::Date::GetToolTipText() { return LOCTEXT("Date_Column_Tooltip", "Displays the branch creation date"); }

FName PlasticSourceControlBranchesListViewColumn::Comment::Id() { return TEXT("Comment"); }
FText PlasticSourceControlBranchesListViewColumn::Comment::GetDisplayText() { return LOCTEXT("Comment_Column", "Comment"); }
FText PlasticSourceControlBranchesListViewColumn::Comment::GetToolTipText() { return LOCTEXT("Comment_Column_Tooltip", "Displays the branch comment"); }

void SBranchTableRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwner)
{
	BranchToVisualize = static_cast<FPlasticSourceControlBranch*>(InArgs._BranchToVisualize.Get());

	HighlightText = InArgs._HighlightText;

	FSuperRowType::FArguments Args = FSuperRowType::FArguments()
		.ShowSelection(true);
	FSuperRowType::Construct(Args, InOwner);
}

TSharedRef<SWidget> SBranchTableRow::GenerateWidgetForColumn(const FName& InColumnId)
{
	if (InColumnId == PlasticSourceControlBranchesListViewColumn::Name::Id())
	{
		return SNew(STextBlock)
			.Text(FText::FromString(BranchToVisualize->Name))
			.ToolTipText(FText::FromString(BranchToVisualize->Name))
			.HighlightText(HighlightText);
	}
	else if (InColumnId == PlasticSourceControlBranchesListViewColumn::Repository::Id())
	{
		return SNew(STextBlock)
			.Text(FText::FromString(BranchToVisualize->Repository))
			.ToolTipText(FText::FromString(BranchToVisualize->Repository))
			.HighlightText(HighlightText);
	}
	else if (InColumnId == PlasticSourceControlBranchesListViewColumn::CreatedBy::Id())
	{
		return SNew(STextBlock)
			.Text(FText::FromString(BranchToVisualize->CreatedBy))
			.ToolTipText(FText::FromString(BranchToVisualize->CreatedBy))
			.HighlightText(HighlightText);
	}
	else if (InColumnId == PlasticSourceControlBranchesListViewColumn::Date::Id())
	{
		return SNew(STextBlock)
			.Text(FText::AsDateTime(BranchToVisualize->Date))
			.ToolTipText(FText::AsDateTime(BranchToVisualize->Date));
	}
	else if (InColumnId == PlasticSourceControlBranchesListViewColumn::Comment::Id())
	{
		return SNew(STextBlock)
			.Text(FText::FromString(BranchToVisualize->Comment))
			.ToolTipText(FText::FromString(BranchToVisualize->Comment))
			.HighlightText(HighlightText);
	}
	else
	{
		return SNullWidget::NullWidget;
	}
}

#undef LOCTEXT_NAMESPACE
