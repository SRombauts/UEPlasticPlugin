// Copyright (c) 2023 Unity Technologies

#include "SPlasticSourceControlBranchesWidget.h"

#include "PlasticSourceControlModule.h"
#include "PlasticSourceControlOperations.h"
#include "PlasticSourceControlProjectSettings.h"
#include "PlasticSourceControlBranch.h"
#include "SPlasticSourceControlBranchRow.h"

#include "ISourceControlModule.h"

#include "Runtime/Launch/Resources/Version.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
#include "Misc/ComparisonUtility.h"
#endif
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "Styling/AppStyle.h"
#else
#include "EditorStyleSet.h"
#endif
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"

#define LOCTEXT_NAMESPACE "PlasticSourceControlWindow"

void SPlasticSourceControlBranchesWidget::Construct(const FArguments& InArgs)
{
	ISourceControlModule::Get().RegisterProviderChanged(FSourceControlProviderChanged::FDelegate::CreateSP(this, &SPlasticSourceControlBranchesWidget::OnSourceControlProviderChanged));

	CurrentBranchName = FPlasticSourceControlModule::Get().GetProvider().GetBranchName();

	SearchTextFilter = MakeShared<TTextFilter<const FPlasticSourceControlBranch&>>(TTextFilter<const FPlasticSourceControlBranch&>::FItemToStringArray::CreateSP(this, &SPlasticSourceControlBranchesWidget::PopulateItemSearchStrings));
	SearchTextFilter->OnChanged().AddSP(this, &SPlasticSourceControlBranchesWidget::OnRefreshUI);

	FromDateInDaysValues.Add({ 7, FText::FromString(TEXT("Last week")) });
	FromDateInDaysValues.Add({ 30, FText::FromString(TEXT("Last month")) });
	FromDateInDaysValues.Add({ 90, FText::FromString(TEXT("Last 3 months")) });
	FromDateInDaysValues.Add({ 365, FText::FromString(TEXT("Last year")) });
	FromDateInDaysValues.Add({ -1, FText::FromString(TEXT("All time")) });

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
			.Padding(4)
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
				.MaxWidth(10)
				[
					SNew(SSpacer)
				]
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.MaxWidth(300)
				[
					SAssignNew(FileSearchBox, SSearchBox)
					.HintText(LOCTEXT("SearchBranches", "Search Branches"))
					.ToolTipText(LOCTEXT("PlasticBranchesSearch_Tooltip", "Filter the list of branches by keyword."))
					.OnTextChanged(this, &SPlasticSourceControlBranchesWidget::OnSearchTextChanged)
				]
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.MaxWidth(125)
				.Padding(FMargin(10.f, 0.f))
				[
					SNew(SComboButton)
					.ToolTipText(LOCTEXT("PlasticBranchesDate_Tooltip", "Filter the list of branches by date of creation."))
					.OnGetMenuContent(this, &SPlasticSourceControlBranchesWidget::BuildFromDateDropDownMenu)
					.ButtonContent()
					[
						SNew(STextBlock)
						.Text_Lambda([this]() { return FromDateInDaysValues[FromDateInDays]; })
					]
				]
			]
		]
		+SVerticalBox::Slot() // The main content: the list of branches
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
					.Text_Lambda([this]() { return FText::FromString(CurrentBranchName); })
					.ToolTipText(LOCTEXT("PlasticBranchCurrent_Tooltip", "Current branch."))
				]
			]
		]
	];
}

TSharedRef<SWidget> SPlasticSourceControlBranchesWidget::CreateToolBar()
{
#if ENGINE_MAJOR_VERSION >= 5
	FSlimHorizontalToolBarBuilder ToolBarBuilder(nullptr, FMultiBoxCustomization::None);
#else
	FToolBarBuilder ToolBarBuilder(nullptr, FMultiBoxCustomization::None);
#endif

	ToolBarBuilder.AddToolBarButton(
		FUIAction(
			FExecuteAction::CreateLambda([this]() { RequestBranchesRefresh(); })),
		NAME_None,
		LOCTEXT("SourceControl_RefreshButton", "Refresh"),
		LOCTEXT("SourceControl_RefreshButton_Tooltip", "Refreshes branches from revision control provider."),
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "SourceControl.Actions.Refresh"));
#else
		FSlateIcon(FEditorStyle::GetStyleSetName(), "SourceControl.Actions.Refresh"));
