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
	switch(WorkingCopyState)
	{
	case EWorkingCopyState::CheckedOut:
		return FName("Perforce.CheckedOut");
	case EWorkingCopyState::Added:
	case EWorkingCopyState::Moved:
	case EWorkingCopyState::Copied:
	case EWorkingCopyState::Replaced:
		return FName("Perforce.OpenForAdd");
	case EWorkingCopyState::Deleted:
		return FName("Perforce.MarkedForDelete");
	case EWorkingCopyState::Changed:
	case EWorkingCopyState::Conflicted:
		return FName("Perforce.NotAtHeadRevision");
	case EWorkingCopyState::NotControlled:
		return FName("Perforce.NotInDepot");
	case EWorkingCopyState::Unknown:
	case EWorkingCopyState::Ignored:
	case EWorkingCopyState::Controled: // (Unchanged) same as "Pristine" for Perforce (not checked out) ie no icon
	default:
		return NAME_None;
	}
}

FName FPlasticSourceControlState::GetSmallIconName() const
{
	switch(WorkingCopyState)
	{
	case EWorkingCopyState::CheckedOut:
		return FName("Perforce.CheckedOut_Small");
	case EWorkingCopyState::Added:
	case EWorkingCopyState::Moved:
	case EWorkingCopyState::Copied:
	case EWorkingCopyState::Replaced:
		return FName("Perforce.OpenForAdd_Small");
	case EWorkingCopyState::Deleted:
		return FName("Perforce.MarkedForDelete_Small");
	case EWorkingCopyState::Changed:
	case EWorkingCopyState::Conflicted:
		return FName("Perforce.NotAtHeadRevision_Small");
	case EWorkingCopyState::NotControlled:
		return FName("Perforce.NotInDepot_Small");
	case EWorkingCopyState::Unknown:
	case EWorkingCopyState::Ignored:
	case EWorkingCopyState::Controled: // (Unchanged) same as "Pristine" for Perforce (not checked out) ie no icon
	default:
		return NAME_None;
	}
}

FText FPlasticSourceControlState::GetDisplayName() const
{
	switch(WorkingCopyState)
	{
	case EWorkingCopyState::Unknown:
		return LOCTEXT("Unknown", "Unknown");
	case EWorkingCopyState::Ignored:
		return LOCTEXT("Ignored", "Ignored");
	case EWorkingCopyState::Controled:
		return LOCTEXT("Controled", "Controled");
	case EWorkingCopyState::CheckedOut:
		return LOCTEXT("CheckedOut", "CheckedOut");
	case EWorkingCopyState::Added:
		return LOCTEXT("Added", "Added");
	case EWorkingCopyState::Moved:
		return LOCTEXT("Moved", "Moved");
	case EWorkingCopyState::Copied:
		return LOCTEXT("Copied", "Copied");
	case EWorkingCopyState::Replaced:
		return LOCTEXT("Replaced", "Replaced");
	case EWorkingCopyState::Deleted:
		return LOCTEXT("Deleted", "Deleted");
	case EWorkingCopyState::Changed:
		return LOCTEXT("Changed", "Changed");
	case EWorkingCopyState::Conflicted:
		return LOCTEXT("ContentsConflict", "Contents Conflict");
	case EWorkingCopyState::NotControlled:
		return LOCTEXT("NotControlled", "Not Under Source Control");
	}

	return FText();
}

