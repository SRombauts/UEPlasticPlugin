// Copyright (c) 2016 Codice Software - Sebastien Rombauts (sebastien.rombauts@gmail.com)

#pragma once

#include "IPlasticSourceControlWorker.h"
#include "PlasticSourceControlState.h"
#include "PlasticSourceControlRevision.h"

/** Called when first activated on a project, and then at project load time.
 *  Look for the root directory of the Plastic repository (where the ".Plastic/" subdirectory is located). */
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

/** Add an untraked file to source control (so only a subset of the Plastic add command). */
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
	/** Map of filenames to Plastic state */
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
	/** Map of filenames to Plastic state */
	TArray<FPlasticSourceControlState> States;
};

/** @todo Plastic pull --rebase to update branch from its configure remote
class FPlasticSyncWorker : public IPlasticSourceControlWorker
{
public:
	virtual ~FPlasticSyncWorker() {}
	// IPlasticSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FPlasticSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

public:
	// Map of filenames to Plastic state
	TArray<FPlasticSourceControlState> States;
};
*/

/** Get source control status of files on local working copy. */
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

/** @todo Copy or Move operation on a single file
class FPlasticCopyWorker : public IPlasticSourceControlWorker
{
public:
	virtual ~FPlasticCopyWorker() {}
	// IPlasticSourceControlWorker interface
	virtual FName GetName() const override;
	virtual bool Execute(class FPlasticSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;

public:
	// Temporary states for results
	TArray<FPlasticSourceControlState> OutStates;
};
*/

/** @todo Plastic add to mark a conflict as resolved
class FPlasticResolveWorker : public IPlasticSourceControlWorker
{
public:
	virtual ~FPlasticResolveWorker() {}
	virtual FName GetName() const override;
	virtual bool Execute(class FPlasticSourceControlCommand& InCommand) override;
	virtual bool UpdateStates() const override;
	
private:
	// Temporary states for results
	TArray<FPlasticSourceControlState> States;
};
*/