#endif

	return ToolBarBuilder.MakeWidget();
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
		.OnContextMenuOpening(this, &SPlasticSourceControlBranchesWidget::OnOpenContextMenu)
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
	const bool bIsCurrentBranch = InBranch->Name == CurrentBranchName;
	return SNew(SPlasticSourceControlBranchRow, OwnerTable)
		.BranchToVisualize(InBranch)
		.bIsCurrentBranch(bIsCurrentBranch)
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
	SearchTextFilter->SetRawFilterText(InFilterText);
	FileSearchBox->SetError(SearchTextFilter->GetFilterErrorText());
}

void SPlasticSourceControlBranchesWidget::PopulateItemSearchStrings(const FPlasticSourceControlBranch& InItem, TArray<FString>& OutStrings)
{
	InItem.PopulateSearchString(OutStrings);
}

void SPlasticSourceControlBranchesWidget::OnFromDateChanged(int32 InFromDateInDays)
{
	FromDateInDays = InFromDateInDays;

	RequestBranchesRefresh();
}

TSharedRef<SWidget> SPlasticSourceControlBranchesWidget::BuildFromDateDropDownMenu()
{
	FMenuBuilder MenuBuilder(true, NULL);

	for (const auto & FromDateInDaysValue : FromDateInDaysValues)
	{
		FUIAction MenuAction(FExecuteAction::CreateSP(this, &SPlasticSourceControlBranchesWidget::OnFromDateChanged, FromDateInDaysValue.Key));
		MenuBuilder.AddMenuEntry(FromDateInDaysValue.Value, FromDateInDaysValue.Value, FSlateIcon(), MenuAction);
	}

	return MenuBuilder.MakeWidget();
}

void SPlasticSourceControlBranchesWidget::OnRefreshUI()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SPlasticSourceControlBranchesWidget::OnRefreshUI);

	const int32 ItemCount = SourceControlBranches.Num();
	BranchRows.Empty(ItemCount);
	for (int32 ItemIndex = 0; ItemIndex < ItemCount; ++ItemIndex)
	{
		const FPlasticSourceControlBranchRef& Item = SourceControlBranches[ItemIndex];
		if (SearchTextFilter->PassesFilter(Item.Get()))
		{
			BranchRows.Emplace(Item);
		}
	}

	if (GetListView())
	{
		SortBranchView();
		GetListView()->RequestListRefresh();
	}
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

	if (PrimarySortedColumn.IsNone() || BranchRows.Num() == 0)
	{
		return; // No column selected for sorting or nothing to sort.
	}

	auto CompareNames = [](const FPlasticSourceControlBranch* Lhs, const FPlasticSourceControlBranch* Rhs)
	{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
		return UE::ComparisonUtility::CompareNaturalOrder(*Lhs->Name, *Rhs->Name);
#else
		return FCString::Stricmp(*Lhs->Name, *Rhs->Name);
#endif
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
		BranchRows.Sort([this, &PrimaryCompare, &SecondaryCompare](const FPlasticSourceControlBranchPtr& Lhs, const FPlasticSourceControlBranchPtr& Rhs)
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
		BranchRows.Sort([this, &PrimaryCompare, &SecondaryCompare](const FPlasticSourceControlBranchPtr& Lhs, const FPlasticSourceControlBranchPtr& Rhs)
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

FString SPlasticSourceControlBranchesWidget::GetSelectedBranch()
{
	for (const FPlasticSourceControlBranchPtr& BranchPtr : BranchesListView->GetSelectedItems())
	{
		return BranchPtr->Name;
	}

	return FString();
}

TSharedPtr<SWidget> SPlasticSourceControlBranchesWidget::OnOpenContextMenu()
{
	const FString SelectedBranch = GetSelectedBranch();
	if (SelectedBranch.IsEmpty())
	{
		return nullptr;
	}

	UToolMenus* ToolMenus = UToolMenus::Get();
	static const FName MenuName = "PlasticSourceControl.BranchesContextMenu";
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
		TEXT("SwitchToBranch"),
		LOCTEXT("SwitchToBranch", "Switch workspace to this branch"),
		LOCTEXT("SwitchToBranchTooltip", "Switch workspace to this branch."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &SPlasticSourceControlBranchesWidget::OnSwitchToBranchClicked, SelectedBranch),
			FCanExecuteAction::CreateLambda([this, SelectedBranch]() { return SelectedBranch != CurrentBranchName; })
		)
	);


	return ToolMenus->GenerateWidget(Menu);
}

void SPlasticSourceControlBranchesWidget::OnSwitchToBranchClicked(FString InBranchName)
{
	// TODO Switch:
	UE_LOG(LogSourceControl, Log, TEXT("OnSwitchToBranchClicked(%s)"), *InBranchName);
}

void SPlasticSourceControlBranchesWidget::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
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
		RequestBranchesRefresh();
		bShouldRefresh = false;
	}

	if (bIsRefreshing)
	{
		TickRefreshStatus(InDeltaTime);
	}
}

