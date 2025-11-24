// Copyright (c) 2025 Unity Technologies

#pragma once

#include "CoreMinimal.h"
#include "ISourceControlProvider.h"
#include "IUnityVersionControlWorker.h"
#include "UnityVersionControlConsole.h"
#include "UnityVersionControlMenu.h"
#include "UnityVersionControlSettings.h"
#include "SoftwareVersion.h"

#include "Runtime/Launch/Resources/Version.h"

#if ENGINE_MAJOR_VERSION == 5
#include "ISourceControlChangelistState.h"
#include "UnityVersionControlChangelist.h"
#endif

DECLARE_DELEGATE_RetVal_OneParam(FUnityVersionControlWorkerRef, FGetUnityVersionControlWorker, FUnityVersionControlProvider&)

class FUnityVersionControlProvider : public ISourceControlProvider
{
public:
	/** Constructor & destructor */
	FUnityVersionControlProvider();
	~FUnityVersionControlProvider();

	/* ISourceControlProvider implementation */
	virtual void Init(bool bForceConnection = true) override;
	virtual void Close() override;
	virtual FText GetStatusText() const override;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
	virtual TMap<EStatus, FString> GetStatus() const override; /* NOTE: added in UE5.3, requires the new EStatus */
#endif
	virtual bool IsEnabled() const override;
	virtual bool IsAvailable() const override;
	virtual const FName& GetName(void) const override;
	virtual bool QueryStateBranchConfig(const FString& ConfigSrc, const FString& ConfigDest) override { return false; }
	virtual void RegisterStateBranches(const TArray<FString>& BranchNames, const FString& ContentRoot) override {}
	virtual int32 GetStateBranchIndex(const FString& InBranchName) const override { return INDEX_NONE; }
	virtual bool GetStateBranchAtIndex(int32 BranchIndex, FString& OutBranchName) const /* override NOTE: added in UE5.7 */ { return false; }
	virtual ECommandResult::Type GetState(const TArray<FString>& InFiles, TArray<FSourceControlStateRef>& OutState, EStateCacheUsage::Type InStateCacheUsage) override;
#if ENGINE_MAJOR_VERSION == 5
	virtual ECommandResult::Type GetState(const TArray<FSourceControlChangelistRef>& InChangelists, TArray<FSourceControlChangelistStateRef>& OutState, EStateCacheUsage::Type InStateCacheUsage) override;
#endif
	virtual TArray<FSourceControlStateRef> GetCachedStateByPredicate(TFunctionRef<bool(const FSourceControlStateRef&)> Predicate) const override;
	virtual FDelegateHandle RegisterSourceControlStateChanged_Handle(const FSourceControlStateChanged::FDelegate& SourceControlStateChanged) override;
	virtual void UnregisterSourceControlStateChanged_Handle(FDelegateHandle Handle) override;
#if ENGINE_MAJOR_VERSION == 4
	virtual ECommandResult::Type Execute(const FSourceControlOperationRef& InOperation, const TArray<FString>& InFiles, EConcurrency::Type InConcurrency = EConcurrency::Synchronous, const FSourceControlOperationComplete& InOperationCompleteDelegate = FSourceControlOperationComplete() ) override;
#elif ENGINE_MAJOR_VERSION == 5
	virtual ECommandResult::Type Execute(const FSourceControlOperationRef& InOperation, FSourceControlChangelistPtr InChangelist, const TArray<FString>& InFiles, EConcurrency::Type InConcurrency = EConcurrency::Synchronous, const FSourceControlOperationComplete& InOperationCompleteDelegate = FSourceControlOperationComplete() ) override;
#endif
	virtual bool CanExecuteOperation(const FSourceControlOperationRef& InOperation) const; /* override	NOTE: added in UE5.3 */
	virtual bool CanCancelOperation(const FSourceControlOperationRef& InOperation) const override;
	virtual void CancelOperation(const FSourceControlOperationRef& InOperation) override;
	virtual bool UsesLocalReadOnlyState() const override;
	virtual bool UsesChangelists() const override;
	virtual bool UsesUncontrolledChangelists() const; /* override	NOTE: added in UE5.2 */
	virtual bool UsesCheckout() const override;
	virtual bool UsesFileRevisions() const; /* override				NOTE: added in UE5.1 */
	virtual bool UsesSnapshots() const; /* override					NOTE: added in UE5.2 */
	virtual bool AllowsDiffAgainstDepot() const; /* override		NOTE: added in UE5.2 */
	virtual TOptional<bool> IsAtLatestRevision() const; /* override	NOTE: added in UE5.1 */
	virtual TOptional<int> GetNumLocalChanges() const; /* override	NOTE: added in UE5.1 */
	virtual void Tick() override;
	virtual TArray<TSharedRef<class ISourceControlLabel>> GetLabels(const FString& InMatchingSpec) const override;
#if ENGINE_MAJOR_VERSION == 5
	virtual TArray<FSourceControlChangelistRef> GetChangelists(EStateCacheUsage::Type InStateCacheUsage) override;
#endif
#if SOURCE_CONTROL_WITH_SLATE
	virtual TSharedRef<class SWidget> MakeSettingsWidget() const override;
#endif

