// Copyright Unity Technologies

#include "PlasticSourceControlState.h"
#include "PlasticSourceControlProjectSettings.h"
#include "ISourceControlModule.h"
#if ENGINE_MAJOR_VERSION == 5
#include "Styling/AppStyle.h"
#endif

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
	case EWorkspaceState::Private: WorkspaceStateStr = TEXT("Private"); break;
	default: WorkspaceStateStr = TEXT("???"); break;
	}
	return WorkspaceStateStr;
}
} // namespace EWorkspaceState


int32 FPlasticSourceControlState::GetHistorySize() const
{
	return History.Num();
}

TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FPlasticSourceControlState::GetHistoryItem(int32 HistoryIndex) const
{
	check(History.IsValidIndex(HistoryIndex));
	return History[HistoryIndex];
}

TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FPlasticSourceControlState::FindHistoryRevision(int32 RevisionNumber) const
{
	for (const auto& Revision : History)
	{
		if (Revision->GetRevisionNumber() == RevisionNumber)
		{
			return Revision;
		}
	}

	return nullptr;
}

TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FPlasticSourceControlState::FindHistoryRevision(const FString& InRevision) const
{
	for (const auto& Revision : History)
	{
		if (Revision->GetRevision() == InRevision)
		{
			return Revision;
		}
	}

	return nullptr;
}

TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FPlasticSourceControlState::GetBaseRevForMerge() const
{
	for (const auto& Revision : History)
	{
		// look for the changeset number, not the revision
		if (Revision->ChangesetNumber == PendingMergeBaseChangeset)
		{
			return Revision;
		}
	}

	return nullptr;
}

TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FPlasticSourceControlState::GetCurrentRevision() const
{
	for (const auto& Revision : History)
	{
		// look for the changeset number, not the revision
		if (Revision->ChangesetNumber == LocalRevisionChangeset)
		{
			return Revision;
		}
	}

	return nullptr;
}

#if ENGINE_MAJOR_VERSION == 4

FName FPlasticSourceControlState::GetIconName() const
{
	if (!IsCurrent())
	{
		return FName("Perforce.NotAtHeadRevision");
	}
	else if (WorkspaceState != EWorkspaceState::CheckedOut)
	{
		if (IsCheckedOutOther())
		{
			return FName("Perforce.CheckedOutByOtherUser");
		}
		else if (IsModifiedInOtherBranch())
		{
			return FName("Perforce.ModifiedOtherBranch");
		}
	}

	switch (WorkspaceState)
	{
	case EWorkspaceState::CheckedOut:
	case EWorkspaceState::Replaced: // Merged (waiting for checkin)
		return FName("Perforce.CheckedOut");
	case EWorkspaceState::Added:
	case EWorkspaceState::Copied:
		return FName("Perforce.OpenForAdd");
	case EWorkspaceState::Moved:
		return FName("Perforce.Branched");
	case EWorkspaceState::Deleted: // Deleted & Missing files does not show in Content Browser
	case EWorkspaceState::LocallyDeleted: // Deleted & Missing files does not show in Content Browser
		return FName("Perforce.MarkedForDelete");
	case EWorkspaceState::Conflicted:
		return FName("Perforce.NotAtHeadRevision");
	case EWorkspaceState::Private: // Not controlled
		return FName("Perforce.NotInDepot");
	case EWorkspaceState::Changed: // Changed but unchecked-out file is in a certain way not controlled - TODO: would need a dedicated icon
		if (GetDefault<UPlasticSourceControlProjectSettings>()->bPromptForCheckoutOnChange)
		{
			return FName("Perforce.NotInDepot");
		}
		else
		{
			return FName("Perforce.CheckedOut");
		}
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
	else if (WorkspaceState != EWorkspaceState::CheckedOut)
	{
		if (IsCheckedOutOther())
		{
			return FName("Perforce.CheckedOutByOtherUser_Small");
		}
		else if (IsModifiedInOtherBranch())
		{
			return FName("Perforce.ModifiedOtherBranch_Small");
		}
	}

	switch (WorkspaceState)
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
	case EWorkspaceState::LocallyDeleted: // TODO: would need a dedicated icon
		return FName("Perforce.MarkedForDelete_Small");
	case EWorkspaceState::Conflicted: // TODO: would need a dedicated icon
		return FName("Perforce.NotAtHeadRevision_Small");
	case EWorkspaceState::Private: // Not controlled
		return FName("Perforce.NotInDepot_Small");
	case EWorkspaceState::Changed: // Changed but unchecked-out file is in a certain way not controlled - TODO: would need a dedicated icon
		if (GetDefault<UPlasticSourceControlProjectSettings>()->bPromptForCheckoutOnChange)
		{
			return FName("Perforce.NotInDepot_Small");
		}
		else
		{
			return FName("Perforce.CheckedOut_Small");
		}
	case EWorkspaceState::Unknown:
	case EWorkspaceState::Ignored:
	case EWorkspaceState::Controlled: // (Unchanged) same as "Pristine" for Perforce (not checked out) ie no icon
	default:
		return NAME_None;
	}
}

