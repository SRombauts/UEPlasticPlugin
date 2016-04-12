// Copyright (c) 2016 Codice Software - Sebastien Rombauts (sebastien.rombauts@gmail.com)

#include "PlasticSourceControlPrivatePCH.h"
#include "PlasticSourceControlState.h"

#define LOCTEXT_NAMESPACE "PlasticSourceControl.State"

int32 FPlasticSourceControlState::GetHistorySize() const
{
	return History.Num();
}

TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FPlasticSourceControlState::GetHistoryItem( int32 HistoryIndex ) const
{
	check(History.IsValidIndex(HistoryIndex));
	return History[HistoryIndex];
}

TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FPlasticSourceControlState::FindHistoryRevision( int32 RevisionNumber ) const
{
	for(const auto& Revision : History)
	{
		if(Revision->GetRevisionNumber() == RevisionNumber)
		{
			return Revision;
		}
	}

	return nullptr;
}

TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FPlasticSourceControlState::FindHistoryRevision(const FString& InRevision) const
{
	for(const auto& Revision : History)
	{
		if(Revision->GetRevision() == InRevision)
		{
			return Revision;
		}
	}

	return nullptr;
}

TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FPlasticSourceControlState::GetBaseRevForMerge() const
{
	for(const auto& Revision : History)
	{
		// look for the the SHA1 id of the file, not the commit id (revision)
		if(Revision->FileHash == PendingMergeBaseFileHash)
		{
			return Revision;
		}
	}

	return nullptr;
}

FName FPlasticSourceControlState::GetIconName() const
{
	switch(WorkspaceState)
	{
	case EWorkspaceState::CheckedOut:
		return FName("Perforce.CheckedOut");
	case EWorkspaceState::Added:
	case EWorkspaceState::Moved:
	case EWorkspaceState::Copied:
	case EWorkspaceState::Replaced:
		return FName("Perforce.OpenForAdd");
	case EWorkspaceState::Deleted:
		return FName("Perforce.MarkedForDelete");
	case EWorkspaceState::Changed:
	case EWorkspaceState::Conflicted:
		return FName("Perforce.NotAtHeadRevision");
	case EWorkspaceState::Private:
		return FName("Perforce.NotInDepot");
	case EWorkspaceState::Unknown:
	case EWorkspaceState::Ignored:
	case EWorkspaceState::Controled: // (Unchanged) same as "Pristine" for Perforce (not checked out) ie no icon
	default:
		return NAME_None;
	}
}

FName FPlasticSourceControlState::GetSmallIconName() const
{
	switch(WorkspaceState)
	{
	case EWorkspaceState::CheckedOut:
		return FName("Perforce.CheckedOut_Small");
	case EWorkspaceState::Added:
	case EWorkspaceState::Moved:
	case EWorkspaceState::Copied:
	case EWorkspaceState::Replaced:
		return FName("Perforce.OpenForAdd_Small");
	case EWorkspaceState::Deleted:
		return FName("Perforce.MarkedForDelete_Small");
	case EWorkspaceState::Changed:
	case EWorkspaceState::Conflicted:
		return FName("Perforce.NotAtHeadRevision_Small");
	case EWorkspaceState::Private:
		return FName("Perforce.NotInDepot_Small");
	case EWorkspaceState::Unknown:
	case EWorkspaceState::Ignored:
	case EWorkspaceState::Controled: // (Unchanged) same as "Pristine" for Perforce (not checked out) ie no icon
	default:
		return NAME_None;
	}
}

FText FPlasticSourceControlState::GetDisplayName() const
{
	switch(WorkspaceState)
	{
	case EWorkspaceState::Unknown:
		return LOCTEXT("Unknown", "Unknown");
	case EWorkspaceState::Ignored:
		return LOCTEXT("Ignored", "Ignored");
	case EWorkspaceState::Controled:
		return LOCTEXT("Controled", "Controled");
	case EWorkspaceState::CheckedOut:
		return LOCTEXT("CheckedOut", "CheckedOut");
	case EWorkspaceState::Added:
		return LOCTEXT("Added", "Added");
	case EWorkspaceState::Moved:
		return LOCTEXT("Moved", "Moved");
	case EWorkspaceState::Copied:
		return LOCTEXT("Copied", "Copied");
	case EWorkspaceState::Replaced:
		return LOCTEXT("Replaced", "Replaced");
	case EWorkspaceState::Deleted:
		return LOCTEXT("Deleted", "Deleted");
	case EWorkspaceState::Changed:
		return LOCTEXT("Changed", "Changed");
	case EWorkspaceState::Conflicted:
		return LOCTEXT("ContentsConflict", "Contents Conflict");
	case EWorkspaceState::Private:
		return LOCTEXT("NotControlled", "Not Under Source Control");
	}

	return FText();
}

