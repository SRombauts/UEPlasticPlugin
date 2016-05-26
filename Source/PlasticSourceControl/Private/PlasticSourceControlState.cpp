// Copyright (c) 2016 Codice Software - Sebastien Rombauts (sebastien.rombauts@gmail.com)

#include "PlasticSourceControlPrivatePCH.h"
#include "PlasticSourceControlState.h"

#define LOCTEXT_NAMESPACE "PlasticSourceControl.State"

namespace EWorkspaceState
{
static const TCHAR* ToString(EWorkspaceState::Type InWorkspaceState)
{
	const TCHAR* WorkspaceStateStr = nullptr;
	switch (InWorkspaceState)
	{
	case EWorkspaceState::Unknown: WorkspaceStateStr = TEXT("Unknown"); break;
	case EWorkspaceState::Ignored: WorkspaceStateStr = TEXT("Ignored"); break;
	case EWorkspaceState::Controlled: WorkspaceStateStr = TEXT("Controlled"); break;
	case EWorkspaceState::CheckedOut: WorkspaceStateStr = TEXT("CheckedOut"); break;
	case EWorkspaceState::Added: WorkspaceStateStr = TEXT("Added"); break;
	case EWorkspaceState::Moved: WorkspaceStateStr = TEXT("Moved"); break;
	case EWorkspaceState::Copied: WorkspaceStateStr = TEXT("Copied"); break;
	case EWorkspaceState::Replaced: WorkspaceStateStr = TEXT("Replaced"); break;
	case EWorkspaceState::Deleted: WorkspaceStateStr = TEXT("Deleted"); break;
	case EWorkspaceState::Changed: WorkspaceStateStr = TEXT("Changed"); break;
	case EWorkspaceState::Conflicted: WorkspaceStateStr = TEXT("Conflicted"); break;
	case EWorkspaceState::LockedByOther: WorkspaceStateStr = TEXT("LockedByOther"); break;
	case EWorkspaceState::Private: WorkspaceStateStr = TEXT("Private"); break;
	default: WorkspaceStateStr = TEXT("???"); break;
	}
	return WorkspaceStateStr;
}
}


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
	if (!IsCurrent())
	{
		return FName("Perforce.NotAtHeadRevision");
	}

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
	case EWorkspaceState::LockedByOther:
		return FName("Perforce.CheckedOutByOtherUser");
	case EWorkspaceState::Private:
		return FName("Perforce.NotInDepot");
	case EWorkspaceState::Unknown:
	case EWorkspaceState::Ignored:
	case EWorkspaceState::Controlled: // (Unchanged) same as "Pristine" for Perforce (not checked out) ie no icon
	default:
		return NAME_None;
	}
}

FName FPlasticSourceControlState::GetSmallIconName() const
{
	if (!IsCurrent())
	{
		return FName("Perforce.NotAtHeadRevision_Small");
	}

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
	case EWorkspaceState::LockedByOther:
		return FName("Perforce.CheckedOutByOtherUser_Small");
	case EWorkspaceState::Private:
		return FName("Perforce.NotInDepot_Small");
	case EWorkspaceState::Unknown:
	case EWorkspaceState::Ignored:
	case EWorkspaceState::Controlled: // (Unchanged) same as "Pristine" for Perforce (not checked out) ie no icon
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
	case EWorkspaceState::Controlled:
		return LOCTEXT("Controlled", "Controlled");
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
	case EWorkspaceState::LockedByOther:
		return FText::Format(LOCTEXT("CheckedOutOther", "Checked out by: {0}"), FText::FromString(LockedBy));
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
	case EWorkspaceState::Controlled:
		return LOCTEXT("Pristine_Tooltip", "There are no modifications");
	case EWorkspaceState::CheckedOut:
		return LOCTEXT("CheckedOut_Tooltip", "Item is checked out");
	case EWorkspaceState::Added:
		return LOCTEXT("Added_Tooltip", "Item is scheduled for addition");
	case EWorkspaceState::Moved:
		return LOCTEXT("Moved_Tooltip", "Item has been moved or renamed");
	case EWorkspaceState::Copied:
		return LOCTEXT("Copied_Tooltip", "Item has been copied");
	case EWorkspaceState::Replaced:
		return LOCTEXT("Replaced_Tooltip", "Item has been replaced");
	case EWorkspaceState::Deleted:
		return LOCTEXT("Deleted_Tooltip", "Item is scheduled for deletion");
	case EWorkspaceState::Changed:
		return LOCTEXT("Modified_Tooltip", "Item has been modified");
	case EWorkspaceState::Conflicted:
		return LOCTEXT("ContentsConflict_Tooltip", "The contents of the item conflict with updates received from the repository.");
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
	// TODO: cf. CanCheckout Moved/Copied? Also Locally Moved?
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
	// TODO: Moved/Copied?
	const bool bCanCheckout  = WorkspaceState == EWorkspaceState::Controlled	// In source control, Unmodified
							|| WorkspaceState == EWorkspaceState::Changed;	// In source control, but not checked-out

	UE_LOG(LogSourceControl, Log, TEXT("CanCheckout(%s)=%d"), *LocalFilename, bCanCheckout);

	return bCanCheckout;
}

bool FPlasticSourceControlState::IsCheckedOut() const
{
	// TODO: cf. CanCheckout Moved/Copied? Also Locally Moved?
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
	const bool bIsLockedByOther = WorkspaceState == EWorkspaceState::LockedByOther;

	if (bIsLockedByOther) UE_LOG(LogSourceControl, Log, TEXT("IsCheckedOutOther(%s)=%d by '%s' (%s)"), *LocalFilename, bIsLockedByOther, *LockedBy, *LockedWhere);

	return bIsLockedByOther;
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
	UE_LOG(LogSourceControl, Log, TEXT("IsDeleted(%s)=%d"), *LocalFilename, WorkspaceState == EWorkspaceState::Deleted);

	return WorkspaceState == EWorkspaceState::Deleted;
}

bool FPlasticSourceControlState::IsIgnored() const
{
	UE_LOG(LogSourceControl, Log, TEXT("IsIgnored(%s)=%d"), *LocalFilename, WorkspaceState == EWorkspaceState::Ignored);

	return WorkspaceState == EWorkspaceState::Ignored;
}

bool FPlasticSourceControlState::CanEdit() const
{
	// TODO: cf. CanCheckout Moved/Copied? Also Locally Moved?
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
	//  - Controlled (Unchanged)
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
