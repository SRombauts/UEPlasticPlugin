// Copyright (c) 2016-2017 Codice Software - Sebastien Rombauts (sebastien.rombauts@gmail.com)

#include "PlasticSourceControlPrivatePCH.h"
#include "PlasticSourceControlState.h"

#define LOCTEXT_NAMESPACE "PlasticSourceControl.State"

namespace EWorkspaceState
{
const TCHAR* ToString(EWorkspaceState::Type InWorkspaceState)
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
	case EWorkspaceState::LocallyDeleted: WorkspaceStateStr = TEXT("LocallyDeleted"); break;
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
		if(Revision->ChangesetNumber == PendingMergeBaseChangeset)
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
	case EWorkspaceState::Replaced: // Merged (waiting for checkin) TODO: would need a dedicated icon
		return FName("Perforce.CheckedOut");
	case EWorkspaceState::Added:
	case EWorkspaceState::Copied:
		return FName("Perforce.OpenForAdd");
	case EWorkspaceState::Moved:
		return FName("Perforce.Branched");
	case EWorkspaceState::Deleted: // Deleted & Missing files does not show in Content Browser
		return FName("Perforce.MarkedForDelete");
	case EWorkspaceState::LocallyDeleted: // Deleted & Missing files does not show in Content Browser
	case EWorkspaceState::Conflicted:
		return FName("Perforce.NotAtHeadRevision");
	case EWorkspaceState::LockedByOther:
		return FName("Perforce.CheckedOutByOtherUser");
	case EWorkspaceState::Private: // Not controlled
	case EWorkspaceState::Changed: // Changed but unchecked-out file is in a certain way not controlled - TODO: would need a dedicated icon
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
	case EWorkspaceState::Replaced: // Merged (waiting for checkin)
		return FName("Perforce.CheckedOut_Small");
	case EWorkspaceState::Added:
	case EWorkspaceState::Copied:
		return FName("Perforce.OpenForAdd_Small");
	case EWorkspaceState::Moved:
		return FName("Perforce.Branched_Small");
	case EWorkspaceState::Deleted:
		return FName("Perforce.MarkedForDelete_Small");
	case EWorkspaceState::LocallyDeleted: // TODO: would need a dedicated icon
	case EWorkspaceState::Conflicted: // TODO: would need a dedicated icon
		return FName("Perforce.NotAtHeadRevision_Small");
	case EWorkspaceState::LockedByOther:
		return FName("Perforce.CheckedOutByOtherUser_Small");
	case EWorkspaceState::Private: // Not controlled
	case EWorkspaceState::Changed: // Changed but unchecked-out file is in a certain way not controlled
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
	case EWorkspaceState::LocallyDeleted:
		return LOCTEXT("LocallyDeleted", "Missing");
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
		return LOCTEXT("Added_Tooltip", "Item has been added");
	case EWorkspaceState::Moved:
		return LOCTEXT("Moved_Tooltip", "Item has been moved or renamed");
	case EWorkspaceState::Copied:
		return LOCTEXT("Copied_Tooltip", "Item has been copied");
	case EWorkspaceState::Replaced:
		return LOCTEXT("Replaced_Tooltip", "Item has been replaced / merged");
	case EWorkspaceState::Deleted:
		return LOCTEXT("Deleted_Tooltip", "Item is scheduled for deletion");
	case EWorkspaceState::LocallyDeleted:
		return LOCTEXT("LocallyDeleted_Tooltip", "Item is missing");
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
	// UE_LOG(LogSourceControl, Log, TEXT("GetFilename(%s)"), *LocalFilename);

	return LocalFilename;
}

const FDateTime& FPlasticSourceControlState::GetTimeStamp() const
{
	// UE_LOG(LogSourceControl, Log, TEXT("GetTimeStamp(%s)=%s"), *LocalFilename, *TimeStamp.ToString());

	return TimeStamp;
}

// Deleted and Missing assets cannot appear in the Content Browser but does appear in Submit to Source Control Window
bool FPlasticSourceControlState::CanCheckIn() const
{
	const bool bCanCheckIn = WorkspaceState == EWorkspaceState::Added
		|| WorkspaceState == EWorkspaceState::Deleted
		|| WorkspaceState == EWorkspaceState::LocallyDeleted
		|| WorkspaceState == EWorkspaceState::Changed // NOTE: Comment to enable checkout on prompt of a Changed file (see bellow)
		|| WorkspaceState == EWorkspaceState::Moved
		|| WorkspaceState == EWorkspaceState::Copied
		|| WorkspaceState == EWorkspaceState::Replaced
		|| WorkspaceState == EWorkspaceState::CheckedOut;

	UE_LOG(LogSourceControl, Log, TEXT("%s CanCheckIn=%d"), *LocalFilename, bCanCheckIn);

	return bCanCheckIn;
}

bool FPlasticSourceControlState::CanCheckout() const
{
	const bool bCanCheckout  = (   WorkspaceState == EWorkspaceState::Controlled	// In source control, Unmodified
								|| WorkspaceState == EWorkspaceState::Changed		// In source control, but not checked-out
								|| WorkspaceState == EWorkspaceState::Replaced)		// In source control, merged, waiting for checkin to conclude the merge 
								&& IsCurrent(); // Is up to date (at the revision of the repo)

	UE_LOG(LogSourceControl, Log, TEXT("%s CanCheckout=%d"), *LocalFilename, bCanCheckout);

	return bCanCheckout;
}