FText FPlasticSourceControlState::GetDisplayTooltip() const
{
	switch(WorkspaceState)
	{
	case EWorkspaceState::Unknown:
		return LOCTEXT("Unknown_Tooltip", "Unknown source control state");
	case EWorkspaceState::Ignored:
		return LOCTEXT("Ignored_Tooltip", "Item is being ignored.");
	case EWorkspaceState::Controled:
		return LOCTEXT("Pristine_Tooltip", "There are no modifications");
	case EWorkspaceState::CheckedOut:
		return LOCTEXT("CheckedOut_Tooltip", "The file(s) are checked out");
	case EWorkspaceState::Added:
		return LOCTEXT("Added_Tooltip", "Item is scheduled for addition");
	case EWorkspaceState::Deleted:
		return LOCTEXT("Deleted_Tooltip", "Item is scheduled for deletion");
	case EWorkspaceState::Changed:
		return LOCTEXT("Modified_Tooltip", "Item has been modified");
	case EWorkspaceState::Conflicted:
		return LOCTEXT("ContentsConflict_Tooltip", "The contents (as opposed to the properties) of the item conflict with updates received from the repository.");
	case EWorkspaceState::Private:
		return LOCTEXT("NotControlled_Tooltip", "Item is not under version control.");
	}

	return FText();
}

const FString& FPlasticSourceControlState::GetFilename() const
{
	return LocalFilename;
}

const FDateTime& FPlasticSourceControlState::GetTimeStamp() const
{
	return TimeStamp;
}

// Deleted and Missing assets cannot appear in the Content Browser
bool FPlasticSourceControlState::CanCheckIn() const
{
	// TODO: cf. CanCheckout Moved/Copied? Also Localy Moved?
	const bool bCanCheckIn = WorkspaceState == EWorkspaceState::Added
		|| WorkspaceState == EWorkspaceState::Deleted
		|| WorkspaceState == EWorkspaceState::Changed
		|| WorkspaceState == EWorkspaceState::Moved
		|| WorkspaceState == EWorkspaceState::CheckedOut;

	UE_LOG(LogSourceControl, Log, TEXT("CanCheckIn(%s)=%d"), *LocalFilename, bCanCheckIn);

	return bCanCheckIn;
}

bool FPlasticSourceControlState::CanCheckout() const
{
	// TODO: Moved/Copied? Also Localy Moved?
	const bool bCanCheckout  = WorkspaceState == EWorkspaceState::Controled	// In source control, Unmodified
							|| WorkspaceState == EWorkspaceState::Changed;	// In source control, but not checked-out

	UE_LOG(LogSourceControl, Log, TEXT("CanCheckout(%s)=%d"), *LocalFilename, bCanCheckout);

	return bCanCheckout;
}

bool FPlasticSourceControlState::IsCheckedOut() const
{
	// TODO: cf. CanCheckout Moved/Copied? Also Localy Moved?
	const bool bIsCheckedOut = WorkspaceState == EWorkspaceState::CheckedOut
							|| WorkspaceState == EWorkspaceState::Added
							|| WorkspaceState == EWorkspaceState::Moved
							|| WorkspaceState == EWorkspaceState::Copied
							|| WorkspaceState == EWorkspaceState::Replaced;

	UE_LOG(LogSourceControl, Log, TEXT("IsCheckedOut(%s)=%d"), *LocalFilename, bIsCheckedOut);

	return bIsCheckedOut;
}

bool FPlasticSourceControlState::IsCheckedOutOther(FString* Who) const
{
	if (Who != NULL)
	{
		*Who = LockedBy;
	}
	// TODO	return State == EPerforceState::CheckedOutOther;  Does Plastic uses a specific state?
	if(0 < LockedBy.Len()) UE_LOG(LogSourceControl, Log, TEXT("IsCheckedOutOther(%s)=%s"), *LocalFilename, *LockedBy);
	return (0 < LockedBy.Len());
}

bool FPlasticSourceControlState::IsCurrent() const
{
	const bool bIsCurrent = (LocalRevisionChangeset == DepotRevisionChangeset);

	UE_LOG(LogSourceControl, Log, TEXT("IsCurrent(%s)=%d"), *LocalFilename, bIsCurrent);
	
	return bIsCurrent;
}

