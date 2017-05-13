// Copyright (c) 2016-2017 Codice Software - Sebastien Rombauts (sebastien.rombauts@gmail.com)

// Specialization of classes defineds in Engine\Source\Developer\SourceControl\Public\SourceControlOperations.h

#pragma once

#include "CoreMinimal.h"
#include "IPlasticSourceControlWorker.h"
#include "PlasticSourceControlState.h"
#include "PlasticSourceControlRevision.h"

#include "ISourceControlOperation.h"


/**
 * Internal operation used to revert checked-out unchanged files
*/
class FPlasticRevertUnchanged : public ISourceControlOperation
{
public:
	// ISourceControlOperation interface
	virtual FName GetName() const override;

	virtual FText GetInProgressString() const override;
};


/**
 * Internal operation used to revert checked-out files
*/
class FPlasticRevertAll : public ISourceControlOperation
{
public:
	// ISourceControlOperation interface
	virtual FName GetName() const override;

	virtual FText GetInProgressString() const override;
};


/**
* Internal operation used to initialize a new Workspace and a new Repository
*/
class FPlasticMakeWorkspace : public ISourceControlOperation
{
public:
	// ISourceControlOperation interface
	virtual FName GetName() const override;

	virtual FText GetInProgressString() const override;

	FString WorkspaceName;
	FString RepositoryName;
	FString ServerUrl;
};


/** Called when first activated on a project, and then at project load time.
 *  Look for the root directory of the Plastic workspace (where the ".plastic/" subdirectory is located). */
class FPlasticConnectWorker : public IPlasticSourceControlWorker
{
public:
	virtual ~FPlasticConnectWorker() {}
	// IPlasticSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FPlasticSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;
};

class FPlasticCheckOutWorker : public IPlasticSourceControlWorker
{
public:
	virtual ~FPlasticCheckOutWorker() {}
	// IPlasticSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FPlasticSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

public:
	/** Temporary states for results */
	TArray<FPlasticSourceControlState> States;
};

/** Check-in a set of file to the local depot. */
class FPlasticCheckInWorker : public IPlasticSourceControlWorker
{
public:
	virtual ~FPlasticCheckInWorker() {}
	// IPlasticSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FPlasticSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

public:
	/** Temporary states for results */
	TArray<FPlasticSourceControlState> States;
};

/** Add an untracked file to source control (so only a subset of the Plastic add command). */
class FPlasticMarkForAddWorker : public IPlasticSourceControlWorker
{
public:
	virtual ~FPlasticMarkForAddWorker() {}
	// IPlasticSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FPlasticSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

public:
	/** Temporary states for results */
	TArray<FPlasticSourceControlState> States;
};

/** Delete a file and remove it from source control. */
class FPlasticDeleteWorker : public IPlasticSourceControlWorker
{
public:
	virtual ~FPlasticDeleteWorker() {}
	// IPlasticSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FPlasticSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

public:
	/** Temporary states for results */
	TArray<FPlasticSourceControlState> States;
};

/** Revert any change to a file to its state on the local depot. */
class FPlasticRevertWorker : public IPlasticSourceControlWorker
{
public:
	virtual ~FPlasticRevertWorker() {}
	// IPlasticSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FPlasticSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

public:
	/** Temporary states for results */
	TArray<FPlasticSourceControlState> States;
};

/** Revert only unchanged file(s) (uncheckout). */
class FPlasticRevertUnchangedWorker : public IPlasticSourceControlWorker
{
public:
	virtual ~FPlasticRevertUnchangedWorker() {}
	// IPlasticSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FPlasticSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;
};

/** Revert all checked-out file(s). */
class FPlasticRevertAllWorker : public IPlasticSourceControlWorker
{
public:
	virtual ~FPlasticRevertAllWorker() {}
	// IPlasticSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FPlasticSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;
};

/** Initialize a new Workspace and a new Repository */
class FPlasticMakeWorkspaceWorker : public IPlasticSourceControlWorker
{
public:
	virtual ~FPlasticMakeWorkspaceWorker() {}
	// IPlasticSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FPlasticSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;
};

/** Plastic update the workspace to latest changes */
class FPlasticSyncWorker : public IPlasticSourceControlWorker
{
public:
	virtual ~FPlasticSyncWorker() {}
	// IPlasticSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FPlasticSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

public:
	/** Temporary states for results */
	TArray<FPlasticSourceControlState> States;
};

/** Get source control status of files on local workspace. */
class FPlasticUpdateStatusWorker : public IPlasticSourceControlWorker
{
public:
	virtual ~FPlasticUpdateStatusWorker() {}
	// IPlasticSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FPlasticSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

public:
	/** Temporary states for results */
	TArray<FPlasticSourceControlState> States;

	/** Map of filenames to history */
	TMap<FString, TPlasticSourceControlHistory> Histories;
};

/** Copy or Move operation on a single file */
class FPlasticCopyWorker : public IPlasticSourceControlWorker
{
public:
	virtual ~FPlasticCopyWorker() {}
	// IPlasticSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FPlasticSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

public:
	/** Temporary states for results */
	TArray<FPlasticSourceControlState> States;
};

/** Plastic command to mark the conflict as solved */
class FPlasticResolveWorker : public IPlasticSourceControlWorker
{
public:
	virtual ~FPlasticResolveWorker() {}
	virtual FName GetName() const override;
	virtual bool Execute(class FPlasticSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

private:
	/** Temporary states for results */
	TArray<FPlasticSourceControlState> States;
};
