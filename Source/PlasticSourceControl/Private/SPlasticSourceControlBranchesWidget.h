// Copyright (c) 2023 Unity Technologies

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STableViewBase.h"


// TODO: move the following to their own file(s)
// Inspired by class SFileTableRow : public SMultiColumnTableRow<FChangelistTreeItemPtr> in SSourceControlChangelistRows.h
class FPlasticSourceControlBranch
{
public:
	FPlasticSourceControlBranch(const FString& InName, const FString& InRepository, const FString& InCreatedBy, const FDateTime& InDate, const FString& InComment)
		: Name(InName)
		, Repository(InRepository)
		, CreatedBy(InCreatedBy)
		, Date(InDate)
		, Comment(InComment)
	{}
	FString Name;
	FString Repository;
	FString CreatedBy;
	FDateTime Date;
	FString Comment;
};


class SSearchBox;

typedef TSharedRef<class FPlasticSourceControlBranch, ESPMode::ThreadSafe> FPlasticSourceControlBranchRef;
typedef TSharedPtr<class FPlasticSourceControlBranch, ESPMode::ThreadSafe> FPlasticSourceControlBranchPtr;


// Widget displaying the list of branches in the tab window, see FPlasticSourceControlBranchesWindow
class SPlasticSourceControlBranchesWidget : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SPlasticSourceControlBranchesWidget) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedRef<SWidget> CreateContentPanel();

	TSharedRef<ITableRow> OnGenerateRow(FPlasticSourceControlBranchRef InBranch, const TSharedRef<STableViewBase>& OwnerTable);
	void OnHiddenColumnsListChanged();

	void OnSearchTextChanged(const FText& InFilterText);

	EColumnSortPriority::Type GetColumnSortPriority(const FName InColumnId) const;
	EColumnSortMode::Type GetColumnSortMode(const FName InColumnId) const;
	void OnColumnSortModeChanged(const EColumnSortPriority::Type InSortPriority, const FName& InColumnId, const EColumnSortMode::Type InSortMode);

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

	TSharedPtr<SListView<FPlasticSourceControlBranchRef>> BranchesListView;

	TArray<FPlasticSourceControlBranchRef> SourceControlBranches; // Full list from source (filtered by date)
	TArray<FPlasticSourceControlBranchRef> BranchRows; // Filtered list to display based on the search text filter
};


// TODO: move the following to its own file
// Inspired by class SFileTableRow : public SMultiColumnTableRow<FChangelistTreeItemPtr> in SSourceControlChangelistRows.h


/** Lists the unique columns used in the list view displaying branches. */
namespace PlasticSourceControlBranchesListViewColumn
{
	/** The branch Name column. */
	namespace Name // NOLINT(runtime/indentation_namespace)
	{
		FName Id();
		FText GetDisplayText();
		FText GetToolTipText();
	};

	/** The branch Repository column. */
	namespace Repository // NOLINT(runtime/indentation_namespace)
	{
		FName Id();
		FText GetDisplayText();
		FText GetToolTipText();
	};

	/** The branch CreatedBy column. */
	namespace CreatedBy // NOLINT(runtime/indentation_namespace)
	{
		FName Id();
		FText GetDisplayText();
		FText GetToolTipText();
	};

	/** The branch Date column. */
	namespace Date // NOLINT(runtime/indentation_namespace)
	{
		FName Id();
		FText GetDisplayText();
		FText GetToolTipText();
	};

	/** The branch Comment column. */
	namespace Comment // NOLINT(runtime/indentation_namespace)
	{
		FName Id();
		FText GetDisplayText();
		FText GetToolTipText();
	}
} // namespace PlasticSourceControlBranchesListViewColumn

class SBranchTableRow : public SMultiColumnTableRow<FPlasticSourceControlBranchRef>
{
public:
	SLATE_BEGIN_ARGS(SBranchTableRow)
		: _BranchToVisualize(nullptr)
		, _HighlightText()
	{
	}
		SLATE_ARGUMENT(FPlasticSourceControlBranchPtr, BranchToVisualize)
		SLATE_ATTRIBUTE(FText, HighlightText)
	SLATE_END_ARGS()

public:
	/**
	* Construct a row child widgets of the ListView.
	*
	* @param InArgs Parameters including the branch to visualize in this row.
	* @param InOwner The owning ListView.
	*/
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwner);

	// SMultiColumnTableRow overrides
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnId) override;

private:
	/** The branch that we are visualizing in this row. */
	FPlasticSourceControlBranch* BranchToVisualize;

	/** The search text to highlight if any */
	TAttribute<FText> HighlightText;
};