bool FPlasticSourceControlState::IsSourceControlled() const
{
	const bool bIsSourceControlled = WorkspaceState != EWorkspaceState::Private
								  && WorkspaceState != EWorkspaceState::Ignored
								  && WorkspaceState != EWorkspaceState::Unknown;

	UE_LOG(LogSourceControl, Log, TEXT("IsSourceControlled(%s)=%d"), *LocalFilename, bIsSourceControlled);

	return bIsSourceControlled;
}

bool FPlasticSourceControlState::IsAdded() const
{
	UE_LOG(LogSourceControl, Log, TEXT("IsAdded(%s)=%d"), *LocalFilename, WorkspaceState == EWorkspaceState::Added);

	return WorkspaceState == EWorkspaceState::Added; // TODO Moved & Copie? 
}

bool FPlasticSourceControlState::IsDeleted() const
{
	UE_LOG(LogSourceControl, Log, TEXT("IsAdded(%s)=%d"), *LocalFilename, WorkspaceState == EWorkspaceState::Deleted);

	return WorkspaceState == EWorkspaceState::Deleted;
}

bool FPlasticSourceControlState::IsIgnored() const
{
	UE_LOG(LogSourceControl, Log, TEXT("IsAdded(%s)=%d"), *LocalFilename, WorkspaceState == EWorkspaceState::Ignored);

	return WorkspaceState == EWorkspaceState::Ignored;
}

bool FPlasticSourceControlState::CanEdit() const
{
	// TODO: cf. CanCheckout Moved/Copied? Also Localy Moved?
	const bool bCanEdit =  WorkspaceState == EWorkspaceState::CheckedOut
						|| WorkspaceState == EWorkspaceState::Added
						|| WorkspaceState == EWorkspaceState::Moved
						|| WorkspaceState == EWorkspaceState::Copied
						|| WorkspaceState == EWorkspaceState::Replaced
						|| WorkspaceState == EWorkspaceState::Ignored
						|| WorkspaceState == EWorkspaceState::Private;

	UE_LOG(LogSourceControl, Log, TEXT("CanEdit(%s)=%d"), *LocalFilename, bCanEdit);

	return bCanEdit;
}

bool FPlasticSourceControlState::IsUnknown() const
{
	UE_LOG(LogSourceControl, Log, TEXT("IsUnknown(%s)=%d"), *LocalFilename, WorkspaceState == EWorkspaceState::Unknown);

	return WorkspaceState == EWorkspaceState::Unknown;
}

bool FPlasticSourceControlState::IsModified() const
{
	// Warning: for a clean "check-in" (commit) checked-out files unmodified should be removed from the changeset (the index)
	//
	// Thus, before check-in UE4 Editor call RevertUnchangedFiles() in PromptForCheckin() and CheckinFiles().
	//
	// So here we must take care to enumerate all states that need to be commited, all other will be discarded:
	//  - Unknown
	//  - Controled (Unchanged)
	//  - Private
	//  - Ignored
	const bool bIsModified =   WorkspaceState == EWorkspaceState::CheckedOut
							|| WorkspaceState == EWorkspaceState::Added
							|| WorkspaceState == EWorkspaceState::Moved
							|| WorkspaceState == EWorkspaceState::Copied
							|| WorkspaceState == EWorkspaceState::Replaced
							|| WorkspaceState == EWorkspaceState::Deleted
							|| WorkspaceState == EWorkspaceState::Changed
							|| WorkspaceState == EWorkspaceState::Conflicted;

	UE_LOG(LogSourceControl, Log, TEXT("IsModified(%s)=%d"), *LocalFilename, bIsModified);

	return bIsModified;
}


bool FPlasticSourceControlState::CanAdd() const
{
	UE_LOG(LogSourceControl, Log, TEXT("CanAdd(%s)=%d"), *LocalFilename, WorkspaceState == EWorkspaceState::Private);

	return WorkspaceState == EWorkspaceState::Private;
}

bool FPlasticSourceControlState::IsConflicted() const
{
	UE_LOG(LogSourceControl, Log, TEXT("IsConflicted(%s)=%d"), *LocalFilename, WorkspaceState == EWorkspaceState::Conflicted);

	return WorkspaceState == EWorkspaceState::Conflicted;
}

#undef LOCTEXT_NAMESPACE
