// Copyright (c) 2024 Unity Technologies

#include "SPlasticSourceControlChangesetsWidget.h"

#include "PlasticSourceControlChangeset.h"
#include "PlasticSourceControlModule.h"
#include "PlasticSourceControlOperations.h"
#include "PlasticSourceControlProjectSettings.h"
#include "PlasticSourceControlUtils.h"
#include "SPlasticSourceControlChangesetRow.h"

#include "PackageUtils.h"

#include "ISourceControlModule.h"

#include "Logging/MessageLog.h"
#include "ToolMenus.h"
#include "ToolMenuContext.h"

#include "Runtime/Launch/Resources/Version.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
#include "Misc/ComparisonUtility.h"
#endif
#include "Misc/MessageDialog.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "Styling/AppStyle.h"
#else
#include "EditorStyleSet.h"
#endif
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "PlasticSourceControlChangesetWindow"

void SPlasticSourceControlChangesetsWidget::Construct(const FArguments& InArgs)
{
	ISourceControlModule::Get().RegisterProviderChanged(FSourceControlProviderChanged::FDelegate::CreateSP(this, &SPlasticSourceControlChangesetsWidget::OnSourceControlProviderChanged));
	// register for any source control change to detect new local Changesets on check-out, and release of them on check-in
	SourceControlStateChangedDelegateHandle = ISourceControlModule::Get().GetProvider().RegisterSourceControlStateChanged_Handle(FSourceControlStateChanged::FDelegate::CreateSP(this, &SPlasticSourceControlChangesetsWidget::HandleSourceControlStateChanged));

	CurrentChangesetId = FPlasticSourceControlModule::Get().GetProvider().GetChangesetNumber();

	SearchTextFilter = MakeShared<TTextFilter<const FPlasticSourceControlChangeset&>>(TTextFilter<const FPlasticSourceControlChangeset&>::FItemToStringArray::CreateSP(this, &SPlasticSourceControlChangesetsWidget::PopulateItemSearchStrings));
	SearchTextFilter->OnChanged().AddSP(this, &SPlasticSourceControlChangesetsWidget::OnRefreshUI);

	FromDateInDaysValues.Add(TPair<int32, FText>(7, FText::FromString(TEXT("Last week"))));
	FromDateInDaysValues.Add(TPair<int32, FText>(15, FText::FromString(TEXT("Last 15 days"))));
	FromDateInDaysValues.Add(TPair<int32, FText>(30, FText::FromString(TEXT("Last month"))));
	FromDateInDaysValues.Add(TPair<int32, FText>(91, FText::FromString(TEXT("Last 3 months"))));
	FromDateInDaysValues.Add(TPair<int32, FText>(182, FText::FromString(TEXT("Last 6 months"))));
	FromDateInDaysValues.Add(TPair<int32, FText>(365, FText::FromString(TEXT("Last year"))));
	FromDateInDaysValues.Add(TPair<int32, FText>(-1, FText::FromString(TEXT("All time"))));

	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot() // For the toolbar (Search box and Refresh button)
		.AutoHeight()
		[
			SNew(SBorder)
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
#else
			.BorderImage(FEditorStyle::GetBrush("DetailsView.CategoryBottom"))
#endif
			.Padding(4.0f)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					CreateToolBar()
				]
				+SHorizontalBox::Slot()
				.MaxWidth(10.0f)
				[
					SNew(SSpacer)
				]
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.MaxWidth(300.0f)
				[
					SAssignNew(FileSearchBox, SSearchBox)
					.HintText(LOCTEXT("SearchChangesets", "Search changesets"))
					.ToolTipText(LOCTEXT("PlasticChangesetsSearch_Tooltip", "Filter the list of changesets by keyword."))
					.OnTextChanged(this, &SPlasticSourceControlChangesetsWidget::OnSearchTextChanged)
				]
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.MaxWidth(125.0f)
				.Padding(10.f, 0.f)
				[
					SNew(SComboButton)
					.ToolTipText(LOCTEXT("PlasticChangesetesDate_Tooltip", "Filter the list of changesets by date of creation."))
					.OnGetMenuContent(this, &SPlasticSourceControlChangesetsWidget::BuildFromDateDropDownMenu)
					.ButtonContent()
					[
						SNew(STextBlock)
						.Text_Lambda([this]() { return FromDateInDaysValues[FromDateInDays]; })
					]
				]
			]
		]
		+SVerticalBox::Slot() // The main content: the list of changesets
		[
			CreateContentPanel()
		]
		+SVerticalBox::Slot() // Status bar (Always visible)
		.AutoHeight()
		[
			SNew(SBox)
			.Padding(FMargin(0.f, 3.f))
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text_Lambda([this]() { return RefreshStatus; })
					.Margin(FMargin(5.f, 0.f))
				]
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Right)
				[
					SNew(STextBlock)
					.Text_Lambda([this]() { return FText::AsNumber(CurrentChangesetId); })
					.ToolTipText(LOCTEXT("PlasticChangesetCurrent_Tooltip", "Current changeset."))
				]
			]
		]
	];
}

