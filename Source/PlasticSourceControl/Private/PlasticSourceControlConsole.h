// Copyright (c) 2016-2022 Codice Software

#pragma once

#include "CoreMinimal.h"

/**
 * Editor only console commands.
 *
 *  Such commands can be executed from the editor output log window, but also from command line arguments,
 * from Editor Blueprints utilities, or from C++ Code using. eg. GEngine->Exec("cm status");
 */
class PlasticSourceControlConsole
{
public:
	// PlasticSCM Command Line Interface: Run 'cm' commands directly from the Unreal Editor Console.
	static void ExecutePlasticConsoleCommand(const TArray<FString>& a_args);
};