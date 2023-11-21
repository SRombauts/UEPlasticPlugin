// Copyright (c) 2023 Unity Technologies

#pragma once

#include "CoreMinimal.h"
#include "Misc/TextFilter.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

#include "ISourceControlOperation.h"
#include "ISourceControlProvider.h"

typedef TSharedRef<class FPlasticSourceControlBranch, ESPMode::ThreadSafe> FPlasticSourceControlBranchRef;

class SSearchBox;

// Widget displaying the list of branches in the tab window, see FPlasticSourceControlBranchesWindow
class SPlasticSourceControlBranchesWidget : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SPlasticSourceControlBranchesWidget) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	TSharedRef<SWidget> CreateToolBar();
	TSharedRef<SWidget> CreateContentPanel();

	TSharedRef<ITableRow> OnGenerateRow(FPlasticSourceControlBranchRef InBranch, const TSharedRef<STableViewBase>& OwnerTable);
	void OnHiddenColumnsListChanged();

	void OnSearchTextChanged(const FText& InFilterText);
	void PopulateItemSearchStrings(const FPlasticSourceControlBranch& InItem, TArray<FString>& OutStrings);

	TSharedRef<SWidget> BuildFromDateDropDownMenu();
	void OnFromDateChanged(int32 InFromDateInDays);

	void OnRefreshUI();

	EColumnSortPriority::Type GetColumnSortPriority(const FName InColumnId) const;
	EColumnSortMode::Type GetColumnSortMode(const FName InColumnId) const;
	void OnColumnSortModeChanged(const EColumnSortPriority::Type InSortPriority, const FName& InColumnId, const EColumnSortMode::Type InSortMode);

	void SortBranchView();

	void StartRefreshStatus();
	void TickRefreshStatus(double InDeltaTime);
	void EndRefreshStatus();

	void RequestBranchesRefresh();

	/** Source control callbacks */
	void OnGetBranchesOperationComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult);
	void OnSourceControlProviderChanged(ISourceControlProvider& OldProvider, ISourceControlProvider& NewProvider);

	SListView<FPlasticSourceControlBranchRef>* GetListView() const
	{
		return BranchesListView.Get();
	}

private:
	TSharedPtr<SSearchBox> FileSearchBox;

	FName PrimarySortedColumn;
	FName SecondarySortedColumn;
	EColumnSortMode::Type PrimarySortMode = EColumnSortMode::Ascending;
	EColumnSortMode::Type SecondarySortMode = EColumnSortMode::None;

	TArray<FName> HiddenColumnsList;

	bool bShouldRefresh = false;
	bool bSourceControlAvailable = false;

	FText RefreshStatus;
	bool bIsRefreshing = false;
	double RefreshStatusStartSecs;

	FString CurrentBranchName;

	TSharedPtr<SListView<FPlasticSourceControlBranchRef>> BranchesListView;
	TSharedPtr<TTextFilter<const FPlasticSourceControlBranch&>> SearchTextFilter;

	TMap<int32, FText> FromDateInDaysValues;
	int32 FromDateInDays = 30;

	TArray<FPlasticSourceControlBranchRef> SourceControlBranches; // Full list from source (filtered by date)
	TArray<FPlasticSourceControlBranchRef> BranchRows; // Filtered list to display based on the search text filter
};