#elif ENGINE_MAJOR_VERSION == 5

FSlateIcon FPlasticSourceControlState::GetIcon() const
{
	if (!IsCurrent())
	{
		return FSlateIcon(FAppStyle::GetAppStyleSetName(), "Perforce.NotAtHeadRevision");
	}
	else if (WorkspaceState != EWorkspaceState::CheckedOut && WorkspaceState != EWorkspaceState::Conflicted)
	{
		if (IsCheckedOutOther())
		{
			return FSlateIcon(FAppStyle::GetAppStyleSetName(), "Perforce.CheckedOutByOtherUser", NAME_None, "SourceControl.LockOverlay");
		}
		else if (IsModifiedInOtherBranch())
		{
			return FSlateIcon(FAppStyle::GetAppStyleSetName(), "Perforce.ModifiedOtherBranch");
		}
	}

#if ENGINE_MINOR_VERSION >= 1 // UE5.1+

	switch (WorkspaceState)
	{
	case EWorkspaceState::CheckedOut:
	case EWorkspaceState::Replaced: // Merged (waiting for check-in)
		return FSlateIcon(FAppStyle::GetAppStyleSetName(), "Plastic.CheckedOut");
	case EWorkspaceState::Changed: // Changed but unchecked-out file custom color icon
		return FSlateIcon(FAppStyle::GetAppStyleSetName(), "Plastic.Changed"); // custom
	case EWorkspaceState::Added:
	case EWorkspaceState::Copied:
		return FSlateIcon(FAppStyle::GetAppStyleSetName(), "Plastic.OpenForAdd");
	case EWorkspaceState::Moved:
		return FSlateIcon(FAppStyle::GetAppStyleSetName(), "Plastic.Branched");
	case EWorkspaceState::Deleted:
		return FSlateIcon(FAppStyle::GetAppStyleSetName(), "Plastic.MarkedForDelete");
	case EWorkspaceState::LocallyDeleted:
		return FSlateIcon(FAppStyle::GetAppStyleSetName(), "Plastic.LocallyDeleted"); // custom
	case EWorkspaceState::Conflicted:
		return FSlateIcon(FAppStyle::GetAppStyleSetName(), "Plastic.Conflicted"); // custom
	case EWorkspaceState::Private: // Not controlled
		return FSlateIcon(FAppStyle::GetAppStyleSetName(), "Plastic.NotInDepot");
	case EWorkspaceState::Ignored:
		return FSlateIcon(FAppStyle::GetAppStyleSetName(), "Plastic.Ignored"); // custom
	case EWorkspaceState::Unknown:
	case EWorkspaceState::Controlled: // Unchanged (not checked out) ie no icon
	default:
		return FSlateIcon();
	}

#else // UE5.0

	switch (WorkspaceState)
	{
	case EWorkspaceState::CheckedOut:
	case EWorkspaceState::Replaced: // Merged (waiting for checkin)
		return FSlateIcon(FAppStyle::GetAppStyleSetName(), "Perforce.CheckedOut");
	case EWorkspaceState::Added:
	case EWorkspaceState::Copied:
		return FSlateIcon(FAppStyle::GetAppStyleSetName(), "Perforce.OpenForAdd");
	case EWorkspaceState::Moved:
		return FSlateIcon(FAppStyle::GetAppStyleSetName(), "Perforce.Branched");
	case EWorkspaceState::Deleted:
	case EWorkspaceState::LocallyDeleted:
		return FSlateIcon(FAppStyle::GetAppStyleSetName(), "Perforce.MarkedForDelete");
	case EWorkspaceState::Conflicted:
		return FSlateIcon(FAppStyle::GetAppStyleSetName(), "Perforce.NotAtHeadRevision");
	case EWorkspaceState::Private: // Not controlled
		return FSlateIcon(FAppStyle::GetAppStyleSetName(), "Perforce.NotInDepot");
	case EWorkspaceState::Changed: // Changed but unchecked-out file is in a certain way not controlled
		if (GetDefault<UPlasticSourceControlProjectSettings>()->bPromptForCheckoutOnChange)
		{
			return FSlateIcon(FAppStyle::GetAppStyleSetName(), "Perforce.NotInDepot");
		}
		else
		{
			return FSlateIcon(FAppStyle::GetAppStyleSetName(), "Perforce.CheckedOut");
		}
	case EWorkspaceState::Unknown:
	case EWorkspaceState::Ignored:
	case EWorkspaceState::Controlled: // (Unchanged) same as "Pristine" for Perforce (not checked out) ie no icon
	default:
		return FSlateIcon();
	}

#endif
}