void SPlasticSourceControlBranchesWidget::StartRefreshStatus()
{
	if (!bIsRefreshing)
	{
		bIsRefreshing = true;
		RefreshStatusStartSecs = FPlatformTime::Seconds();
	}
}

void SPlasticSourceControlBranchesWidget::TickRefreshStatus(double InDeltaTime)
{
	const int32 RefreshStatusTimeElapsed = static_cast<int32>(FPlatformTime::Seconds() - RefreshStatusStartSecs);
	RefreshStatus = FText::Format(LOCTEXT("PlasticSourceControl_RefreshBranches", "Refreshing branches... ({0} s)"), FText::AsNumber(RefreshStatusTimeElapsed));
}

void SPlasticSourceControlBranchesWidget::EndRefreshStatus()
{
	bIsRefreshing = false;
	RefreshStatus = FText::GetEmpty();
}

void SPlasticSourceControlBranchesWidget::RequestBranchesRefresh()
{
	if (!ISourceControlModule::Get().IsEnabled() || (!FPlasticSourceControlModule::Get().GetProvider().IsAvailable()))
	{
		return;
	}

	StartRefreshStatus();

	TSharedRef<FPlasticGetBranches, ESPMode::ThreadSafe> GetBranchesOperation = ISourceControlOperation::Create<FPlasticGetBranches>();
	if (FromDateInDays > -1)
	{
		GetBranchesOperation->FromDate = FDateTime::Now() - FTimespan::FromDays(FromDateInDays);
	}

	FPlasticSourceControlProvider& Provider = FPlasticSourceControlModule::Get().GetProvider();
	Provider.Execute(GetBranchesOperation, EConcurrency::Asynchronous, FSourceControlOperationComplete::CreateSP(this, &SPlasticSourceControlBranchesWidget::OnGetBranchesOperationComplete));
}

void SPlasticSourceControlBranchesWidget::OnGetBranchesOperationComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SPlasticSourceControlBranchesWidget::OnGetBranchesOperationComplete);

	TSharedRef<FPlasticGetBranches, ESPMode::ThreadSafe> OperationGetBranches = StaticCastSharedRef<FPlasticGetBranches>(InOperation);
	SourceControlBranches = MoveTemp(OperationGetBranches->Branches);

	CurrentBranchName = FPlasticSourceControlModule::Get().GetProvider().GetBranchName();

	EndRefreshStatus();
	OnRefreshUI();
}

void SPlasticSourceControlBranchesWidget::OnSourceControlProviderChanged(ISourceControlProvider& OldProvider, ISourceControlProvider& NewProvider)
{
	bSourceControlAvailable = NewProvider.IsAvailable(); // Check if it is connected.
	bShouldRefresh = true;

	if (&NewProvider != &OldProvider)
	{
		BranchRows.Reset();
		if (GetListView())
		{
			GetListView()->RequestListRefresh();
		}
	}
}

#undef LOCTEXT_NAMESPACE