TSharedRef<SWidget> SPlasticSourceControlChangesetsWidget::CreateToolBar()
{
#if ENGINE_MAJOR_VERSION >= 5
	FSlimHorizontalToolBarBuilder ToolBarBuilder(nullptr, FMultiBoxCustomization::None);
#else
	FToolBarBuilder ToolBarBuilder(nullptr, FMultiBoxCustomization::None);
#endif

	ToolBarBuilder.AddToolBarButton(
		FUIAction(
			FExecuteAction::CreateLambda([this]() { RequestChangesetsRefresh(); })),
		NAME_None,
		LOCTEXT("SourceControl_RefreshButton", "Refresh"),
		LOCTEXT("SourceControl_RefreshButton_Tooltip", "Refreshes changesets from revision control provider."),
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "SourceControl.Actions.Refresh"));
#else
		FSlateIcon(FEditorStyle::GetStyleSetName(), "SourceControl.Actions.Refresh"));
#endif

	return ToolBarBuilder.MakeWidget();
}

TSharedRef<SWidget> SPlasticSourceControlChangesetsWidget::CreateContentPanel()
{
	// Inspired by Engine\Source\Editor\SourceControlWindows\Private\SSourceControlChangelists.cpp
	// TSharedRef<SListView<FChangelistTreeItemPtr>> SSourceControlChangelistsWidget::CreateChangelistFilesView()

	UPlasticSourceControlProjectSettings* Settings = GetMutableDefault<UPlasticSourceControlProjectSettings>();
	if (!Settings->bShowChangesetCreatedByColumn)
	{
		HiddenColumnsList.Add(PlasticSourceControlChangesetsListViewColumn::CreatedBy::Id());
	}
	if (!Settings->bShowChangesetDateColumn)
	{
		HiddenColumnsList.Add(PlasticSourceControlChangesetsListViewColumn::Date::Id());
	}
	if (!Settings->bShowChangesetCommentColumn)
	{
		HiddenColumnsList.Add(PlasticSourceControlChangesetsListViewColumn::Comment::Id());
	}
	if (!Settings->bShowChangesetBranchColumn)
	{
		HiddenColumnsList.Add(PlasticSourceControlChangesetsListViewColumn::Branch::Id());
	}

	TSharedRef<SListView<FPlasticSourceControlChangesetRef>> ChangesetView = SNew(SListView<FPlasticSourceControlChangesetRef>)
		.ItemHeight(24.0f)
		.ListItemsSource(&ChangesetRows)
		.OnGenerateRow(this, &SPlasticSourceControlChangesetsWidget::OnGenerateRow)
		.SelectionMode(ESelectionMode::Multi)
		.OnContextMenuOpening(this, &SPlasticSourceControlChangesetsWidget::OnOpenContextMenu)
		.OnMouseButtonDoubleClick(this, &SPlasticSourceControlChangesetsWidget::OnItemDoubleClicked)
		.OnItemToString_Debug_Lambda([this](FPlasticSourceControlChangesetRef Changeset) { return FString::FromInt(Changeset->ChangesetId); })
		.HeaderRow
		(
			SNew(SHeaderRow)
			.CanSelectGeneratedColumn(true)
			.HiddenColumnsList(HiddenColumnsList)
			.OnHiddenColumnsListChanged(this, &SPlasticSourceControlChangesetsWidget::OnHiddenColumnsListChanged)

			+SHeaderRow::Column(PlasticSourceControlChangesetsListViewColumn::ChangesetId::Id())
			.DefaultLabel(PlasticSourceControlChangesetsListViewColumn::ChangesetId::GetDisplayText())
			.DefaultTooltip(PlasticSourceControlChangesetsListViewColumn::ChangesetId::GetToolTipText())
			.ShouldGenerateWidget(true) // Ensure the column cannot be hidden (grayed out in the show/hide drop down menu)
			.FillWidth(0.6f)
			.SortPriority(this, &SPlasticSourceControlChangesetsWidget::GetColumnSortPriority, PlasticSourceControlChangesetsListViewColumn::ChangesetId::Id())
			.SortMode(this, &SPlasticSourceControlChangesetsWidget::GetColumnSortMode, PlasticSourceControlChangesetsListViewColumn::ChangesetId::Id())
			.OnSort(this, &SPlasticSourceControlChangesetsWidget::OnColumnSortModeChanged)

			+SHeaderRow::Column(PlasticSourceControlChangesetsListViewColumn::CreatedBy::Id())
			.DefaultLabel(PlasticSourceControlChangesetsListViewColumn::CreatedBy::GetDisplayText())
			.DefaultTooltip(PlasticSourceControlChangesetsListViewColumn::CreatedBy::GetToolTipText())
			.FillWidth(2.5f)
			.SortPriority(this, &SPlasticSourceControlChangesetsWidget::GetColumnSortPriority, PlasticSourceControlChangesetsListViewColumn::CreatedBy::Id())
			.SortMode(this, &SPlasticSourceControlChangesetsWidget::GetColumnSortMode, PlasticSourceControlChangesetsListViewColumn::CreatedBy::Id())
			.OnSort(this, &SPlasticSourceControlChangesetsWidget::OnColumnSortModeChanged)

			+SHeaderRow::Column(PlasticSourceControlChangesetsListViewColumn::Date::Id())
			.DefaultLabel(PlasticSourceControlChangesetsListViewColumn::Date::GetDisplayText())
			.DefaultTooltip(PlasticSourceControlChangesetsListViewColumn::Date::GetToolTipText())
			.FillWidth(1.5f)
			.SortPriority(this, &SPlasticSourceControlChangesetsWidget::GetColumnSortPriority, PlasticSourceControlChangesetsListViewColumn::Date::Id())
			.SortMode(this, &SPlasticSourceControlChangesetsWidget::GetColumnSortMode, PlasticSourceControlChangesetsListViewColumn::Date::Id())
			.OnSort(this, &SPlasticSourceControlChangesetsWidget::OnColumnSortModeChanged)

			+SHeaderRow::Column(PlasticSourceControlChangesetsListViewColumn::Comment::Id())
			.DefaultLabel(PlasticSourceControlChangesetsListViewColumn::Comment::GetDisplayText())
			.DefaultTooltip(PlasticSourceControlChangesetsListViewColumn::Comment::GetToolTipText())
			.FillWidth(5.0f)
			.SortPriority(this, &SPlasticSourceControlChangesetsWidget::GetColumnSortPriority, PlasticSourceControlChangesetsListViewColumn::Comment::Id())
			.SortMode(this, &SPlasticSourceControlChangesetsWidget::GetColumnSortMode, PlasticSourceControlChangesetsListViewColumn::Comment::Id())
			.OnSort(this, &SPlasticSourceControlChangesetsWidget::OnColumnSortModeChanged)

			+SHeaderRow::Column(PlasticSourceControlChangesetsListViewColumn::Branch::Id())
			.DefaultLabel(PlasticSourceControlChangesetsListViewColumn::Branch::GetDisplayText())
			.DefaultTooltip(PlasticSourceControlChangesetsListViewColumn::Branch::GetToolTipText())
			.FillWidth(2.0f)
			.SortPriority(this, &SPlasticSourceControlChangesetsWidget::GetColumnSortPriority, PlasticSourceControlChangesetsListViewColumn::Branch::Id())
			.SortMode(this, &SPlasticSourceControlChangesetsWidget::GetColumnSortMode, PlasticSourceControlChangesetsListViewColumn::Branch::Id())
			.OnSort(this, &SPlasticSourceControlChangesetsWidget::OnColumnSortModeChanged)
		);

	ChangesetsListView = ChangesetView;

	return ChangesetView;
}