#endif

FText FPlasticSourceControlState::GetDisplayName() const
{
	FNumberFormattingOptions NoCommas;
	NoCommas.UseGrouping = false;
	if (!IsCurrent())
	{
		return FText::Format(LOCTEXT("NotCurrent", "Not at the head revision CS:{0} {1} (local revision is CS:{2})"),
			FText::AsNumber(DepotRevisionChangeset), FText::FromString(HeadUserName), FText::AsNumber(LocalRevisionChangeset, &NoCommas));
	}
	else if (WorkspaceState != EWorkspaceState::CheckedOut && WorkspaceState != EWorkspaceState::Conflicted)
	{
		if (IsCheckedOutOther())
		{
			return FText::Format(LOCTEXT("CheckedOutOther", "Checked out by {0} in {1}"), FText::FromString(LockedBy), FText::FromString(LockedWhere));
		}
		else if (IsModifiedInOtherBranch())
		{
			return FText::Format(LOCTEXT("ModifiedOtherBranch", "Modified in {0} as CS:{1} by {2} (local revision is CS:{3})"),
				FText::FromString(HeadBranch), FText::AsNumber(HeadChangeList, &NoCommas), FText::FromString(HeadUserName), FText::AsNumber(LocalRevisionChangeset, &NoCommas));
		}
	}

	switch (WorkspaceState)
	{
	case EWorkspaceState::Unknown:
		return LOCTEXT("Unknown", "Unknown");
	case EWorkspaceState::Ignored:
		return LOCTEXT("Ignored", "Ignored");
	case EWorkspaceState::Controlled:
		return LOCTEXT("Controlled", "Controlled");
	case EWorkspaceState::CheckedOut:
		return LOCTEXT("CheckedOut", "Checked-out");
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
		return LOCTEXT("Conflicted", "Conflicted");
	case EWorkspaceState::Private:
		return LOCTEXT("NotControlled", "Not Under Source Control");
	}

	return FText();
}

