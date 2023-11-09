// Copyright (c) 2023 Unity Technologies

#pragma once

#include "CoreMinimal.h"

class FPlasticSourceControlBranch
{
public:
	FPlasticSourceControlBranch() = default;
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

	void PopulateSearchString(TArray<FString>& OutStrings) const
	{
		OutStrings.Emplace(Name);
		OutStrings.Emplace(CreatedBy);
		OutStrings.Emplace(Comment);
	}
};

typedef TSharedRef<class FPlasticSourceControlBranch, ESPMode::ThreadSafe> FPlasticSourceControlBranchRef;
typedef TSharedPtr<class FPlasticSourceControlBranch, ESPMode::ThreadSafe> FPlasticSourceControlBranchPtr;
