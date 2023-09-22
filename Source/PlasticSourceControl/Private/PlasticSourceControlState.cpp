// Copyright Unity Technologies

#include "PlasticSourceControlState.h"
#include "PlasticSourceControlProjectSettings.h"
#include "ISourceControlModule.h"
#if ENGINE_MAJOR_VERSION == 5
#include "Styling/AppStyle.h"
#if ENGINE_MINOR_VERSION >= 2
#include "RevisionControlStyle/RevisionControlStyle.h"
#endif
#endif

#define LOCTEXT_NAMESPACE "PlasticSourceControl.State"

const TCHAR* FPlasticSourceControlState::ToString() const
{
	const TCHAR* WorkspaceStateStr = nullptr;
	switch (WorkspaceState)
	{
		case EWorkspaceState::Unknown: WorkspaceStateStr = TEXT("Unknown"); break;
		case EWorkspaceState::Ignored: WorkspaceStateStr = TEXT("Ignored"); break;
		case EWorkspaceState::Controlled: WorkspaceStateStr = TEXT("Controlled"); break;
		case EWorkspaceState::CheckedOutChanged: WorkspaceStateStr = TEXT("CheckedOutChanged"); break;
		case EWorkspaceState::CheckedOutUnchanged: WorkspaceStateStr = TEXT("CheckedOutUnchanged"); break;
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

#if ENGINE_MAJOR_VERSION == 4 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 3)
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
#endif

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

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
ISourceControlState::FResolveInfo FPlasticSourceControlState::GetResolveInfo() const
{
	return PendingResolveInfo;
}
#endif

#if ENGINE_MAJOR_VERSION == 4

FName FPlasticSourceControlState::GetIconName() const
{
	if (!IsCurrent())
	{
		return FName("Perforce.NotAtHeadRevision");
	}

	if (!IsCheckedOutImplementation())
	{
		if (IsCheckedOutOther())
		{
			return FName("Perforce.CheckedOutByOtherUser");
		}

		if (IsRetainedInOtherBranch())
		{
			return FName("Perforce.CheckedOutByOtherUserOtherBranch");
		}

		if (IsModifiedInOtherBranch())
		{
			return FName("Perforce.ModifiedOtherBranch");
		}
	}

	switch (WorkspaceState)
	{
	case EWorkspaceState::CheckedOutChanged:
	case EWorkspaceState::CheckedOutUnchanged:
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
	case EWorkspaceState::Changed: // Changed but unchecked-out file is in a certain way not controlled
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
	if (!IsCheckedOutImplementation())
	{
		if (IsCheckedOutOther())
		{
			return FName("Perforce.CheckedOutByOtherUser_Small");
		}

		if (IsRetainedInOtherBranch())
		{
			return FName("Perforce.CheckedOutByOtherUserOtherBranch_Small");
		}

		if (IsModifiedInOtherBranch())
		{
			return FName("Perforce.ModifiedOtherBranch_Small");
		}
	}

	switch (WorkspaceState)
	{
	case EWorkspaceState::CheckedOutChanged:
	case EWorkspaceState::CheckedOutUnchanged:
	case EWorkspaceState::Replaced: // Merged (waiting for checkin)
		return FName("Perforce.CheckedOut_Small");
	case EWorkspaceState::Added:
	case EWorkspaceState::Copied:
		return FName("Perforce.OpenForAdd_Small");
	case EWorkspaceState::Moved:
		return FName("Perforce.Branched_Small");
	case EWorkspaceState::Deleted:
	case EWorkspaceState::LocallyDeleted:
		return FName("Perforce.MarkedForDelete_Small");
	case EWorkspaceState::Conflicted:
		return FName("Perforce.NotAtHeadRevision_Small");
	case EWorkspaceState::Private: // Not controlled
		return FName("Perforce.NotInDepot_Small");
	case EWorkspaceState::Changed: // Changed but unchecked-out file is in a certain way not controlled
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
#if ENGINE_MINOR_VERSION >= 2

	if (!IsCurrent())
	{
		return FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.NotAtHeadRevision");
	}

	if (!IsCheckedOutImplementation())
	{
		if (IsCheckedOutOther())
		{
			return FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.CheckedOutByOtherUser", NAME_None, "RevisionControl.CheckedOutByOtherUserBadge");
		}

		if (IsRetainedInOtherBranch())
		{
			return FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.CheckedOutByOtherUserOtherBranch", NAME_None, "RevisionControl.CheckedOutByOtherUserOtherBranchBadge");
		}

		if (IsModifiedInOtherBranch())
		{
			return FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.ModifiedOtherBranch", NAME_None, "RevisionControl.ModifiedBadge");
		}
	}

#else

	if (!IsCurrent())
	{
		return FSlateIcon(FAppStyle::GetAppStyleSetName(), "Perforce.NotAtHeadRevision");
	}

	if (!IsCheckedOutImplementation())
	{
		if (IsCheckedOutOther())
		{
			return FSlateIcon(FAppStyle::GetAppStyleSetName(), "Perforce.CheckedOutByOtherUser", NAME_None, "SourceControl.LockOverlay");
		}

		if (IsRetainedInOtherBranch())
		{
			return FSlateIcon(FAppStyle::GetAppStyleSetName(), "Perforce.CheckedOutByOtherUserOtherBranch", NAME_None, "SourceControl.LockOverlay");
		}

		if (IsModifiedInOtherBranch())
		{
			return FSlateIcon(FAppStyle::GetAppStyleSetName(), "Perforce.ModifiedOtherBranch");
		}
	}

#endif

#if ENGINE_MINOR_VERSION >= 2 // UE5.2+

	switch (WorkspaceState)
	{
	case EWorkspaceState::CheckedOutChanged:
	case EWorkspaceState::CheckedOutUnchanged:
	case EWorkspaceState::Replaced: // Merged (waiting for check-in)
		return FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.CheckedOut");
	case EWorkspaceState::Added:
	case EWorkspaceState::Copied:
		return FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.OpenForAdd");
	case EWorkspaceState::Moved:
		return FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.Branched");
	case EWorkspaceState::Deleted:
	case EWorkspaceState::LocallyDeleted:
		return FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.MarkedForDelete");
	case EWorkspaceState::Conflicted:
		return FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.Conflicted");
	case EWorkspaceState::Private: // Not controlled
	case EWorkspaceState::Changed: // Changed but unchecked-out is in a certain way not controlled
		return FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.NotInDepot");
	case EWorkspaceState::Ignored:
	case EWorkspaceState::Unknown:
	case EWorkspaceState::Controlled: // Unchanged (not checked out) ie no icon
	default:
		return FSlateIcon();
	}

#elif ENGINE_MINOR_VERSION >= 1 // UE5.1+

	switch (WorkspaceState)
	{
	case EWorkspaceState::CheckedOutChanged:
	case EWorkspaceState::CheckedOutUnchanged:
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
	case EWorkspaceState::CheckedOutChanged:
	case EWorkspaceState::CheckedOutUnchanged:
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

	if (!IsCheckedOutImplementation())
	{
		if (IsCheckedOutOther())
		{
			return FText::Format(LOCTEXT("CheckedOutOther", "Checked out by {0} on {1} (in {2}) since {3}"), FText::FromString(LockedBy), FText::FromString(LockedBranch), FText::FromString(LockedWhere), FText::AsDateTime(LockedDate));
		}

		if (IsRetainedInOtherBranch())
		{
			return FText::Format(LOCTEXT("RetainedLock", "Retained on {0} by {1} since {2}"), FText::FromString(LockedBranch), FText::FromString(RetainedBy), FText::AsDateTime(LockedDate));
		}

		if (IsModifiedInOtherBranch())
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
	case EWorkspaceState::CheckedOutChanged:
		return LOCTEXT("CheckedOutChanged", "Checked-out (changed)");
	case EWorkspaceState::CheckedOutUnchanged:
		return LOCTEXT("CheckedOutUnchanged", "Checked-out (unchanged)");
	case EWorkspaceState::Added:
		return LOCTEXT("Added", "Added");
	case EWorkspaceState::Moved:
		return LOCTEXT("Moved", "Moved");
	case EWorkspaceState::Copied:
		return LOCTEXT("Copied", "Copied");
	case EWorkspaceState::Replaced:
		return LOCTEXT("Replaced", "Replaced");
	case EWorkspaceState::Deleted:
		return LOCTEXT("Deleted", "Removed");
	case EWorkspaceState::LocallyDeleted:
		return LOCTEXT("LocallyDeleted", "Deleted locally");
	case EWorkspaceState::Changed:
		return LOCTEXT("Changed", "Changed");
	case EWorkspaceState::Conflicted:
		return LOCTEXT("Conflicted", "Conflicted");
	case EWorkspaceState::Private:
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
		return LOCTEXT("NotControlled", "Not Under Revision Control");
#else
		return LOCTEXT("NotControlled", "Not Under Source Control");
#endif
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

	if (!IsCheckedOutImplementation())
	{
		if (IsCheckedOutOther())
		{
			return FText::Format(LOCTEXT("CheckedOutOther_Tooltip", "Checked out by {0} on {1} (in {2}) since {3}"), FText::FromString(LockedBy), FText::FromString(LockedBranch), FText::FromString(LockedWhere), FText::AsDateTime(LockedDate));
		}

		if (IsRetainedInOtherBranch())
		{
			return FText::Format(LOCTEXT("RetainedLock_Tooltip", "Retained on {0} by {1} since {2}"), FText::FromString(LockedBranch), FText::FromString(RetainedBy), FText::AsDateTime(LockedDate));
		}

		if (IsModifiedInOtherBranch())
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
	case EWorkspaceState::CheckedOutChanged:
		return LOCTEXT("CheckedOut_Tooltip", "Checked-out (changed)");
	case EWorkspaceState::CheckedOutUnchanged:
		return LOCTEXT("CheckedOut_Tooltip", "Checked-out (unchanged)");
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
		return LOCTEXT("Replaced_Tooltip", "Replaced (merged)");
	case EWorkspaceState::Deleted:
		return LOCTEXT("Deleted_Tooltip", "Removed");
	case EWorkspaceState::LocallyDeleted:
		return LOCTEXT("LocallyDeleted_Tooltip", "Deleted locally");
	case EWorkspaceState::Changed:
		return LOCTEXT("Modified_Tooltip", "Changed locally");
	case EWorkspaceState::Conflicted:
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
		return FText::Format(LOCTEXT("Conflicted_Tooltip", "Conflict merging from source/remote CS:{0} into target/local CS:{1})"),
			FText::FromString(PendingResolveInfo.RemoteRevision), FText::AsNumber(LocalRevisionChangeset, &NoCommas));
#else
		return FText::Format(LOCTEXT("Conflicted_Tooltip", "Conflict merging from source/remote CS:{0} into target/local CS:{1})"),
			FText::AsNumber(PendingMergeSourceChangeset, &NoCommas), FText::AsNumber(LocalRevisionChangeset, &NoCommas));
#endif
	case EWorkspaceState::Private:
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
		return LOCTEXT("NotControlled_Tooltip", "Private: not under revision control");
#else
		return LOCTEXT("NotControlled_Tooltip", "Private: not under source control");
#endif
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
							|| WorkspaceState == EWorkspaceState::CheckedOutChanged)
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
								|| WorkspaceState == EWorkspaceState::Changed)		// In source control, but not checked-out
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
	const bool bIsCheckedOut = IsCheckedOutImplementation()
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

bool FPlasticSourceControlState::IsCheckedOutImplementation() const
{
	const bool bIsCheckedOut = WorkspaceState == EWorkspaceState::CheckedOutChanged
							|| WorkspaceState == EWorkspaceState::CheckedOutUnchanged
							|| WorkspaceState == EWorkspaceState::Added
							|| WorkspaceState == EWorkspaceState::Deleted
							|| WorkspaceState == EWorkspaceState::Copied
							|| WorkspaceState == EWorkspaceState::Moved
							|| WorkspaceState == EWorkspaceState::Conflicted	// In source control, waiting for merge
							|| WorkspaceState == EWorkspaceState::Replaced;		// In source control, merged, waiting for checkin to conclude the merge

	return bIsCheckedOut;
}

bool FPlasticSourceControlState::IsLocked() const
{
	return !LockedBy.IsEmpty();
}

bool FPlasticSourceControlState::IsRetainedInOtherBranch() const
{
	return !RetainedBy.IsEmpty();
}

bool FPlasticSourceControlState::IsCheckedOutOther(FString* Who) const
{
	if (Who != NULL)
	{
		*Who = LockedBy;
	}

	// An asset is locked somewhere else if it is Locked but not CheckedOut on the current workspace
	const bool bIsLockedByOther = IsLocked() && !IsCheckedOutImplementation();

	if (bIsLockedByOther)
	{
		UE_LOG(LogSourceControl, VeryVerbose, TEXT("%s IsCheckedOutOther by '%s' (%s)"), *LocalFilename, *LockedBy, *LockedWhere);
	}

	return bIsLockedByOther;
}

/** Get whether this file is checked out in a different branch */
bool FPlasticSourceControlState::IsCheckedOutInOtherBranch(const FString& CurrentBranch /* = FString() */) const
{
	// NOTE: technically this scenario isn't currently possible with Unity Version Control,
	//       but the plugin need to use an existing Engine hook, so it's using this one as a way to display "Retained" locks
	return IsRetainedInOtherBranch();
}

/** Get whether this file is modified in a different branch */
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

	if (!bIsSourceControlled && !IsUnknown()) UE_LOG(LogSourceControl, Verbose, TEXT("%s NOT SourceControlled"), *LocalFilename);

	return bIsSourceControlled;
}

bool FPlasticSourceControlState::IsAdded() const
{
	const bool bIsAdded =	WorkspaceState == EWorkspaceState::Added
						 || WorkspaceState == EWorkspaceState::Copied;

	if (bIsAdded) UE_LOG(LogSourceControl, Verbose, TEXT("%s IsAdded"), *LocalFilename);

	return bIsAdded;
}

bool FPlasticSourceControlState::IsDeleted() const
{
	const bool bIsDeleted =	WorkspaceState == EWorkspaceState::Deleted
						 || WorkspaceState == EWorkspaceState::LocallyDeleted;

	if (bIsDeleted) UE_LOG(LogSourceControl, Verbose, TEXT("%s IsDeleted"), *LocalFilename);

	return bIsDeleted;
}

bool FPlasticSourceControlState::IsIgnored() const
{
	if (WorkspaceState == EWorkspaceState::Ignored) UE_LOG(LogSourceControl, Verbose, TEXT("%s IsIgnored"), *LocalFilename);

	return WorkspaceState == EWorkspaceState::Ignored;
}

bool FPlasticSourceControlState::CanEdit() const
{
	const bool bCanEdit =  WorkspaceState == EWorkspaceState::CheckedOutChanged
						|| WorkspaceState == EWorkspaceState::CheckedOutUnchanged
						|| WorkspaceState == EWorkspaceState::Added
						|| WorkspaceState == EWorkspaceState::Moved
						|| WorkspaceState == EWorkspaceState::Copied
						|| WorkspaceState == EWorkspaceState::Replaced;

	UE_LOG(LogSourceControl, Verbose, TEXT("%s CanEdit=%d"), *LocalFilename, bCanEdit);

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
	// Warning: for a clean "checkin" (commit) checked-out files unmodified should be removed from the changeset (Perforce)
	//
	// Thus, before checkin UE4 Editor call RevertUnchangedFiles() in PromptForCheckin() and CheckinFiles().
	//
	// So here we must take care to enumerate all states that need to be committed, all other will be discarded:
	//  - Unknown
	//  - Controlled (Unchanged)
	//  - CheckedOutUnchanged
	//  - Private (Not Controlled)
	//  - Ignored
	const bool bIsModified =   WorkspaceState == EWorkspaceState::CheckedOutChanged
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
	if (!IsUnknown()) UE_LOG(LogSourceControl, Verbose, TEXT("%s CanAdd=%d"), *LocalFilename, WorkspaceState == EWorkspaceState::Private);

	return WorkspaceState == EWorkspaceState::Private;
}

bool FPlasticSourceControlState::IsConflicted() const
{
	if (WorkspaceState == EWorkspaceState::Conflicted) UE_LOG(LogSourceControl, Verbose, TEXT("%s IsConflicted"), *LocalFilename);

	return WorkspaceState == EWorkspaceState::Conflicted;
}

bool FPlasticSourceControlState::CanRevert() const
{
	return IsModified() || WorkspaceState == EWorkspaceState::CheckedOutUnchanged;
}

#undef LOCTEXT_NAMESPACE