FText FPlasticSourceControlState::GetDisplayTooltip() const
{
	FNumberFormattingOptions NoCommas;
	NoCommas.UseGrouping = false;
	if (!IsCurrent())
	{
		return FText::Format(LOCTEXT("NotCurrent_Tooltip", "Not at the head revision CS:{0} {1} (local revision is CS:{2})"),
			FText::AsNumber(DepotRevisionChangeset), FText::FromString(HeadUserName), FText::AsNumber(LocalRevisionChangeset, &NoCommas));
	}
	else if (WorkspaceState != EWorkspaceState::CheckedOut && WorkspaceState != EWorkspaceState::Conflicted)
	{
		if (IsCheckedOutOther())
		{
			return FText::Format(LOCTEXT("CheckedOutOther_Tooltip", "Checked out by {0} in {1}"), FText::FromString(LockedBy), FText::FromString(LockedWhere));
		}
		else if (IsModifiedInOtherBranch())
		{
			return FText::Format(LOCTEXT("ModifiedOtherBranch_Tooltip", "Modified in {0} as CS:{1} by {2} (local revision is CS:{3})"),
				FText::FromString(HeadBranch), FText::AsNumber(HeadChangeList, &NoCommas), FText::FromString(HeadUserName), FText::AsNumber(LocalRevisionChangeset, &NoCommas));
		}
	}

	switch (WorkspaceState)
	{
	case EWorkspaceState::Unknown:
		return FText();
	case EWorkspaceState::Ignored:
		return LOCTEXT("Ignored_Tooltip", "Ignored");
	case EWorkspaceState::Controlled:
		return FText();
	case EWorkspaceState::CheckedOut:
		return LOCTEXT("CheckedOut_Tooltip", "Checked-out");
	case EWorkspaceState::Added:
		return LOCTEXT("Added_Tooltip", "Added");
	case EWorkspaceState::Moved:
	{
		FString MoveOrigin = MovedFrom;
		FPaths::MakePathRelativeTo(MoveOrigin, *LocalFilename);
		return FText::Format(LOCTEXT("Moved_Tooltip", "Moved from {0}"),
			FText::FromString(FPaths::GetBaseFilename(MoveOrigin, false)));
	}
	case EWorkspaceState::Copied:
		return LOCTEXT("Copied_Tooltip", "Copied");
	case EWorkspaceState::Replaced:
		return LOCTEXT("Replaced_Tooltip", "Replaced: merge conflict resolved");
	case EWorkspaceState::Deleted:
		return LOCTEXT("Deleted_Tooltip", "Deleted");
	case EWorkspaceState::LocallyDeleted:
		return LOCTEXT("LocallyDeleted_Tooltip", "Locally Deleted");
	case EWorkspaceState::Changed:
		return LOCTEXT("Modified_Tooltip", "Locally modified");
	case EWorkspaceState::Conflicted:
		return FText::Format(LOCTEXT("Conflicted_Tooltip", "Conflict merging from source/remote CS:{0} into target/local CS:{1})"),
			FText::AsNumber(PendingMergeSourceChangeset, &NoCommas), FText::AsNumber(LocalRevisionChangeset, &NoCommas));
	case EWorkspaceState::Private:
		return LOCTEXT("NotControlled_Tooltip", "Private: not under version control");
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

bool FPlasticSourceControlState::CanCheckIn() const
{
	// Deleted assets don't appear in the Content Browser but in Submit to Source Control Window
	const bool bCanCheckIn =  (WorkspaceState == EWorkspaceState::Added
							|| WorkspaceState == EWorkspaceState::Deleted
							|| WorkspaceState == EWorkspaceState::LocallyDeleted
							|| WorkspaceState == EWorkspaceState::Changed
							|| WorkspaceState == EWorkspaceState::Moved
							|| WorkspaceState == EWorkspaceState::Copied
							|| WorkspaceState == EWorkspaceState::Replaced
							|| WorkspaceState == EWorkspaceState::CheckedOut)
							&& !IsCheckedOutOther()	// Is not already checked-out elsewhere
							&& IsCurrent();			// Is up to date (at the revision of the repo)

	if (!IsUnknown())
	{
		UE_LOG(LogSourceControl, Verbose, TEXT("%s CanCheckIn=%d"), *LocalFilename, bCanCheckIn);
	}

	return bCanCheckIn;
}

bool FPlasticSourceControlState::CanCheckout() const
{
	if (!GetDefault<UPlasticSourceControlProjectSettings>()->bPromptForCheckoutOnChange)
	{
		return false;
	}

	const bool bCanCheckout  =    (WorkspaceState == EWorkspaceState::Controlled	// In source control, Unmodified
								|| WorkspaceState == EWorkspaceState::Changed		// In source control, but not checked-out
								|| WorkspaceState == EWorkspaceState::Replaced)		// In source control, merged, waiting for checkin to conclude the merge
								&& !IsCheckedOutOther()	// Is not already checked-out elsewhere
								&& IsCurrent();			// Is up to date (at the revision of the repo)

	if (!IsUnknown())
	{
		UE_LOG(LogSourceControl, Verbose, TEXT("%s CanCheckout=%d"), *LocalFilename, bCanCheckout);
	}

	return bCanCheckout;
}

bool FPlasticSourceControlState::IsCheckedOut() const
{
	const bool bIsCheckedOut = WorkspaceState == EWorkspaceState::CheckedOut
							|| WorkspaceState == EWorkspaceState::Moved
							|| WorkspaceState == EWorkspaceState::Conflicted	// In source control, waiting for merged
							|| WorkspaceState == EWorkspaceState::Replaced		// In source control, merged, waiting for checkin to conclude the merge
							|| WorkspaceState == EWorkspaceState::Changed;		// Note: Workaround to enable checkin (still required by UE5.0)

	if (bIsCheckedOut)
	{
		UE_LOG(LogSourceControl, Verbose, TEXT("%s IsCheckedOut"), *LocalFilename);
	}

	if (GetDefault<UPlasticSourceControlProjectSettings>()->bPromptForCheckoutOnChange)
	{
		return bIsCheckedOut;
	}
	else
	{
		// Any controlled state will be considered as checked out if the prompt is disabled
		return IsSourceControlled();
	}
}

bool FPlasticSourceControlState::IsCheckedOutOther(FString* Who) const
{
	if (Who != NULL)
	{
		*Who = LockedBy;
	}

	// If the asset is Locked but not CheckedOut locally, it means it is locked somewhere else
	const bool bIsLocked = !LockedBy.IsEmpty();
	const bool bIsCheckedOut = WorkspaceState == EWorkspaceState::CheckedOut
							|| WorkspaceState == EWorkspaceState::Added
							|| WorkspaceState == EWorkspaceState::Moved
							|| WorkspaceState == EWorkspaceState::Conflicted	// In source control, waiting for merged
							|| WorkspaceState == EWorkspaceState::Replaced;		// In source control, merged, waiting for checkin to conclude the merge
	const bool bIsLockedByOther = bIsLocked && !bIsCheckedOut;

	if (bIsLockedByOther)
	{
		UE_LOG(LogSourceControl, VeryVerbose, TEXT("%s IsCheckedOutOther by '%s' (%s)"), *LocalFilename, *LockedBy, *LockedWhere);
	}

	return bIsLockedByOther;
}


/** Get whether this file is checked out in a different branch, if no branch is specified defaults to FEngineVerion current branch */
bool FPlasticSourceControlState::IsCheckedOutInOtherBranch(const FString& CurrentBranch /* = FString() */) const
{
	// Note: to my knowledge, it's not possible to detect that with PlasticSCM without the Locks,
	// which are already detected by fileinfo LockedBy/LockedWhere and reported by IsCheckedOutOther() above
	return false;
}

/** Get whether this file is modified in a different branch, if no branch is specified defaults to FEngineVerion current branch */
bool FPlasticSourceControlState::IsModifiedInOtherBranch(const FString& CurrentBranch /* = FString() */) const
{
	return !HeadBranch.IsEmpty();
}

/** Get head modification information for other branches
 * @returns true with parameters populated if there is a branch with a newer modification (edit/delete/etc)
*/
bool FPlasticSourceControlState::GetOtherBranchHeadModification(FString& HeadBranchOut, FString& ActionOut, int32& HeadChangeListOut) const
{
	HeadBranchOut = HeadBranch;
	ActionOut = HeadAction;
	HeadChangeListOut = HeadChangeList;

	return !HeadBranch.IsEmpty();
}

bool FPlasticSourceControlState::IsCurrent() const
{
	// NOTE: Deleted assets get a "-1" HeadRevision which we do not want to override the real icon state
	const bool bIsCurrent = (LocalRevisionChangeset == DepotRevisionChangeset) || (WorkspaceState == EWorkspaceState::Deleted);

	if (bIsCurrent)
	{
		UE_LOG(LogSourceControl, VeryVerbose, TEXT("%s IsCurrent"), *LocalFilename);
	}

	return bIsCurrent;
}

bool FPlasticSourceControlState::IsSourceControlled() const
{
	// NOTE: the Editor Collections rely on the default 'Unknown' state (until the actual file status is obtained) to be considered "in source control"
	const bool bIsSourceControlled = WorkspaceState != EWorkspaceState::Private
								  && WorkspaceState != EWorkspaceState::Ignored;
								  // WorkspaceState != EWorkspaceState::Unknown 

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
	// Warning: for a clean "checkin" (commit) checked-out files unmodified should be removed from the changeset (the index)
	//
	// Thus, before checkin UE4 Editor call RevertUnchangedFiles() in PromptForCheckin() and CheckinFiles().
	//
	// So here we must take care to enumerate all states that need to be committed, all other will be discarded:
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

	UE_LOG(LogSourceControl, Verbose, TEXT("%s IsModified=%d"), *LocalFilename, bIsModified);

	return bIsModified;
}


bool FPlasticSourceControlState::CanAdd() const
{
	if (!IsUnknown()) UE_LOG(LogSourceControl, Log, TEXT("%s CanAdd=%d"), *LocalFilename, WorkspaceState == EWorkspaceState::Private);

	return WorkspaceState == EWorkspaceState::Private;
}

bool FPlasticSourceControlState::IsConflicted() const
{
	if (WorkspaceState == EWorkspaceState::Conflicted) UE_LOG(LogSourceControl, Verbose, TEXT("%s IsConflicted"), *LocalFilename);

	return WorkspaceState == EWorkspaceState::Conflicted;
}

bool FPlasticSourceControlState::CanRevert() const
{
	return IsModified();
}

#undef LOCTEXT_NAMESPACE
