// Copyright Unity Technologies

#pragma once

#include "CoreMinimal.h"
#include "ISourceControlProvider.h" // for EConcurrency

namespace PlasticSourceControlShell
{
#if PLATFORM_WINDOWS
	constexpr const TCHAR* pchDelim = TEXT("\r\n");
#else
	constexpr const TCHAR* pchDelim = TEXT("\n");
#endif


/**
 * Launch the Plastic SCM "shell" command line process to run it in the background.
 *
 * @param	InPathToPlasticBinary	The path to the Plastic binary
 * @param	InWorkspaceRoot			The workspace from where to run the command - usually the Game directory
 * @returns true if the command succeeded and returned no errors
 */
bool Launch(const FString& InPathToPlasticBinary, const FString& InWorkspaceRoot);

/** Terminate the background 'cm shell' process and associated pipes */
void Terminate();


/**
 * Run a Plastic command - the result is the output of cm, as a multi-line string.
 *
 * @param	InCommand			The Plastic command - e.g. commit
 * @param	InParameters		The parameters to the Plastic command
 * @param	InFiles				The files to be operated on
 * @param	OutResults			The results (from StdOut) as a multi-line string.
 * @param	OutErrors			Any errors (from StdErr) as a multi-line string.
 * @returns true if the command succeeded and returned no errors
 */
bool RunCommand(const FString& InCommand, const TArray<FString>& InParameters, const TArray<FString>& InFiles, FString& OutResults, FString& OutErrors);

} // namespace PlasticSourceControlShell