TSharedRef<ITableRow> SPlasticSourceControlChangesetsWidget::OnGenerateRow(FPlasticSourceControlChangesetRef InChangeset, const TSharedRef<STableViewBase>& OwnerTable)
{
	const bool bIsCurrentChangeset = InChangeset->ChangesetId == CurrentChangesetId;
	return SNew(SPlasticSourceControlChangesetRow, OwnerTable)
		.ChangesetToVisualize(InChangeset)
		.bIsCurrentChangeset(bIsCurrentChangeset)
		.HighlightText_Lambda([this]() { return FileSearchBox->GetText(); });
}

void SPlasticSourceControlChangesetsWidget::OnHiddenColumnsListChanged()
{
	// Update and save config to reload it on the next Editor sessions
	if (ChangesetsListView && ChangesetsListView->GetHeaderRow())
	{
		UPlasticSourceControlProjectSettings* Settings = GetMutableDefault<UPlasticSourceControlProjectSettings>();
		Settings->bShowChangesetCreatedByColumn = true;
		Settings->bShowChangesetDateColumn = true;
		Settings->bShowChangesetCommentColumn = true;
		Settings->bShowChangesetBranchColumn = true;

		for (const FName& ColumnId : ChangesetsListView->GetHeaderRow()->GetHiddenColumnIds())
		{
			if (ColumnId == PlasticSourceControlChangesetsListViewColumn::CreatedBy::Id())
			{
				Settings->bShowChangesetCreatedByColumn = false;
			}
			else if (ColumnId == PlasticSourceControlChangesetsListViewColumn::Date::Id())
			{
				Settings->bShowChangesetDateColumn = false;
			}
			else if (ColumnId == PlasticSourceControlChangesetsListViewColumn::Comment::Id())
			{
				Settings->bShowChangesetCommentColumn = false;
			}
			else if (ColumnId == PlasticSourceControlChangesetsListViewColumn::Branch::Id())
			{
				Settings->bShowChangesetBranchColumn = false;
			}
		}
		Settings->SaveConfig();
	}
}

void SPlasticSourceControlChangesetsWidget::OnSearchTextChanged(const FText& InFilterText)
{
	SearchTextFilter->SetRawFilterText(InFilterText);
	FileSearchBox->SetError(SearchTextFilter->GetFilterErrorText());
}

void SPlasticSourceControlChangesetsWidget::PopulateItemSearchStrings(const FPlasticSourceControlChangeset& InItem, TArray<FString>& OutStrings)
{
	InItem.PopulateSearchString(OutStrings);
}

void SPlasticSourceControlChangesetsWidget::OnFromDateChanged(int32 InFromDateInDays)
{
	FromDateInDays = InFromDateInDays;

	RequestChangesetsRefresh();
}

