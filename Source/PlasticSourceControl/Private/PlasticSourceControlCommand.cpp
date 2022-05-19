// Copyright (c) 2016-2022 Codice Software

#include "PlasticSourceControlCommand.h"
#include "PlasticSourceControlModule.h"
#include "ISourceControlOperation.h"
#include "Modules/ModuleManager.h"
#include "HAL/PlatformTime.h"


FPlasticSourceControlCommand::FPlasticSourceControlCommand(const FSourceControlOperationRef& InOperation, const FPlasticSourceControlWorkerRef& InWorker, const FSourceControlOperationComplete& InOperationCompleteDelegate)
	: Operation(InOperation)
	, Worker(InWorker)
	, OperationCompleteDelegate(InOperationCompleteDelegate)
	, bExecuteProcessed(0)
	, bCommandSuccessful(false)
	, bConnectionDropped(false)
	, bAutoDelete(true)
	, Concurrency(EConcurrency::Synchronous)
	, StartTimestamp(FPlatformTime::Seconds())
{
	// grab the providers settings here, so we don't access them once the worker thread is launched
	check(IsInGameThread());
	const FPlasticSourceControlModule& PlasticSourceControl = FPlasticSourceControlModule::Get();
	PathToWorkspaceRoot = PlasticSourceControl.GetProvider().GetPathToWorkspaceRoot();
	ChangesetNumber = PlasticSourceControl.GetProvider().GetChangesetNumber();
}

bool FPlasticSourceControlCommand::DoWork()
{
	bCommandSuccessful = Worker->Execute(*this);
	FPlatformAtomics::InterlockedExchange(&bExecuteProcessed, 1);

	return bCommandSuccessful;
}

void FPlasticSourceControlCommand::Abandon()
{
	FPlatformAtomics::InterlockedExchange(&bExecuteProcessed, 1);
}

void FPlasticSourceControlCommand::DoThreadedWork()
{
	Concurrency = EConcurrency::Asynchronous;
	DoWork();
}