FText FPlasticSourceControlState::GetDisplayTooltip() const
{
	switch(WorkingCopyState)
	{
	case EWorkingCopyState::Unknown:
		return LOCTEXT("Unknown_Tooltip", "Unknown source control state");
	case EWorkingCopyState::Ignored:
		return LOCTEXT("Ignored_Tooltip", "Item is being ignored.");
	case EWorkingCopyState::Controled:
		return LOCTEXT("Pristine_Tooltip", "There are no modifications");
	case EWorkingCopyState::CheckedOut:
		return LOCTEXT("CheckedOut_Tooltip", "The file(s) are checked out");
	case EWorkingCopyState::Added:
		return LOCTEXT("Added_Tooltip", "Item is scheduled for addition");
	case EWorkingCopyState::Deleted:
		return LOCTEXT("Deleted_Tooltip", "Item is scheduled for deletion");
	case EWorkingCopyState::Changed:
		return LOCTEXT("Modified_Tooltip", "Item has been modified");
	case EWorkingCopyState::Conflicted:
		return LOCTEXT("ContentsConflict_Tooltip", "The contents (as opposed to the properties) of the item conflict with updates received from the repository.");
	case EWorkingCopyState::NotControlled:
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
	const bool bCanCheckIn = WorkingCopyState == EWorkingCopyState::Added
		|| WorkingCopyState == EWorkingCopyState::Deleted
		|| WorkingCopyState == EWorkingCopyState::Changed
		|| WorkingCopyState == EWorkingCopyState::Moved
		|| WorkingCopyState == EWorkingCopyState::CheckedOut;

	UE_LOG(LogSourceControl, Log, TEXT("CanCheckIn(%s)=%d"), *LocalFilename, bCanCheckIn);

	return bCanCheckIn;
}

bool FPlasticSourceControlState::CanCheckout() const
{
	// TODO: Moved/Copied? Also Localy Moved?
	const bool bCanCheckout  = WorkingCopyState == EWorkingCopyState::Controled	// In source control, Unmodified
							|| WorkingCopyState == EWorkingCopyState::Changed;	// In source control, but not checked-out

	UE_LOG(LogSourceControl, Log, TEXT("CanCheckout(%s)=%d"), *LocalFilename, bCanCheckout);

	return bCanCheckout;
}

bool FPlasticSourceControlState::IsCheckedOut() const
{
	// TODO: cf. CanCheckout Moved/Copied? Also Localy Moved?
	const bool bIsCheckedOut = WorkingCopyState == EWorkingCopyState::CheckedOut
							|| WorkingCopyState == EWorkingCopyState::Added
							|| WorkingCopyState == EWorkingCopyState::Moved
							|| WorkingCopyState == EWorkingCopyState::Copied
							|| WorkingCopyState == EWorkingCopyState::Replaced;

	UE_LOG(LogSourceControl, Log, TEXT("IsCheckedOut(%s)=%d"), *LocalFilename, bIsCheckedOut);

	return bIsCheckedOut;
}

bool FPlasticSourceControlState::IsCheckedOutOther(FString* Who) const
{
	return false; // TODO
}

bool FPlasticSourceControlState::IsCurrent() const
{
	UE_LOG(LogSourceControl, Log, TEXT("IsCurrent(%s)=1"), *LocalFilename);
	return true; // TODO check the state of the HEAD versus the state of tracked branch on remote
}

bool FPlasticSourceControlState::IsSourceControlled() const
{
	const bool bIsSourceControlled = WorkingCopyState != EWorkingCopyState::NotControlled
								  && WorkingCopyState != EWorkingCopyState::Ignored
								  && WorkingCopyState != EWorkingCopyState::Unknown;

	UE_LOG(LogSourceControl, Log, TEXT("IsSourceControlled(%s)=%d"), *LocalFilename, bIsSourceControlled);

	return bIsSourceControlled;
}

bool FPlasticSourceControlState::IsAdded() const
{
	UE_LOG(LogSourceControl, Log, TEXT("IsAdded(%s)=%d"), *LocalFilename, WorkingCopyState == EWorkingCopyState::Added);

	return WorkingCopyState == EWorkingCopyState::Added; // TODO Moved & Copie? 
}

bool FPlasticSourceControlState::IsDeleted() const
{
	UE_LOG(LogSourceControl, Log, TEXT("IsAdded(%s)=%d"), *LocalFilename, WorkingCopyState == EWorkingCopyState::Deleted);

	return WorkingCopyState == EWorkingCopyState::Deleted;
}

bool FPlasticSourceControlState::IsIgnored() const
{
	UE_LOG(LogSourceControl, Log, TEXT("IsAdded(%s)=%d"), *LocalFilename, WorkingCopyState == EWorkingCopyState::Ignored);

	return WorkingCopyState == EWorkingCopyState::Ignored;
}

bool FPlasticSourceControlState::CanEdit() const
{
	// TODO: cf. CanCheckout Moved/Copied? Also Localy Moved?
	const bool bCanEdit =  WorkingCopyState == EWorkingCopyState::CheckedOut
						|| WorkingCopyState == EWorkingCopyState::Added
						|| WorkingCopyState == EWorkingCopyState::Moved
						|| WorkingCopyState == EWorkingCopyState::Copied
						|| WorkingCopyState == EWorkingCopyState::Replaced
						|| WorkingCopyState == EWorkingCopyState::Ignored
						|| WorkingCopyState == EWorkingCopyState::NotControlled;

	UE_LOG(LogSourceControl, Log, TEXT("CanEdit(%s)=%d"), *LocalFilename, bCanEdit);

	return bCanEdit;
}

bool FPlasticSourceControlState::IsUnknown() const
{
	UE_LOG(LogSourceControl, Log, TEXT("IsUnknown(%s)=%d"), *LocalFilename, WorkingCopyState == EWorkingCopyState::Unknown);

	return WorkingCopyState == EWorkingCopyState::Unknown;
}

bool FPlasticSourceControlState::IsModified() const
{
	// Warning: for a clean "check-in" (commit) checked-out files unmodified should be removed from the changeset (the index)
	//
	// Thus, before check-in UE4 Editor call RevertUnchangedFiles() in PromptForCheckin() and CheckinFiles().
	//
	// So here we must take care to enumerate all states that need to be commited,
	// all other will be discarded :
	//  - Unknown
	//  - Controled (Unchanged)
	//  - NotControlled
	//  - Ignored
	// TODO: is there a way to 
	const bool bIsModified =   WorkingCopyState == EWorkingCopyState::CheckedOut
							|| WorkingCopyState == EWorkingCopyState::Added
							|| WorkingCopyState == EWorkingCopyState::Moved
							|| WorkingCopyState == EWorkingCopyState::Copied
							|| WorkingCopyState == EWorkingCopyState::Replaced
							|| WorkingCopyState == EWorkingCopyState::Deleted
							|| WorkingCopyState == EWorkingCopyState::Changed
							|| WorkingCopyState == EWorkingCopyState::Conflicted;

	UE_LOG(LogSourceControl, Log, TEXT("IsModified(%s)=%d"), *LocalFilename, bIsModified);

	return bIsModified;
}


bool FPlasticSourceControlState::CanAdd() const
{
	UE_LOG(LogSourceControl, Log, TEXT("CanAdd(%s)=%d"), *LocalFilename, WorkingCopyState == EWorkingCopyState::NotControlled);

	return WorkingCopyState == EWorkingCopyState::NotControlled;
}

bool FPlasticSourceControlState::IsConflicted() const
{
	UE_LOG(LogSourceControl, Log, TEXT("IsConflicted(%s)=%d"), *LocalFilename, WorkingCopyState == EWorkingCopyState::Conflicted);

	return WorkingCopyState == EWorkingCopyState::Conflicted;
}

#undef LOCTEXT_NAMESPACE
