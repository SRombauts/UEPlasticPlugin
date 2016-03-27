// Copyright (c) 2016 Codice Software - Sebastien Rombauts (sebastien.rombauts@gmail.com)

#include "PlasticSourceControlPrivatePCH.h"
#include "PlasticSourceControlOperations.h"
#include "PlasticSourceControlState.h"
#include "PlasticSourceControlCommand.h"
#include "PlasticSourceControlModule.h"
#include "PlasticSourceControlUtils.h"

#define LOCTEXT_NAMESPACE "PlasticSourceControl"

FName FPlasticConnectWorker::GetName() const
{
	return "Connect";
}

bool FPlasticConnectWorker::Execute(FPlasticSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());

	InCommand.bCommandSuccessful = PlasticSourceControlUtils::RunCommand(TEXT("status"), InCommand.PathToPlasticBinary, InCommand.PathToRepositoryRoot, TArray<FString>(), TArray<FString>(), InCommand.InfoMessages, InCommand.ErrorMessages);
	if(!InCommand.bCommandSuccessful || InCommand.ErrorMessages.Num() > 0 || InCommand.InfoMessages.Num() == 0)
	{
		StaticCastSharedRef<FConnect>(InCommand.Operation)->SetErrorText(LOCTEXT("NotAPlasticRepository", "Failed to enable Plastic source control. You need to initialize the project as a Plastic repository first."));
		InCommand.bCommandSuccessful = false;
	}

	return InCommand.bCommandSuccessful;
}

bool FPlasticConnectWorker::UpdateStates() const
{
	return false;
}

#undef LOCTEXT_NAMESPACE
