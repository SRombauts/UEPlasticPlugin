// Copyright (c) 2024 Unity Technologies

#pragma once

#include "CoreMinimal.h"

#include "Notification.h"

#include "Misc/TextFilter.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

#include "ISourceControlOperation.h"
#include "ISourceControlProvider.h"

typedef TSharedRef<class FPlasticSourceControlChangeset, ESPMode::ThreadSafe> FPlasticSourceControlChangesetRef;
typedef TSharedPtr<class FPlasticSourceControlChangeset, ESPMode::ThreadSafe> FPlasticSourceControlChangesetPtr;

class SSearchBox;

// Widget displaying the list of Changesets in the tab window, see FPlasticSourceControlChangesetsWindow
class SPlasticSourceControlChangesetsWidget : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SPlasticSourceControlChangesetsWidget) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	TSharedRef<SWidget> CreateToolBar();
	TSharedRef<SWidget> CreateContentPanel();

	TSharedRef<ITableRow> OnGenerateRow(FPlasticSourceControlChangesetRef InChangeset, const TSharedRef<STableViewBase>& OwnerTable);
	void OnHiddenColumnsListChanged();

	void OnSearchTextChanged(const FText& InFilterText);
	void PopulateItemSearchStrings(const FPlasticSourceControlChangeset& InItem, TArray<FString>& OutStrings);

	TSharedRef<SWidget> BuildFromDateDropDownMenu();
	void OnFromDateChanged(int32 InFromDateInDays);

	void OnRefreshUI();

	EColumnSortPriority::Type GetColumnSortPriority(const FName InColumnId) const;
	EColumnSortMode::Type GetColumnSortMode(const FName InColumnId) const;
	void OnColumnSortModeChanged(const EColumnSortPriority::Type InSortPriority, const FName& InColumnId, const EColumnSortMode::Type InSortMode);

	void SortChangesetView();

	TSharedPtr<SWidget> OnOpenContextMenu();

	void OnDiffChangesetClicked(FPlasticSourceControlChangesetPtr InSelectedChangeset);
	void OnDiffChangesetsClicked(TArray<FPlasticSourceControlChangesetRef> InSelectedChangesets);
	void OnDiffBranchClicked(FPlasticSourceControlChangesetPtr InSelectedChangeset);
	void OnSwitchToBranchClicked(FPlasticSourceControlChangesetPtr InSelectedChangeset);
	void OnSwitchToChangesetClicked(FPlasticSourceControlChangesetPtr InSelectedChangeset);

	void StartRefreshStatus();
	void TickRefreshStatus(double InDeltaTime);
	void EndRefreshStatus();

	void RequestChangesetsRefresh();

	/** Source control callbacks */
	void OnGetChangesetsOperationComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult);
	void OnSwitchToBranchOperationComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult);
	void OnSwitchToChangesetOperationComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult);
	void OnSourceControlProviderChanged(ISourceControlProvider& OldProvider, ISourceControlProvider& NewProvider);

	/** Delegate handler for when source control state changes */
	void HandleSourceControlStateChanged();

	SListView<FPlasticSourceControlChangesetRef>* GetListView() const
	{
		return ChangesetsListView.Get();
	}

	/** Double click to diff the selected changeset */
	void OnItemDoubleClicked(FPlasticSourceControlChangesetRef InBranch);

	/** Interpret F5 and Enter keys */
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

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

	int32 CurrentChangesetId;

	/** Ongoing notification for a long-running asynchronous source control operation, if any */
	FNotification Notification;

	TSharedPtr<SListView<FPlasticSourceControlChangesetRef>> ChangesetsListView;
	TSharedPtr<TTextFilter<const FPlasticSourceControlChangeset&>> SearchTextFilter;

	TMap<int32, FText> FromDateInDaysValues;
	int32 FromDateInDays = 30;

	TArray<FPlasticSourceControlChangesetRef> SourceControlChangesets; // Full list from source (filtered by date)
	TArray<FPlasticSourceControlChangesetRef> ChangesetRows; // Filtered list to display based on the search text filter

	/** Delegate handle for the HandleSourceControlStateChanged function callback */
	FDelegateHandle SourceControlStateChangedDelegateHandle;
};
