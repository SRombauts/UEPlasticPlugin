// Copyright (c) 2016-2022 Codice Software

#include "PlasticSourceControlConsole.h"

#include "ISourceControlModule.h"

#include "PlasticSourceControlUtils.h"

void FPlasticSourceControlConsole::Register()
{
	if (!CmConsoleCommand.IsValid())
	{
		CmConsoleCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("cm"),
			TEXT("PlasticSCM Command Line Interface.\n")
			TEXT("Run any 'cm' command directly from the Unreal Editor Console.\n")
			TEXT("Type 'cm showcommands' to get a command list."),
			FConsoleCommandWithArgsDelegate::CreateRaw(this, &FPlasticSourceControlConsole::ExecutePlasticConsoleCommand)
		);
	}
}

void FPlasticSourceControlConsole::Unregister()
{
	CmConsoleCommand.Reset();
}

void FPlasticSourceControlConsole::ExecutePlasticConsoleCommand(const TArray<FString>& a_args)
{
	// If called with no argument, explicitly call "cm help" instead to mimic the cm CLI behavior.
	if (a_args.Num() < 1)
	{
		ExecutePlasticConsoleCommand(TArray<FString>({TEXT("help")}));
		return;
	}

	FString Results;
	FString Errors;
	const FString Command = a_args[0];
	TArray<FString> Parameters = a_args;
	Parameters.RemoveAt(0);
	PlasticSourceControlUtils::RunCommandInternal(Command, Parameters, TArray<FString>(), EConcurrency::Synchronous, Results, Errors);
	UE_LOG(LogSourceControl, Log, TEXT("Output:\n%s"), *Results);
}