bool FPlasticSourceControlState::IsCheckedOut() const
{
	const bool bIsCheckedOut = WorkspaceState == EWorkspaceState::CheckedOut
							|| WorkspaceState == EWorkspaceState::Moved
							|| WorkspaceState == EWorkspaceState::Conflicted
							|| WorkspaceState == EWorkspaceState::Replaced // Workaround to enable checkin
							|| WorkspaceState == EWorkspaceState::Changed; // Workaround to enable checkin  NOTE: Comment to enable checkout on prompt of a Changed file (see above)

	if (bIsCheckedOut) UE_LOG(LogSourceControl, Log, TEXT("%s IsCheckedOut"), *LocalFilename);

	return bIsCheckedOut;
}

bool FPlasticSourceControlState::IsCheckedOutOther(FString* Who) const
{
	if (Who != NULL)
	{
		*Who = LockedBy;
	}
	const bool bIsLockedByOther = WorkspaceState == EWorkspaceState::LockedByOther;

	// @todo: temporary debug log
	// if (bIsLockedByOther) UE_LOG(LogSourceControl, Log, TEXT("%s IsCheckedOutOther by '%s' (%s)"), *LocalFilename, *LockedBy, *LockedWhere);

	return bIsLockedByOther;
}

bool FPlasticSourceControlState::IsCurrent() const
{
	// NOTE: Deleted assets get a "-1" HeadRevision which we do not want to override the real icon state
	const bool bIsCurrent = (LocalRevisionChangeset == DepotRevisionChangeset) || (WorkspaceState == EWorkspaceState::Deleted);

	// @todo: temporary debug log
	// if (bIsCurrent) UE_LOG(LogSourceControl, Log, TEXT("%s IsCurrent"), *LocalFilename, );
	
	return bIsCurrent;
}

bool FPlasticSourceControlState::IsSourceControlled() const
{
	const bool bIsSourceControlled = WorkspaceState != EWorkspaceState::Private
								  && WorkspaceState != EWorkspaceState::Ignored
								  && WorkspaceState != EWorkspaceState::Unknown;

	if (!bIsSourceControlled && !IsUnknown()) UE_LOG(LogSourceControl, Log, TEXT("%s NOT SourceControlled"), *LocalFilename);

	return bIsSourceControlled;
}

bool FPlasticSourceControlState::IsAdded() const
{
	const bool bIsAdded =	WorkspaceState == EWorkspaceState::Added
						 || WorkspaceState == EWorkspaceState::Copied;

	if (bIsAdded) UE_LOG(LogSourceControl, Log, TEXT("%s IsAdded"), *LocalFilename);

	return bIsAdded;
}

bool FPlasticSourceControlState::IsDeleted() const
{
	const bool bIsDeleted =	WorkspaceState == EWorkspaceState::Deleted
						 || WorkspaceState == EWorkspaceState::LocallyDeleted;

	if (bIsDeleted) UE_LOG(LogSourceControl, Log, TEXT("%s IsDeleted"), *LocalFilename);

	return bIsDeleted;
}

bool FPlasticSourceControlState::IsIgnored() const
{
	if (WorkspaceState == EWorkspaceState::Ignored) UE_LOG(LogSourceControl, Log, TEXT("%s IsIgnored"), *LocalFilename);

	return WorkspaceState == EWorkspaceState::Ignored;
}

bool FPlasticSourceControlState::CanEdit() const
{
	const bool bCanEdit =  WorkspaceState == EWorkspaceState::CheckedOut
						|| WorkspaceState == EWorkspaceState::Added
						|| WorkspaceState == EWorkspaceState::Moved
						|| WorkspaceState == EWorkspaceState::Copied
						|| WorkspaceState == EWorkspaceState::Replaced;

	UE_LOG(LogSourceControl, Log, TEXT("%s CanEdit=%d"), *LocalFilename, bCanEdit);

	return bCanEdit;
}

bool FPlasticSourceControlState::CanDelete() const
{
	return !IsCheckedOutOther() && IsSourceControlled() && IsCurrent();
}

bool FPlasticSourceControlState::IsUnknown() const
{
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
	//  - Private (Not Controlled)
	//  - Ignored
	const bool bIsModified =   WorkspaceState == EWorkspaceState::CheckedOut
							|| WorkspaceState == EWorkspaceState::Added
							|| WorkspaceState == EWorkspaceState::Moved
							|| WorkspaceState == EWorkspaceState::Copied
							|| WorkspaceState == EWorkspaceState::Replaced
							|| WorkspaceState == EWorkspaceState::Deleted
							|| WorkspaceState == EWorkspaceState::LocallyDeleted
							|| WorkspaceState == EWorkspaceState::Changed
							|| WorkspaceState == EWorkspaceState::Conflicted;

	UE_LOG(LogSourceControl, Log, TEXT("%s IsModified=%d"), *LocalFilename, bIsModified);

	return bIsModified;
}


bool FPlasticSourceControlState::CanAdd() const
{
	if (!IsUnknown()) UE_LOG(LogSourceControl, Log, TEXT("%s CanAdd=%d"), *LocalFilename, WorkspaceState == EWorkspaceState::Private);

	return WorkspaceState == EWorkspaceState::Private;
}

bool FPlasticSourceControlState::IsConflicted() const
{
	if (WorkspaceState == EWorkspaceState::Conflicted) UE_LOG(LogSourceControl, Log, TEXT("%s IsConflicted"), *LocalFilename);

	return WorkspaceState == EWorkspaceState::Conflicted;
}

bool FPlasticSourceControlState::CanRevert() const
{
	return IsModified();
}

#undef LOCTEXT_NAMESPACE
