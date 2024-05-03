// Copyright (c) 2024 Unity Technologies

#pragma once

#include "CoreMinimal.h"

#include "PlasticSourceControlState.h"

class FPlasticSourceControlChangeset
{
public:
	int32 ChangesetId;
	FString CreatedBy;
	FDateTime Date;
	FString Comment;
	FString Branch;
	// Note: array of file States, each with one Revision for Diffing (like for Files and ShelvedFiles in FPlasticSourceControlChangelist)
	TArray<FPlasticSourceControlStateRef> Files;

	void PopulateSearchString(TArray<FString>& OutStrings) const
	{
		OutStrings.Emplace(CreatedBy);
		OutStrings.Emplace(Comment);
		OutStrings.Emplace(Branch);
	}
};

typedef TSharedRef<class FPlasticSourceControlChangeset, ESPMode::ThreadSafe> FPlasticSourceControlChangesetRef;
typedef TSharedPtr<class FPlasticSourceControlChangeset, ESPMode::ThreadSafe> FPlasticSourceControlChangesetPtr;