TSharedRef<SWidget> SPlasticSourceControlChangesetsWidget::BuildFromDateDropDownMenu()
{
	FMenuBuilder MenuBuilder(true, NULL);

	for (const auto & FromDateInDaysValue : FromDateInDaysValues)
	{
		FUIAction MenuAction(FExecuteAction::CreateSP(this, &SPlasticSourceControlChangesetsWidget::OnFromDateChanged, FromDateInDaysValue.Key));
		MenuBuilder.AddMenuEntry(FromDateInDaysValue.Value, FromDateInDaysValue.Value, FSlateIcon(), MenuAction);
	}

	return MenuBuilder.MakeWidget();
}

void SPlasticSourceControlChangesetsWidget::OnRefreshUI()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SPlasticSourceControlChangesetsWidget::OnRefreshUI);

	const int32 ItemCount = SourceControlChangesets.Num();
	ChangesetRows.Empty(ItemCount);
	for (int32 ItemIndex = 0; ItemIndex < ItemCount; ++ItemIndex)
	{
		const FPlasticSourceControlChangesetRef& Item = SourceControlChangesets[ItemIndex];
		if (SearchTextFilter->PassesFilter(Item.Get()))
		{
			ChangesetRows.Emplace(Item);
		}
	}

	if (GetListView())
	{
		SortChangesetView();
		GetListView()->RequestListRefresh();
	}
}

EColumnSortPriority::Type SPlasticSourceControlChangesetsWidget::GetColumnSortPriority(const FName InColumnId) const
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

EColumnSortMode::Type SPlasticSourceControlChangesetsWidget::GetColumnSortMode(const FName InColumnId) const
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

void SPlasticSourceControlChangesetsWidget::OnColumnSortModeChanged(const EColumnSortPriority::Type InSortPriority, const FName& InColumnId, const EColumnSortMode::Type InSortMode)
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
		SortChangesetView();
		GetListView()->RequestListRefresh();
	}
}

void SPlasticSourceControlChangesetsWidget::SortChangesetView()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SPlasticSourceControlChangesetsWidget::SortChangesetView);

	if (PrimarySortedColumn.IsNone() || ChangesetRows.Num() == 0)
	{
		return; // No column selected for sorting or nothing to sort.
	}

	auto CompareChangesetIds = [](const FPlasticSourceControlChangeset* Lhs, const FPlasticSourceControlChangeset* Rhs)
	{
		return Lhs->ChangesetId < Rhs->ChangesetId ? -1 : (Lhs->ChangesetId == Rhs->ChangesetId ? 0 : 1);
	};

	auto CompareCreatedBys = [](const FPlasticSourceControlChangeset* Lhs, const FPlasticSourceControlChangeset* Rhs)
	{
		return FCString::Stricmp(*Lhs->CreatedBy, *Rhs->CreatedBy);
	};

	auto CompareDates = [](const FPlasticSourceControlChangeset* Lhs, const FPlasticSourceControlChangeset* Rhs)
	{
		return Lhs->Date < Rhs->Date ? -1 : (Lhs->Date == Rhs->Date ? 0 : 1);
	};

	auto CompareComments = [](const FPlasticSourceControlChangeset* Lhs, const FPlasticSourceControlChangeset* Rhs)
	{
		return FCString::Stricmp(*Lhs->Comment, *Rhs->Comment);
	};

	auto CompareBranches = [](const FPlasticSourceControlChangeset* Lhs, const FPlasticSourceControlChangeset* Rhs)
	{
		return FCString::Stricmp(*Lhs->Branch, *Rhs->Branch);
	};

	auto GetCompareFunc = [&](const FName& ColumnId)
	{
		if (ColumnId == PlasticSourceControlChangesetsListViewColumn::ChangesetId::Id())
		{
			return TFunction<int32(const FPlasticSourceControlChangeset*, const FPlasticSourceControlChangeset*)>(CompareChangesetIds);
		}
		else if (ColumnId == PlasticSourceControlChangesetsListViewColumn::CreatedBy::Id())
		{
			return TFunction<int32(const FPlasticSourceControlChangeset*, const FPlasticSourceControlChangeset*)>(CompareCreatedBys);
		}
		else if (ColumnId == PlasticSourceControlChangesetsListViewColumn::Date::Id())
		{
			return TFunction<int32(const FPlasticSourceControlChangeset*, const FPlasticSourceControlChangeset*)>(CompareDates);
		}
		else if (ColumnId == PlasticSourceControlChangesetsListViewColumn::Comment::Id())
		{
			return TFunction<int32(const FPlasticSourceControlChangeset*, const FPlasticSourceControlChangeset*)>(CompareComments);
		}
		else if (ColumnId == PlasticSourceControlChangesetsListViewColumn::Branch::Id())
		{
			return TFunction<int32(const FPlasticSourceControlChangeset*, const FPlasticSourceControlChangeset*)>(CompareBranches);
		}
		else
		{
			checkNoEntry();
			return TFunction<int32(const FPlasticSourceControlChangeset*, const FPlasticSourceControlChangeset*)>();
		};
	};

	TFunction<int32(const FPlasticSourceControlChangeset*, const FPlasticSourceControlChangeset*)> PrimaryCompare = GetCompareFunc(PrimarySortedColumn);
	TFunction<int32(const FPlasticSourceControlChangeset*, const FPlasticSourceControlChangeset*)> SecondaryCompare;
	if (!SecondarySortedColumn.IsNone())
	{
		SecondaryCompare = GetCompareFunc(SecondarySortedColumn);
	}

	if (PrimarySortMode == EColumnSortMode::Ascending)
	{
		// NOTE: StableSort() would give a better experience when the sorted columns(s) has the same values and new values gets added, but it is slower
		//       with large changelists (7600 items was about 1.8x slower in average measured with Unreal Insight). Because this code runs in the main
		//       thread and can be invoked a lot, the trade off went if favor of speed.
		ChangesetRows.Sort([this, &PrimaryCompare, &SecondaryCompare](const FPlasticSourceControlChangesetPtr& Lhs, const FPlasticSourceControlChangesetPtr& Rhs)
		{
			int32 Result = PrimaryCompare(static_cast<FPlasticSourceControlChangeset*>(Lhs.Get()), static_cast<FPlasticSourceControlChangeset*>(Rhs.Get()));
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
				return SecondaryCompare(static_cast<FPlasticSourceControlChangeset*>(Lhs.Get()), static_cast<FPlasticSourceControlChangeset*>(Rhs.Get())) < 0;
			}
			else
			{
				return SecondaryCompare(static_cast<FPlasticSourceControlChangeset*>(Lhs.Get()), static_cast<FPlasticSourceControlChangeset*>(Rhs.Get())) > 0;
			}
		});
	}
	else
	{
		ChangesetRows.Sort([this, &PrimaryCompare, &SecondaryCompare](const FPlasticSourceControlChangesetPtr& Lhs, const FPlasticSourceControlChangesetPtr& Rhs)
		{
			int32 Result = PrimaryCompare(static_cast<FPlasticSourceControlChangeset*>(Lhs.Get()), static_cast<FPlasticSourceControlChangeset*>(Rhs.Get()));
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
				return SecondaryCompare(static_cast<FPlasticSourceControlChangeset*>(Lhs.Get()), static_cast<FPlasticSourceControlChangeset*>(Rhs.Get())) < 0;
			}
			else
			{
				return SecondaryCompare(static_cast<FPlasticSourceControlChangeset*>(Lhs.Get()), static_cast<FPlasticSourceControlChangeset*>(Rhs.Get())) > 0;
			}
		});
	}
}

