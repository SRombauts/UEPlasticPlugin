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

	const FString OrganizationName = FPlasticSourceControlModule::Get().GetProvider().GetCloudOrganization();

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
		// TODO menu
		// .OnContextMenuOpening(this, &SPlasticSourceControlChangesetsWidget::OnOpenContextMenu)
		// .OnMouseButtonDoubleClick(this, &SPlasticSourceControlChangesetsWidget::OnItemDoubleClicked)
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
			.FillWidth(0.5f)
			.SortPriority(this, &SPlasticSourceControlChangesetsWidget::GetColumnSortPriority, PlasticSourceControlChangesetsListViewColumn::ChangesetId::Id())
			.SortMode(this, &SPlasticSourceControlChangesetsWidget::GetColumnSortMode, PlasticSourceControlChangesetsListViewColumn::ChangesetId::Id())
			.OnSort(this, &SPlasticSourceControlChangesetsWidget::OnColumnSortModeChanged)

			+SHeaderRow::Column(PlasticSourceControlChangesetsListViewColumn::CreatedBy::Id())
			.DefaultLabel(PlasticSourceControlChangesetsListViewColumn::CreatedBy::GetDisplayText())
			.DefaultTooltip(PlasticSourceControlChangesetsListViewColumn::CreatedBy::GetToolTipText())
			.FillWidth(4.0f)
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
			.FillWidth(2.0f)
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
	return SNew(SPlasticSourceControlChangesetRow, OwnerTable)
		.ChangesetToVisualize(InChangeset)
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
		return (Lhs->ChangesetId < Rhs->ChangesetId);
	};

	auto CompareCreatedBys = [](const FPlasticSourceControlChangeset* Lhs, const FPlasticSourceControlChangeset* Rhs)
	{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
		return UE::ComparisonUtility::CompareNaturalOrder(*Lhs->CreatedBy, *Rhs->CreatedBy);
#else
		return FCString::Stricmp(*Lhs->CreatedBy, *Rhs->CreatedBy);
#endif
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
		else if (ColumnId == PlasticSourceControlChangesetsListViewColumn::CreatedBy::Id())
		{
			return TFunction<int32(const FPlasticSourceControlChangeset*, const FPlasticSourceControlChangeset*)>(CompareCreatedBys);
		}
		else if (ColumnId == PlasticSourceControlChangesetsListViewColumn::Date::Id())
		{
			return TFunction<int32(const FPlasticSourceControlChangeset*, const FPlasticSourceControlChangeset*)>(CompareDates);
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

FReply SPlasticSourceControlChangesetsWidget::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::F5)
	{
		// Pressing F5 refreshes the list of changesets
		RequestChangesetsRefresh();
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

#undef LOCTEXT_NAMESPACE
