// Copyright (c) 2016-2022 Codice Software

#if ENGINE_MAJOR_VERSION == 5

#include "PlasticSourceControlChangelistState.h"

#define LOCTEXT_NAMESPACE "PlasticSourceControl.ChangelistState"

FName FPlasticSourceControlChangelistState::GetIconName() const
{
	return FName("SourceControl.Changelist");
}

FName FPlasticSourceControlChangelistState::GetSmallIconName() const
{
	return FName("SourceControl.Changelist");
}

FText FPlasticSourceControlChangelistState::GetDisplayText() const
{
	return FText::FromString(Changelist.GetName());
}

FText FPlasticSourceControlChangelistState::GetDescriptionText() const
{
	return FText::FromString(Description);
}

FText FPlasticSourceControlChangelistState::GetDisplayTooltip() const
{
	return LOCTEXT("Tooltip", "Tooltip");
}

const FDateTime& FPlasticSourceControlChangelistState::GetTimeStamp() const
{
	return TimeStamp;
}

const TArray<FSourceControlStateRef>& FPlasticSourceControlChangelistState::GetFilesStates() const
{
	return Files;
}

const TArray<FSourceControlStateRef>& FPlasticSourceControlChangelistState::GetShelvedFilesStates() const
{
	return ShelvedFiles;
}

FSourceControlChangelistRef FPlasticSourceControlChangelistState::GetChangelist() const
{
	FPlasticSourceControlChangelistRef ChangelistCopy = MakeShareable( new FPlasticSourceControlChangelist(Changelist));
	return StaticCastSharedRef<ISourceControlChangelist>(ChangelistCopy);
}

#undef LOCTEXT_NAMESPACE

#endif