TSharedPtr<SWidget> SPlasticSourceControlChangesetsWidget::OnOpenContextMenu()
{
	const TArray<FPlasticSourceControlChangesetRef> SelectedChangesets = ChangesetsListView->GetSelectedItems();
	if (SelectedChangesets.Num() == 0)
	{
		return nullptr;
	}

	// Detect if all selected changesets are from the same branch
	bool bSingleBranchSelected = false;
	FPlasticSourceControlChangesetPtr SelectedChangeset;
	if (SelectedChangesets.Num() >= 1)
	{
		SelectedChangeset = SelectedChangesets[0];
		bSingleBranchSelected = true;
		for (int32 i = 1; i < SelectedChangesets.Num(); i++)
		{
			if (SelectedChangesets[i]->Branch != SelectedChangeset->Branch)
			{
				bSingleBranchSelected = false;
				SelectedChangeset.Reset();
				break;
			}
		}
	}
	const bool bSingleSelection = (SelectedChangesets.Num() == 1);
	const bool bDoubleSelection = (SelectedChangesets.Num() == 2);
	const bool bSingleNotCurrent = bSingleSelection && (SelectedChangeset->ChangesetId != CurrentChangesetId);

	static const FText SelectASingleChangesetTooltip(LOCTEXT("SelectASingleChangesetTooltip", "Select a single changeset."));
	static const FText SelectADifferentChangesetTooltip(LOCTEXT("SelectADifferentChangesetTooltip", "Select a changeset that is not the current one."));
	static const FText SelectASingleBranchTooltip(LOCTEXT("SelectASingleBranchTooltip", "Select changesets from a single branch."));

	UToolMenus* ToolMenus = UToolMenus::Get();
	static const FName MenuName = "PlasticSourceControl.ChangesetsContextMenu";
	if (!ToolMenus->IsMenuRegistered(MenuName))
	{
		UToolMenu* RegisteredMenu = ToolMenus->RegisterMenu(MenuName);
		// Add section so it can be used as insert position for menu extensions
		RegisteredMenu->AddSection("Source Control");
	}

	// Build up the menu
	FToolMenuContext Context;
	UToolMenu* Menu = ToolMenus->GenerateMenu(MenuName, Context);

	FToolMenuSection& Section = *Menu->FindSection("Source Control");

	Section.AddMenuEntry(
		TEXT("DiffChangeset"),
		bSingleSelection ? FText::Format(LOCTEXT("DiffChangesetDynamic", "Diff changeset {0}"), FText::AsNumber(SelectedChangeset->ChangesetId)) : LOCTEXT("DiffChangeset", "Diff changeset"),
		bSingleSelection ? LOCTEXT("DiffChangesetTooltip", "Launch the Desktop application diff window showing changes in this changeset.") : SelectASingleChangesetTooltip,
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &SPlasticSourceControlChangesetsWidget::OnDiffChangesetClicked, SelectedChangeset),
			FCanExecuteAction::CreateLambda([bSingleSelection]() { return bSingleSelection; })
		)
	);

	Section.AddMenuEntry(
		TEXT("DiffChangesets"),
		LOCTEXT("DiffChangesets", "Diff selected changesets"),
		bDoubleSelection ? LOCTEXT("DiffChangesetTooltip", "Launch the Desktop application diff window showing changes between the two selected changesets.") : LOCTEXT("DoubleSelection", "Select a couple of changesets."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &SPlasticSourceControlChangesetsWidget::OnDiffChangesetsClicked, SelectedChangesets),
			FCanExecuteAction::CreateLambda([bDoubleSelection]() { return bDoubleSelection; })
		)
	);

	Section.AddSeparator("PlasticSeparator1");

	Section.AddMenuEntry(
		TEXT("DiffBranch"),
		bSingleSelection ? FText::Format(LOCTEXT("DiffBranchDynamic", "Diff branch {0}"), FText::FromString(SelectedChangeset->Branch)) : LOCTEXT("DiffBranch", "Diff branch"),
		bSingleBranchSelected ? LOCTEXT("DiffChangesetTooltip", "Launch the Desktop application diff window showing all changes in the selected branch.") : SelectASingleBranchTooltip,
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &SPlasticSourceControlChangesetsWidget::OnDiffBranchClicked, SelectedChangeset),
			FCanExecuteAction::CreateLambda([bSingleBranchSelected]() { return bSingleBranchSelected; })
		)
	);

	Section.AddSeparator("PlasticSeparator2");

	Section.AddMenuEntry(
		TEXT("SwitchToBranch"),
		bSingleSelection ? FText::Format(LOCTEXT("SwitchToBranchDynamic", "Switch workspace to the branch {0}"), FText::FromString(SelectedChangeset->Branch)) : LOCTEXT("SwitchToBranch", "Switch workspace to this branch"),
		bSingleBranchSelected ? LOCTEXT("SwitchToBranchTooltip", "Switch the workspace to the head of the branch with this changeset.") : SelectASingleBranchTooltip,
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &SPlasticSourceControlChangesetsWidget::OnSwitchToBranchClicked, SelectedChangeset),
			FCanExecuteAction::CreateLambda([bSingleBranchSelected]() { return bSingleBranchSelected; })
		)
	);

	Section.AddMenuEntry(
		TEXT("SwitchToChangeset"),
		bSingleSelection ? FText::Format(LOCTEXT("SwitchToChangesetDynamic", "Switch workspace to this changeset {0}"), FText::AsNumber(SelectedChangeset->ChangesetId)) : LOCTEXT("SwitchToChangeset", "Switch workspace to this changeset"),
		bSingleNotCurrent ? LOCTEXT("SwitchToChangesetsTooltip", "Switch the workspace to the specific changeset. Note that you won't be able to check in any change from it.") :
		bSingleSelection ? SelectADifferentChangesetTooltip : SelectASingleChangesetTooltip,
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &SPlasticSourceControlChangesetsWidget::OnSwitchToChangesetClicked, SelectedChangeset),
			FCanExecuteAction::CreateLambda([bSingleNotCurrent]() { return bSingleNotCurrent; })
		)
	);

	return ToolMenus->GenerateWidget(Menu);
}

