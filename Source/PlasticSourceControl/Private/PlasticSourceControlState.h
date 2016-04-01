// Copyright (c) 2016 Codice Software - Sebastien Rombauts (sebastien.rombauts@gmail.com)

#pragma once

#include "ISourceControlState.h"
#include "PlasticSourceControlRevision.h"

namespace EWorkingCopyState
{
	enum Type
	{
		Unknown,
		Ignored,
		Controled, // called "Pristine" in Perforce, "Unchanged" in Git, "clean" in SVN
		CheckedOut,
		Added,
		Moved, // Renamed
		Copied,
		Deleted,
		Changed, // Modified but not CheckedOut
		Conflicted,
		NotControlled,
	};
}

class FPlasticSourceControlState : public ISourceControlState, public TSharedFromThis<FPlasticSourceControlState, ESPMode::ThreadSafe>
{
public:
	FPlasticSourceControlState( const FString& InLocalFilename )
		: LocalFilename(InLocalFilename)
		, WorkingCopyState(EWorkingCopyState::Unknown)
		, TimeStamp(0)
	{
	}

	/** ISourceControlState interface */
	virtual int32 GetHistorySize() const override;
	virtual TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> GetHistoryItem(int32 HistoryIndex) const override;
	virtual TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FindHistoryRevision(int32 RevisionNumber) const override;
	virtual TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FindHistoryRevision(const FString& InRevision) const override;
	virtual TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> GetBaseRevForMerge() const override;
	virtual FName GetIconName() const override;
	virtual FName GetSmallIconName() const override;
	virtual FText GetDisplayName() const override;
	virtual FText GetDisplayTooltip() const override;
	virtual const FString& GetFilename() const override;
	virtual const FDateTime& GetTimeStamp() const override;
	virtual bool CanCheckIn() const override;
	virtual bool CanCheckout() const override;
	virtual bool IsCheckedOut() const override;
	virtual bool IsCheckedOutOther(FString* Who = nullptr) const override;
	virtual bool IsCurrent() const override;
	virtual bool IsSourceControlled() const override;
	virtual bool IsAdded() const override;
	virtual bool IsDeleted() const override;
	virtual bool IsIgnored() const override;
	virtual bool CanEdit() const override;
	virtual bool IsUnknown() const override;
	virtual bool IsModified() const override;
	virtual bool CanAdd() const override;
	virtual bool IsConflicted() const override;

public:
	/** History of the item, if any */
	TPlasticSourceControlHistory History;

	/** Filename on disk */
	FString LocalFilename;

	/** File Id with which our local revision diverged from the remote revision */
	FString PendingMergeBaseFileHash;

	/** State of the working copy */
	EWorkingCopyState::Type WorkingCopyState;

	/** The timestamp of the last update */
	FDateTime TimeStamp;
};
