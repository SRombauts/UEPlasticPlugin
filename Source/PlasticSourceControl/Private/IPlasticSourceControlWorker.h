// Copyright (c) 2016-2022 Codice Software

#pragma once

#include "CoreMinimal.h"

class FPlasticSourceControlProvider;

class IPlasticSourceControlWorker
{
public:
	// Implemented in PlasticSourceControlOperations.cpp with all the code for the workers
	static void RegisterWorkers(FPlasticSourceControlProvider& PlasticSourceControlProvider);

public:
	/**
	 * Name describing the work that this worker does. Used for factory method hookup.
	 */
	virtual FName GetName() const = 0;

	/**
	 * Function that actually does the work. Can be executed on another thread.
	 */
	virtual bool Execute( class FPlasticSourceControlCommand& InCommand ) = 0;

	/**
	 * Updates the state of any items after completion (if necessary). This is always executed on the main thread.
	 * @returns true if states were updated
	 */
	virtual bool UpdateStates() = 0;
};

typedef TSharedRef<IPlasticSourceControlWorker, ESPMode::ThreadSafe> FPlasticSourceControlWorkerRef;