void SPlasticSourceControlChangesetsWidget::OnDiffChangesetClicked(FPlasticSourceControlChangesetPtr InSelectedChangeset)
{
	if (InSelectedChangeset.IsValid())
	{
		PlasticSourceControlUtils::OpenDesktopApplicationForDiff(InSelectedChangeset->ChangesetId);
	}
}

void SPlasticSourceControlChangesetsWidget::OnDiffChangesetsClicked(TArray<FPlasticSourceControlChangesetRef> InSelectedChangesets)
{
	if (InSelectedChangesets.Num() == 2)
	{
		PlasticSourceControlUtils::OpenDesktopApplicationForDiff(InSelectedChangesets[0]->ChangesetId, InSelectedChangesets[1]->ChangesetId);
	}
}

void SPlasticSourceControlChangesetsWidget::OnDiffBranchClicked(FPlasticSourceControlChangesetPtr InSelectedChangeset)
{
	if (InSelectedChangeset.IsValid())
	{
		PlasticSourceControlUtils::OpenDesktopApplicationForDiff(InSelectedChangeset->Branch);
	}
}

void SPlasticSourceControlChangesetsWidget::OnSwitchToBranchClicked(FPlasticSourceControlChangesetPtr InSelectedChangeset)
{
	if (!InSelectedChangeset.IsValid())
		return;

	if (!Notification.IsInProgress())
	{
		// Warn the user about any unsaved assets (risk of losing work) but don't enforce saving them. Saving and checking out these assets will make the switch to the branch fail.
		PackageUtils::SaveDirtyPackages();

		// Find and Unlink all loaded packages in Content directory to allow to update them
		PackageUtils::UnlinkPackages(PackageUtils::ListAllPackages());

		// Launch a custom "Switch" operation
		FPlasticSourceControlProvider& Provider = FPlasticSourceControlModule::Get().GetProvider();
		TSharedRef<FPlasticSwitch, ESPMode::ThreadSafe> SwitchToBranchOperation = ISourceControlOperation::Create<FPlasticSwitch>();
		SwitchToBranchOperation->BranchName = InSelectedChangeset->Branch;
		const ECommandResult::Type Result = Provider.Execute(SwitchToBranchOperation, TArray<FString>(), EConcurrency::Asynchronous, FSourceControlOperationComplete::CreateSP(this, &SPlasticSourceControlChangesetsWidget::OnSwitchToBranchOperationComplete));
		if (Result == ECommandResult::Succeeded)
		{
			// Display an ongoing notification during the whole operation (packages will be reloaded at the completion of the operation)
			Notification.DisplayInProgress(SwitchToBranchOperation->GetInProgressString());
			StartRefreshStatus();
		}
		else
		{
			// Report failure with a notification (but nothing need to be reloaded since no local change is expected)
			FNotification::DisplayFailure(SwitchToBranchOperation.Get());
		}
	}
	else
	{
		FMessageLog SourceControlLog("SourceControl");
		SourceControlLog.Warning(LOCTEXT("SourceControlMenu_InProgress", "Source control operation already in progress"));
		SourceControlLog.Notify();
	}
}

