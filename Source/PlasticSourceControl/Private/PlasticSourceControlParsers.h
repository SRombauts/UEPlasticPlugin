// Copyright (c) 2023 Unity Technologies

#pragma once

#include "CoreMinimal.h"

#include "Runtime/Launch/Resources/Version.h"

class FPlasticSourceControlChangelistState;
class FPlasticSourceControlLock;
class FPlasticSourceControlRevision;
class FPlasticSourceControlState;
typedef TSharedRef<class FPlasticSourceControlBranch, ESPMode::ThreadSafe> FPlasticSourceControlBranchRef;

namespace PlasticSourceControlParsers
{

FPlasticSourceControlLock ParseLockInfo(const FString& InResult);

class FPlasticMergeConflictParser
{
public:
	explicit FPlasticMergeConflictParser(const FString& InResult);

	FString Filename;
	FString BaseChangeset;
	FString SourceChangeset;
};

/**
	* Helper struct for RemoveRedundantErrors()
	*/
struct FRemoveRedundantErrors
{
	explicit FRemoveRedundantErrors(const FString& InFilter)
		: Filter(InFilter)
	{
	}

	bool operator()(const FString& String) const
	{
		if (String.Contains(Filter))
		{
			return true;
		}

		return false;
	}

	/** The filter string we try to identify in the reported error */
	FString Filter;
};

bool ParseProfileInfo(TArray<FString>& InResults, const FString& InServerUrl, FString& OutUserName);

bool ParseWorkspaceInfo(TArray<FString>& InResults, FString& OutBranchName, FString& OutRepositoryName, FString& OutServerUrl);

bool GetChangesetFromWorkspaceStatus(const TArray<FString>& InResults, int32& OutChangeset);

void ParseFileStatusResult(TArray<FString>&& InFiles, const TArray<FString>& InResults, TArray<FPlasticSourceControlState>& OutStates);

void ParseDirectoryStatusResult(const FString& InDir, const TArray<FString>& InResults, TArray<FPlasticSourceControlState>& OutStates);

void ParseFileinfoResults(const TArray<FString>& InResults, TArray<FPlasticSourceControlState>& InOutStates);

bool ParseHistoryResults(const bool bInUpdateHistory, const FString& InResultFilename, TArray<FPlasticSourceControlState>& InOutStates);

bool ParseUpdateResults(const FString& InResults, TArray<FString>& OutFiles);
bool ParseUpdateResults(const TArray<FString>& InResults, TArray<FString>& OutFiles);

FText ParseCheckInResults(const TArray<FString>& InResults);

#if ENGINE_MAJOR_VERSION == 5

bool ParseChangelistsResults(const FString& Results, TArray<FPlasticSourceControlChangelistState>& OutChangelistsStates, TArray<TArray<FPlasticSourceControlState>>& OutCLFilesStates);

bool ParseShelveDiffResult(const FString InWorkspaceRoot, TArray<FString>&& InResults, FPlasticSourceControlChangelistState& InOutChangelistsState);
bool ParseShelveDiffResults(const FString InWorkspaceRoot, TArray<FString>&& InResults, TArray<FPlasticSourceControlRevision>& OutBaseRevisions);

bool ParseShelvesResults(const FString& InResults, TArray<FPlasticSourceControlChangelistState>& InOutChangelistsStates);
bool ParseShelvesResult(const FString& InResults, FString& OutComment, FDateTime& OutDate, FString& OutOwner);

#endif

bool ParseBranchesResults(const FString& InResults, TArray<FPlasticSourceControlBranchRef>& OutBranches);

bool ParseMergeResults(const FString& InResult, TArray<FString>& OutFiles);

} // namespace PlasticSourceControlParsers