	using ISourceControlProvider::Execute;

	/**
	 * Run a Plastic "version" command to check the availability of the binary and of the workspace.
	 */
	void CheckPlasticAvailability();

	/** Is Plastic command line working. */
	bool IsPlasticAvailable() const
	{
		return bPlasticAvailable;
	}

	/** Is Plastic workspace found. */
	bool IsWorkspaceFound() const
	{
		return bWorkspaceFound;
	}

	/** Get the path to the root of the Plastic workspace: can be the GameDir itself, or any parent directory */
	const FString& GetPathToWorkspaceRoot() const
	{
		return PathToWorkspaceRoot;
	}

	/** List of configured profiles (known servers and their associated user name). */
	const TMap<FString, FString>& GetProfiles() const
	{
		return Profiles;
	}

	/** Get the Plastic current user */
	const FString& GetUserName() const
	{
		return UserName;
	}

	/** Get the Plastic current workspace */
	const FString& GetWorkspaceName() const
	{
		return WorkspaceName;
	}

	/** Get the Plastic current repository */
	const FString& GetRepositoryName() const
	{
		return RepositoryName;
	}

	/** Get the Plastic current server URL. See also GetCloudOrganization() */
	const FString& GetServerUrl() const
	{
		return ServerUrl;
	}

	/** Get the current repository fully qualified specification */
	const FString GetRepositorySpecification() const
	{
		return FString::Printf(TEXT("%s@%s"), *RepositoryName, *ServerUrl);
	}

	void UpdateServerUrl(const FString& InServerUrl);

	/** Get the Name of the current branch */
	const FString& GetBranchName() const
	{
		return BranchName;
	}
	/** Get the Name of the current selector */
	const FString& GetWorkspaceSelector() const
	{
		return WorkspaceSelector;
	}
	void SetWorkspaceSelector(const FString& InWorkspaceSelector, const FString& InBranchName)
	{
		WorkspaceSelector = InWorkspaceSelector;
		BranchName = InBranchName;
	}

	/** Get the current Changeset Number */
	int32 GetChangesetNumber() const
	{
		return ChangesetNumber;
	}

	/** A partial/Gluon workspace doesn't match with a single changeset, which is identified by -1 */
	bool IsPartialWorkspace() const
	{
		return (ChangesetNumber == -1);
	}

	/** Version of the Unity Version Control executable used */
	const FSoftwareVersion& GetPlasticScmVersion() const
	{
		return PlasticScmVersion;
	}

	/** Version of the Unity Version Control plugin */
	const FString& GetPluginVersion() const
	{
		return PluginVersion;
	}

	/** Return the name of the cloud organization from the ServerUrl if applicable (MyOrganization@cloud) or an empty string */
	FString GetCloudOrganization() const;

	/** Set list of error messages that occurred after last Plastic command */
	void SetLastErrors(const TArray<FString>& InErrors);

	/** Get list of error messages that occurred after last Plastic command */
	TArray<FString> GetLastErrors() const;

	/** Helper function used to update state cache */
	TSharedRef<class FUnityVersionControlState, ESPMode::ThreadSafe> GetStateInternal(const FString& InFilename);

#if ENGINE_MAJOR_VERSION == 5
	/** Helper function used to update changelists state cache */
	TSharedRef<class FUnityVersionControlChangelistState, ESPMode::ThreadSafe> GetStateInternal(const FUnityVersionControlChangelist& InChangelist);
#endif

	/**
	 * Register a worker with the provider.
	 * This is used internally so the provider can maintain a map of all available operations.
	 */
	void RegisterWorker(const FName& InName, const FGetUnityVersionControlWorker& InDelegate);

