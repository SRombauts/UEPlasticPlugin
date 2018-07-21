// Copyright (c) 2016-2018 Codice Software - Sebastien Rombauts (sebastien.rombauts@gmail.com)

#pragma once

#include "CoreMinimal.h"
#include "ISourceControlProvider.h"
#include "IPlasticSourceControlWorker.h"
#include "PlasticSourceControlState.h"
#include "PlasticSourceControlMenu.h"

DECLARE_DELEGATE_RetVal(FPlasticSourceControlWorkerRef, FGetPlasticSourceControlWorker)

class FPlasticSourceControlProvider : public ISourceControlProvider
{
public:
	/** Constructor */
	FPlasticSourceControlProvider()
		: bPlasticAvailable(false)
		, bWorkspaceFound(false)
		, bServerAvailable(false)
		, ChangesetNumber(0)
	{
	}

	/* ISourceControlProvider implementation */
	virtual void Init(bool bForceConnection = true) override;
	virtual void Close() override;
	virtual FText GetStatusText() const override;
	virtual bool IsEnabled() const override;
	virtual bool IsAvailable() const override;
	virtual const FName& GetName(void) const override;
	virtual bool QueryStateBranchConfig(const FString& ConfigSrc, const FString& ConfigDest) /* override UE4.20 */ { return false; }
	virtual void RegisterStateBranches(const TArray<FString>& BranchNames, const FString& ContentRoot) /* override UE4.20 */ {}
	virtual int32 GetStateBranchIndex(const FString& BranchName) const /* override UE4.20 */ { return INDEX_NONE; }
	virtual ECommandResult::Type GetState( const TArray<FString>& InFiles, TArray< TSharedRef<ISourceControlState, ESPMode::ThreadSafe> >& OutState, EStateCacheUsage::Type InStateCacheUsage ) override;
	virtual TArray<FSourceControlStateRef> GetCachedStateByPredicate(TFunctionRef<bool(const FSourceControlStateRef&)> Predicate) const override;
	virtual FDelegateHandle RegisterSourceControlStateChanged_Handle( const FSourceControlStateChanged::FDelegate& SourceControlStateChanged ) override;
	virtual void UnregisterSourceControlStateChanged_Handle( FDelegateHandle Handle ) override;
	virtual ECommandResult::Type Execute( const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation, const TArray<FString>& InFiles, EConcurrency::Type InConcurrency = EConcurrency::Synchronous, const FSourceControlOperationComplete& InOperationCompleteDelegate = FSourceControlOperationComplete() ) override;
	virtual bool CanCancelOperation( const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation ) const override;
	virtual void CancelOperation( const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation ) override;
	virtual bool UsesLocalReadOnlyState() const override;
	virtual bool UsesChangelists() const override;
	virtual bool UsesCheckout() const override;
	virtual void Tick() override;
	virtual TArray< TSharedRef<class ISourceControlLabel> > GetLabels( const FString& InMatchingSpec ) const override;
#if SOURCE_CONTROL_WITH_SLATE
	virtual TSharedRef<class SWidget> MakeSettingsWidget() const override;
#endif

	/**
	 * Run a Plastic "version" command to check the availability of the binary and of the workspace.
	 */
	void CheckPlasticAvailability();

	/** Is Plastic command line working. */
	inline bool IsPlasticAvailable() const
	{
		return bPlasticAvailable;
	}

	/** Is Plastic workspace found. */
	inline bool IsWorkspaceFound() const
	{
		return bWorkspaceFound;
	}

	/** Get the path to the root of the Plastic workspace: can be the GameDir itself, or any parent directory */
	inline const FString& GetPathToWorkspaceRoot() const
	{
		return PathToWorkspaceRoot;
	}

	/** Get the Plastic current user */
	inline const FString& GetUserName() const
	{
		return UserName;
	}

	/** Get the Plastic current workspace */
	inline const FString& GetWorkspaceName() const
	{
		return WorkspaceName;
	}

	/** Get the Plastic current repository */
	inline const FString& GetRepositoryName() const
	{
		return RepositoryName;
	}

	/** Get the Plastic current server URL */
	inline const FString& GetServerUrl() const
	{
		return ServerUrl;
	}

	/** Get the Name of the current branch */
	inline const FString& GetBranchName() const
	{
		return BranchName;
	}

	/** Get the current Changeset Number */
	inline int32 GetChangesetNumber() const
	{
		return ChangesetNumber;
	}

	/** Helper function used to update state cache */
	TSharedRef<FPlasticSourceControlState, ESPMode::ThreadSafe> GetStateInternal(const FString& Filename);

	/**
	 * Register a worker with the provider.
	 * This is used internally so the provider can maintain a map of all available operations.
	 */
	void RegisterWorker( const FName& InName, const FGetPlasticSourceControlWorker& InDelegate );

	/** Remove a named file from the state cache */
	bool RemoveFileFromCache(const FString& Filename);

private:

	/** Is Plastic binary found and working. */
	bool bPlasticAvailable;

	/** Is Plastic workspace found. */
	bool bWorkspaceFound;

	/** Indicates if source control integration is available or not. */
	bool bServerAvailable;

	/** Helper function for Execute() */
	TSharedPtr<class IPlasticSourceControlWorker, ESPMode::ThreadSafe> CreateWorker(const FName& InOperationName) const;

	/** Helper function for running command synchronously. */
	ECommandResult::Type ExecuteSynchronousCommand(class FPlasticSourceControlCommand& InCommand, const FText& Task);
	/** Issue a command asynchronously if possible. */
	ECommandResult::Type IssueCommand(class FPlasticSourceControlCommand& InCommand);

	/** Output any messages this command holds */
	void OutputCommandMessages(const class FPlasticSourceControlCommand& InCommand) const;

	/** Update workspace status on Connect and UpdateStatus operations */
	void UpdateWorkspaceStatus(const class FPlasticSourceControlCommand& InCommand);

	/** Path to the root of the Plastic workspace: can be the GameDir itself, or any parent directory (found by the "Connect" operation) */
	FString PathToWorkspaceRoot;

	/** Plastic current user */
	FString UserName;

	/** Plastic current workspace */
	FString WorkspaceName;

	/** Plastic current repository */
	FString RepositoryName;

	/** Plastic current server URL */
	FString ServerUrl;

	/** Name of the current branch */
	FString BranchName;

	/** Current Changeset Number */
	int32 ChangesetNumber;

	/** State cache */
	TMap<FString, TSharedRef<class FPlasticSourceControlState, ESPMode::ThreadSafe> > StateCache;

	/** The currently registered source control operations */
	TMap<FName, FGetPlasticSourceControlWorker> WorkersMap;

	/** Queue for commands given by the main thread */
	TArray < FPlasticSourceControlCommand* > CommandQueue;

	/** For notifying when the source control states in the cache have changed */
	FSourceControlStateChanged OnSourceControlStateChanged;

	/** Source Control Menu Extension */
	FPlasticSourceControlMenu PlasticSourceControlMenu;
};
