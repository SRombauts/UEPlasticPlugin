// Copyright (c) 2016-2022 Codice Software

// Specialization of classes defined in Engine\Source\Developer\SourceControl\Public\SourceControlOperations.h

#pragma once

#include "CoreMinimal.h"
#include "IPlasticSourceControlWorker.h"
#include "PlasticSourceControlState.h"
#include "PlasticSourceControlRevision.h"

#include "ISourceControlOperation.h"


class FPlasticSourceControlProvider;

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
	explicit FPlasticConnectWorker(FPlasticSourceControlProvider& InSourceControlProvider)
		: IPlasticSourceControlWorker(InSourceControlProvider)
	{}
	virtual ~FPlasticConnectWorker() {}
	// IPlasticSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FPlasticSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() override;

public:
	/** Temporary states for results */
	TArray<FPlasticSourceControlState> States;
};

class FPlasticCheckOutWorker : public IPlasticSourceControlWorker
{
public:
	explicit FPlasticCheckOutWorker(FPlasticSourceControlProvider& InSourceControlProvider)
		: IPlasticSourceControlWorker(InSourceControlProvider)
	{}
	virtual ~FPlasticCheckOutWorker() {}
	// IPlasticSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FPlasticSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() override;

public:
	/** Temporary states for results */
	TArray<FPlasticSourceControlState> States;
};

/** Check-in a set of file to the local depot. */
class FPlasticCheckInWorker : public IPlasticSourceControlWorker
{
public:
	explicit FPlasticCheckInWorker(FPlasticSourceControlProvider& InSourceControlProvider)
		: IPlasticSourceControlWorker(InSourceControlProvider)
	{}
	virtual ~FPlasticCheckInWorker() {}
	// IPlasticSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FPlasticSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() override;

public:
	/** Temporary states for results */
	TArray<FPlasticSourceControlState> States;
};

/** Add an untracked file to source control (so only a subset of the Plastic add command). */
class FPlasticMarkForAddWorker : public IPlasticSourceControlWorker
{
public:
	explicit FPlasticMarkForAddWorker(FPlasticSourceControlProvider& InSourceControlProvider)
		: IPlasticSourceControlWorker(InSourceControlProvider)
	{}
	virtual ~FPlasticMarkForAddWorker() {}
	// IPlasticSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FPlasticSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() override;

public:
	/** Temporary states for results */
	TArray<FPlasticSourceControlState> States;
};

/** Delete a file and remove it from source control. */
class FPlasticDeleteWorker : public IPlasticSourceControlWorker
{
public:
	explicit FPlasticDeleteWorker(FPlasticSourceControlProvider& InSourceControlProvider)
		: IPlasticSourceControlWorker(InSourceControlProvider)
	{}
	virtual ~FPlasticDeleteWorker() {}
	// IPlasticSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FPlasticSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() override;

public:
	/** Temporary states for results */
	TArray<FPlasticSourceControlState> States;
};

/** Revert any change to a file to its state on the local depot. */
class FPlasticRevertWorker : public IPlasticSourceControlWorker
{
public:
	explicit FPlasticRevertWorker(FPlasticSourceControlProvider& InSourceControlProvider)
		: IPlasticSourceControlWorker(InSourceControlProvider)
	{}
	virtual ~FPlasticRevertWorker() {}
	// IPlasticSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FPlasticSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() override;
};

/** Revert only unchanged file(s) (uncheckout). */
class FPlasticRevertUnchangedWorker : public IPlasticSourceControlWorker
{
public:
	explicit FPlasticRevertUnchangedWorker(FPlasticSourceControlProvider& InSourceControlProvider)
		: IPlasticSourceControlWorker(InSourceControlProvider)
	{}
	virtual ~FPlasticRevertUnchangedWorker() {}
	// IPlasticSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FPlasticSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() override;

public:
	/** Temporary states for results */
	TArray<FPlasticSourceControlState> States;
};

/** Revert all checked-out file(s). */
class FPlasticRevertAllWorker : public IPlasticSourceControlWorker
{
public:
	explicit FPlasticRevertAllWorker(FPlasticSourceControlProvider& InSourceControlProvider)
		: IPlasticSourceControlWorker(InSourceControlProvider)
	{}
	virtual ~FPlasticRevertAllWorker() {}
	// IPlasticSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FPlasticSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() override;

public:
	/** Temporary states for results */
	TArray<FPlasticSourceControlState> States;
};

/** Initialize a new Workspace and a new Repository */
class FPlasticMakeWorkspaceWorker : public IPlasticSourceControlWorker
{
public:
	explicit FPlasticMakeWorkspaceWorker(FPlasticSourceControlProvider& InSourceControlProvider)
		: IPlasticSourceControlWorker(InSourceControlProvider)
	{}
	virtual ~FPlasticMakeWorkspaceWorker() {}
	// IPlasticSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FPlasticSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() override;
};

/** Plastic update the workspace to latest changes */
class FPlasticSyncWorker : public IPlasticSourceControlWorker
{
public:
	explicit FPlasticSyncWorker(FPlasticSourceControlProvider& InSourceControlProvider)
		: IPlasticSourceControlWorker(InSourceControlProvider)
	{}
	virtual ~FPlasticSyncWorker() {}
	// IPlasticSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FPlasticSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() override;

public:
	/** Temporary states for results */
	TArray<FPlasticSourceControlState> States;
};

/** Get source control status of files on local workspace. */
class FPlasticUpdateStatusWorker : public IPlasticSourceControlWorker
{
public:
	explicit FPlasticUpdateStatusWorker(FPlasticSourceControlProvider& InSourceControlProvider)
		: IPlasticSourceControlWorker(InSourceControlProvider)
	{}
	virtual ~FPlasticUpdateStatusWorker() {}
	// IPlasticSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FPlasticSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() override;

public:
	/** Temporary states for results */
	TArray<FPlasticSourceControlState> States;
};

/** Copy or Move operation on a single file */
class FPlasticCopyWorker : public IPlasticSourceControlWorker
{
public:
	explicit FPlasticCopyWorker(FPlasticSourceControlProvider& InSourceControlProvider)
		: IPlasticSourceControlWorker(InSourceControlProvider)
	{}
	virtual ~FPlasticCopyWorker() {}
	// IPlasticSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FPlasticSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() override;

public:
	/** Temporary states for results */
	TArray<FPlasticSourceControlState> States;
};

/** Plastic command to mark the conflict as solved */
class FPlasticResolveWorker : public IPlasticSourceControlWorker
{
public:
	explicit FPlasticResolveWorker(FPlasticSourceControlProvider& InSourceControlProvider)
		: IPlasticSourceControlWorker(InSourceControlProvider)
	{}
	virtual ~FPlasticResolveWorker() {}
	virtual FName GetName() const override;
	virtual bool Execute(class FPlasticSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() override;

private:
	/** Temporary states for results */
	TArray<FPlasticSourceControlState> States;
};