	/** Remove a named file from the state cache */
	bool RemoveFileFromCache(const FString& Filename);

#if ENGINE_MAJOR_VERSION == 5
	/** Remove a changelist from the state cache */
	bool RemoveChangelistFromCache(const FUnityVersionControlChangelist& Changelist);

	/** Returns a list of changelists from the cache based on a given predicate */
	TArray<FSourceControlChangelistStateRef> GetCachedStateByPredicate(TFunctionRef<bool(const FSourceControlChangelistStateRef&)> Predicate) const;
#endif

	/** Access the Plastic source control settings */
	FUnityVersionControlSettings& AccessSettings()
	{
		return UnityVersionControlSettings;
	}
	const FUnityVersionControlSettings& AccessSettings() const
	{
		return UnityVersionControlSettings;
	}

	/** Save the Plastic source control settings */
	void SaveSettings()
	{
		UnityVersionControlSettings.SaveSettings();
	}

private:
	/** Is Plastic binary found and working. */
	bool bPlasticAvailable = false;

	/** Is Plastic workspace found. */
	bool bWorkspaceFound = false;

	/** Indicates if source control integration is available or not. */
	bool bServerAvailable = false;

	/** Whether Unity Version Control is configured to uses local read-only state to signal whether a file is editable ("SetFilesAsReadOnly" in client.conf) */
	bool bUsesLocalReadOnlyState = false;

	/** Critical section for thread safety of error messages that occurred after last Plastic command */
	mutable FCriticalSection LastErrorsCriticalSection;

	/** List of error messages that occurred after last Plastic command */
	TArray<FString> LastErrors;

	/** Helper function for Execute() */
	TSharedPtr<class IUnityVersionControlWorker, ESPMode::ThreadSafe> CreateWorker(const FName& InOperationName);

	/** Helper function for running command synchronously. */
	ECommandResult::Type ExecuteSynchronousCommand(class FUnityVersionControlCommand& InCommand, const FText& Task);
	/** Issue a command asynchronously if possible. */
	ECommandResult::Type IssueCommand(class FUnityVersionControlCommand& InCommand);

	/** Output any messages this command holds */
	void OutputCommandMessages(const class FUnityVersionControlCommand& InCommand) const;

	/** Update workspace status on Connect and UpdateStatus operations */
	void UpdateWorkspaceStatus(const class FUnityVersionControlCommand& InCommand);

	/** Called after a package has been saved to disk, to update the source control cache */
#if ENGINE_MAJOR_VERSION == 4
	void HandlePackageSaved(const FString& InPackageFilename, UObject* Outer);
#else
	void HandlePackageSaved(const FString& InPackageFilename, UPackage* InPackage, FObjectPostSaveContext InObjectSaveContext);
#endif

	/** Version of the Unity Version Control executable used */
	FSoftwareVersion PlasticScmVersion;

	/** Version of the Unity Version Control plugin */
	FString PluginVersion;

	/** List of configured profiles (servers and their corresponding user name). */
	TMap<FString, FString> Profiles;

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

	/** Name of the object that the workspace is switched to: usually the branch, sometimes a changeset or a label */
	FString WorkspaceSelector;

	/** Current Changeset Number */
	int32 ChangesetNumber = 0;

	/** State caches */
	TMap<FString, TSharedRef<class FUnityVersionControlState, ESPMode::ThreadSafe> > StateCache;
#if ENGINE_MAJOR_VERSION == 5
	TMap<FUnityVersionControlChangelist, TSharedRef<class FUnityVersionControlChangelistState, ESPMode::ThreadSafe> > ChangelistsStateCache;
#endif

	/** The currently registered source control operations */
	TMap<FName, FGetUnityVersionControlWorker> WorkersMap;

	/** Queue for commands given by the main thread */
	TArray < FUnityVersionControlCommand* > CommandQueue;

	/** For notifying when the source control states in the cache have changed */
	FSourceControlStateChanged OnSourceControlStateChanged;

	/** Source Control Console commands */
	FUnityVersionControlConsole UnityVersionControlConsole;

	/** Source Control Menu Extension */
	FUnityVersionControlMenu UnityVersionControlMenu;

	/** The settings for Plastic source control */
	FUnityVersionControlSettings UnityVersionControlSettings;
};
