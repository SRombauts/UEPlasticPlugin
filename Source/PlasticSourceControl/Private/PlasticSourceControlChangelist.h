// Copyright Unity Technologies

#pragma once

#include "CoreMinimal.h"
#include "Runtime/Launch/Resources/Version.h"

#if ENGINE_MAJOR_VERSION == 5

#include "ISourceControlChangelist.h"

/**
 * Unique Identifier of a changelist under source control: a "name" in Plastic SCM
 */
class FPlasticSourceControlChangelist : public ISourceControlChangelist
{
public:
	FPlasticSourceControlChangelist() = default;

	explicit FPlasticSourceControlChangelist(FString&& InChangelistName, const bool bInInitialized = false)
		: ChangelistName(MoveTemp(InChangelistName))
		, bInitialized(bInInitialized)
	{
	}

	FPlasticSourceControlChangelist(const FPlasticSourceControlChangelist& InOther) = default;

#if ENGINE_MINOR_VERSION >= 1
	virtual bool CanDelete() const override
	{
		return !IsDefault();
	}
#endif

	bool operator==(const FPlasticSourceControlChangelist& InOther) const
	{
		return ChangelistName == InOther.ChangelistName;
	}

	bool operator!=(const FPlasticSourceControlChangelist& InOther) const
	{
		return ChangelistName != InOther.ChangelistName;
	}

	bool IsDefault() const
	{
		return ChangelistName == DefaultChangelist.ChangelistName;
	}

	void SetInitialized()
	{
		bInitialized = true;
	}

	bool IsInitialized() const
	{
		return bInitialized;
	}

	void Reset()
	{
		ChangelistName.Reset();
		bInitialized = false;
	}

	friend FORCEINLINE uint32 GetTypeHash(const FPlasticSourceControlChangelist& InPlasticChangelist)
	{
		return GetTypeHash(InPlasticChangelist.ChangelistName);
	}

	FString GetName() const
	{
		return ChangelistName;
	}

public:
	static const FPlasticSourceControlChangelist DefaultChangelist;

private:
	FString ChangelistName;
	bool bInitialized = false;
};

typedef TSharedRef<class FPlasticSourceControlChangelist, ESPMode::ThreadSafe> FPlasticSourceControlChangelistRef;

#endif