void SPlasticSourceControlChangesetsWidget::OnSwitchToChangesetClicked(FPlasticSourceControlChangesetPtr InSelectedChangeset)
{
	const FText SwitchConfirmation = FText::Format(LOCTEXT("SwitchToChangesetDialog", "Are you sure you want to switch the workspace to the changeset {0} instead of a branch? You won't be able to check in any change from it."),
		FText::AsNumber(InSelectedChangeset->ChangesetId));
	const EAppReturnType::Type Choice = FMessageDialog::Open(
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
		EAppMsgCategory::Info,
#endif
		EAppMsgType::YesNo, SwitchConfirmation
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
		, LOCTEXT("SwitchToChangesetTitle", "Switch to changeset?")
#endif
	);
	if (Choice == EAppReturnType::Yes)
	{
		if (!Notification.IsInProgress())
		{
			// Launch a custom "SyncAll" operation (using the underlying standard "Sync" operation)

			// Warn the user about any unsaved assets (risk of losing work) but don't enforce saving them. Saving and checking out these assets will make the switch to the branch fail.
			PackageUtils::SaveDirtyPackages();

			// Find and Unlink all loaded packages in Content directory to allow to update them
			PackageUtils::UnlinkPackages(PackageUtils::ListAllPackages());

			TSharedRef<FPlasticSyncAll, ESPMode::ThreadSafe> SwitchToChangesetOperation = ISourceControlOperation::Create<FPlasticSyncAll>();
			SwitchToChangesetOperation->SetRevision(FString::FromInt(InSelectedChangeset->ChangesetId));
			FPlasticSourceControlProvider& Provider = FPlasticSourceControlModule::Get().GetProvider();
			const ECommandResult::Type Result = Provider.Execute(SwitchToChangesetOperation, TArray<FString>(), EConcurrency::Asynchronous, FSourceControlOperationComplete::CreateSP(this, &SPlasticSourceControlChangesetsWidget::OnSwitchToChangesetOperationComplete));
			if (Result == ECommandResult::Succeeded)
			{
				// Display an ongoing notification during the whole operation (packages will be reloaded at the completion of the operation)
				Notification.DisplayInProgress(SwitchToChangesetOperation->GetInProgressString());
				StartRefreshStatus();
			}
			else
			{
				// Report failure with a notification (but nothing need to be reloaded since no local change is expected)
				FNotification::DisplayFailure(SwitchToChangesetOperation.Get());
			}
		}
		else
		{
			FMessageLog SourceControlLog("SourceControl");
			SourceControlLog.Warning(LOCTEXT("SourceControlMenu_InProgress", "Source control operation already in progress"));
			SourceControlLog.Notify();
		}
	}
}

void SPlasticSourceControlChangesetsWidget::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (!ISourceControlModule::Get().IsEnabled() || (!FPlasticSourceControlModule::Get().GetProvider().IsAvailable()))
	{
		return;
	}

	// Detect transitions of the source control being available/unavailable. Ex: When the user changes the source control in UI, the provider gets selected,
	// but it is not connected/available until the user accepts the settings. The source control doesn't have callback for availability and we want to refresh everything
	// once it gets available.
	if (ISourceControlModule::Get().IsEnabled() && !bSourceControlAvailable && ISourceControlModule::Get().GetProvider().IsAvailable())
	{
		bSourceControlAvailable = true;
		bShouldRefresh = true;
	}

	if (bShouldRefresh)
	{
		RequestChangesetsRefresh();
		bShouldRefresh = false;
	}

	if (bIsRefreshing)
	{
		TickRefreshStatus(InDeltaTime);
	}
}

void SPlasticSourceControlChangesetsWidget::StartRefreshStatus()
{
	if (!bIsRefreshing)
	{
		bIsRefreshing = true;
		RefreshStatusStartSecs = FPlatformTime::Seconds();
	}
}

void SPlasticSourceControlChangesetsWidget::TickRefreshStatus(double InDeltaTime)
{
	const int32 RefreshStatusTimeElapsed = static_cast<int32>(FPlatformTime::Seconds() - RefreshStatusStartSecs);
	RefreshStatus = FText::Format(LOCTEXT("PlasticSourceControl_RefreshChangesets", "Refreshing changesets... ({0} s)"), FText::AsNumber(RefreshStatusTimeElapsed));
}

void SPlasticSourceControlChangesetsWidget::EndRefreshStatus()
{
	bIsRefreshing = false;
	RefreshStatus = FText::GetEmpty();
}

void SPlasticSourceControlChangesetsWidget::RequestChangesetsRefresh()
{
	if (!ISourceControlModule::Get().IsEnabled() || (!FPlasticSourceControlModule::Get().GetProvider().IsAvailable()))
	{
		return;
	}

	StartRefreshStatus();

	TSharedRef<FPlasticGetChangesets, ESPMode::ThreadSafe> GetChangesetsOperation = ISourceControlOperation::Create<FPlasticGetChangesets>();
	if (FromDateInDays > -1)
	{
		GetChangesetsOperation->FromDate = FDateTime::Now() - FTimespan::FromDays(FromDateInDays);
	}

	FPlasticSourceControlProvider& Provider = FPlasticSourceControlModule::Get().GetProvider();
	Provider.Execute(GetChangesetsOperation, EConcurrency::Asynchronous, FSourceControlOperationComplete::CreateSP(this, &SPlasticSourceControlChangesetsWidget::OnGetChangesetsOperationComplete));
}

void SPlasticSourceControlChangesetsWidget::OnGetChangesetsOperationComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SPlasticSourceControlChangesetsWidget::OnGetChangesetsOperationComplete);

	TSharedRef<FPlasticGetChangesets, ESPMode::ThreadSafe> OperationGetChangesets = StaticCastSharedRef<FPlasticGetChangesets>(InOperation);
	SourceControlChangesets = MoveTemp(OperationGetChangesets->Changesets);

	CurrentChangesetId = FPlasticSourceControlModule::Get().GetProvider().GetChangesetNumber();

	EndRefreshStatus();
	OnRefreshUI();
}

void SPlasticSourceControlChangesetsWidget::OnSwitchToBranchOperationComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SPlasticSourceControlChangesetsWidget::OnSwitchToBranchOperationComplete);

	// Reload packages that where updated by the SwitchToBranch operation (and the current map if needed)
	TSharedRef<FPlasticSwitch, ESPMode::ThreadSafe> SwitchToBranchOperation = StaticCastSharedRef<FPlasticSwitch>(InOperation);
	PackageUtils::ReloadPackages(SwitchToBranchOperation->UpdatedFiles);

	// Ask for a full refresh of the list of branches (and don't call EndRefreshStatus() yet)
	bShouldRefresh = true;

	Notification.RemoveInProgress();

	FNotification::DisplayResult(InOperation, InResult);
}

void SPlasticSourceControlChangesetsWidget::OnSwitchToChangesetOperationComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SPlasticSourceControlChangesetsWidget::OnSwitchToChangesetOperationComplete);

	// Reload packages that where updated by the SwitchToChangeset operation (and the current map if needed)
	TSharedRef<FPlasticSyncAll, ESPMode::ThreadSafe> SwitchToChangesetOperation = StaticCastSharedRef<FPlasticSyncAll>(InOperation);
	PackageUtils::ReloadPackages(SwitchToChangesetOperation->UpdatedFiles);

	// Ask for a full refresh of the list of branches (and don't call EndRefreshStatus() yet)
	bShouldRefresh = true;

	Notification.RemoveInProgress();

	FNotification::DisplayResult(InOperation, InResult);
}

void SPlasticSourceControlChangesetsWidget::OnSourceControlProviderChanged(ISourceControlProvider& OldProvider, ISourceControlProvider& NewProvider)
{
	bSourceControlAvailable = NewProvider.IsAvailable(); // Check if it is connected.
	bShouldRefresh = true;

	if (&NewProvider != &OldProvider)
	{
		ChangesetRows.Reset();
		if (GetListView())
		{
			GetListView()->RequestListRefresh();
		}
	}
}

void SPlasticSourceControlChangesetsWidget::HandleSourceControlStateChanged()
{
	bShouldRefresh = true;
	if (GetListView())
	{
		GetListView()->RequestListRefresh();
	}
}

void SPlasticSourceControlChangesetsWidget::SwitchToChangesetWithConfirmation(const FString& InSelectedChangeset)
{}

void SPlasticSourceControlChangesetsWidget::OnItemDoubleClicked(FPlasticSourceControlChangesetRef InChangeset)
{
	OnDiffChangesetClicked(InChangeset);
}

FReply SPlasticSourceControlChangesetsWidget::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::F5)
	{
		// Pressing F5 refreshes the list of changesets
		RequestChangesetsRefresh();
		return FReply::Handled();
	}
	else if (InKeyEvent.GetKey() == EKeys::Enter)
	{
		// Pressing Enter open the diff for the selected changeset (like a double click)
		const TArray<FPlasticSourceControlChangesetRef> SelectedChangesets = ChangesetsListView->GetSelectedItems();
		if (SelectedChangesets.Num() == 1)
		{
			OnDiffChangesetClicked(SelectedChangesets[0]);
		}
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

#undef LOCTEXT_NAMESPACE
