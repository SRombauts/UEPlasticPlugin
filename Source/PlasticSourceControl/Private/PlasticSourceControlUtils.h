// Copyright (c) 2016-2022 Codice Software

#pragma once

#include "CoreMinimal.h"
#include "ISourceControlProvider.h"
#include "PlasticSourceControlRevision.h"

#include "Runtime/Launch/Resources/Version.h"

class FPlasticSourceControlChangelistState;
class FPlasticSourceControlCommand;
class FPlasticSourceControlState;
struct FSoftwareVersion;

/**
 * Helper struct for maintaining temporary files for passing to commands
 */
class FScopedTempFile
{
public:
	/** Constructor - open & write string to temp file */
	explicit FScopedTempFile(const FString& InText);
	explicit FScopedTempFile(const FText& InText);

	/** Destructor - delete temp file */
	~FScopedTempFile();

	/** Get the filename of this temp file - empty if it failed to be created */
	const FString& GetFilename() const;

private:
	/** The filename we are writing to */
	FString Filename;
};

namespace PlasticSourceControlUtils
{

const struct FSoftwareVersion& GetOldestSupportedPlasticScmVersion();

/**
 * Find the path to the Plastic binary: for now relying on the Path to access the "cm" command.
 */
FString FindPlasticBinaryPath();

/**
 * Launch the Plastic SCM "shell" command to run it in background.
 * @param	InPathToPlasticBinary	The path to the Plastic binary
 * @param	InWorkspaceRoot			The workspace from where to run the command - usually the Game directory
 * @returns true if the command succeeded and returned no errors
 */
bool LaunchBackgroundPlasticShell(const FString& InPathToPlasticBinary, const FString& InWorkspaceRoot);

/** Terminate the background 'cm shell' process and associated pipes */
void Terminate();

/**
 * Find the root of the Plastic workspace, looking from the GameDir and upward in its parent directories
 * @param InPathToGameDir		The path to the Game Directory
 * @param OutWorkspaceRoot		The path to the root directory of the Plastic workspace if found, else the path to the GameDir
 * @returns true if the command succeeded and returned no errors
 */
bool FindRootDirectory(const FString& InPathToGameDir, FString& OutWorkspaceRoot);

/**
 * Get Plastic SCM cli version
 * @param	OutCliVersion		Version of the Plastic SCM Command Line Interface tool
*/
bool GetPlasticScmVersion(FSoftwareVersion& OutPlasticScmVersion);

/**
 * Get Plastic SCM current user
 * @param	OutUserName			Name of the Plastic SCM user configured globally
 */
void GetUserName(FString& OutUserName);

/**
 * Get Plastic workspace name
 * @param	InWorkspaceRoot		The workspace from where to run the command - usually the Game directory
 * @param	OutWorkspaceName	Name of the current workspace
 * @param	OutErrorMessages	Any errors (from StdErr) as an array per-line
*/
bool GetWorkspaceName(const FString& InWorkspaceRoot, FString& OutWorkspaceName, TArray<FString>& OutErrorMessages);

/**
 * Get Plastic repository name and server URL, branch name and current changeset number
 * @param	OutChangeset		The current Changeset Number
 * @param	OutRepositoryName	Name of the repository of the current workspace
 * @param	OutServerUrl		URL/Port of the server of the repository
 * @param	OutBranchName		Name of the current checked-out branch
 * @param	OutErrorMessages	Any errors (from StdErr) as an array per-line
 */
bool GetWorkspaceInformation(int32& OutChangeset, FString& OutRepositoryName, FString& OutServerUrl, FString& OutBranchName, TArray<FString>& OutErrorMessages);

/**
 * Get Plastic repository name, server URL, and branch name
 *
 * Note: this is a local fast variant, not making network call to the server.
 * 
 * @param	OutBranchName		Name of the current checked-out branch
 * @param	OutRepositoryName	Name of the repository of the current workspace
 * @param	OutServerUrl		URL/Port of the server of the repository
 * @param	OutErrorMessages	Any errors (from StdErr) as an array per-line
 */
bool GetWorkspaceInfo(FString& OutBranchName, FString& OutRepositoryName, FString& OutServerUrl, TArray<FString>& OutErrorMessages);

/**
 * Use the Project Settings to replace Plastic SCM full username/e-mail by a shorter version for display.
 *
 * Used when retrieving the username of a revision, to display in history and content browser asset tooltip.
 *
 * @param	InUserName			The Plastic SCM username to shorten for display.
 */
FString UserNameToDisplayName(const FString& InUserName);

/**
 * Run a Plastic command - output is a string TArray.
 *
 * @param	InCommand			The Plastic command - e.g. commit
 * @param	InParameters		The parameters to the Plastic command
 * @param	InFiles				The files to be operated on
 * @param	InConcurrency		Is the command running in the background, or blocking the main thread
 * @param	OutResults			The results (from StdOut) as an array per-line
 * @param	OutErrorMessages	Any errors (from StdErr) as an array per-line
 * @returns true if the command succeeded and returned no errors
 */
bool RunCommand(const FString& InCommand, const TArray<FString>& InParameters, const TArray<FString>& InFiles, const EConcurrency::Type InConcurrency, TArray<FString>& OutResults, TArray<FString>& OutErrorMessages);
// Run a Plastic command - output is a string.
bool RunCommandInternal(const FString& InCommand, const TArray<FString>& InParameters, const TArray<FString>& InFiles, const EConcurrency::Type InConcurrency, FString& OutResults, FString& OutErrors);

/**
 * Run a Plastic "status" command and parse it.
 *
 * @param	InFiles				The files to be operated on
 * @param	bInUpdateHistory	If getting the history of files, force execute the fileinfo command required to do get RepSpec of xlinks (history view or visual diff)
 * @param	InConcurrency		Is the command running in the background, or blocking the main thread
 * @param	OutErrorMessages	Any errors (from StdErr) as an array per-line
 * @param	OutStates			States of the files
 * @param	OutChangeset		The current Changeset Number
 * @param	OutBranchName		Name of the current checked-out branch
 * @returns true if the command succeeded and returned no errors
 */
bool RunUpdateStatus(const TArray<FString>& InFiles, const bool bInUpdateHistory, const EConcurrency::Type InConcurrency, TArray<FString>& OutErrorMessages, TArray<FPlasticSourceControlState>& OutStates, int32& OutChangeset, FString& OutBranchName);

/**
 * Run a Plastic "cat" command to dump the binary content of a revision into a file.
 *
 * @param	InPathToPlasticBinary	The path to the Plastic binary
 * @param	InRevSpec				The revision specification to get
 * @param	InDumpFileName			The temporary file to dump the revision
 * @returns true if the command succeeded and returned no errors
*/
bool RunDumpToFile(const FString& InPathToPlasticBinary, const FString& InRevSpec, const FString& InDumpFileName);

/**
 * Run Plastic "history" and "log" commands and parse their results.
 *
 * @param	bInUpdateHistory	If getting the history of files, versus only checking the heads of branches to detect newer commits
 * @param	InOutStates			The file states to update with the history
 * @param	OutErrorMessages	Any errors (from StdErr) as an array per-line
 */
bool RunGetHistory(const bool bInUpdateHistory, TArray<FPlasticSourceControlState>& InOutStates, TArray<FString>& OutErrorMessages);

#if ENGINE_MAJOR_VERSION == 5

/**
 * Run a Plastic "status --changelist --xml" and parse its result.
 * @param	InConcurrency
 * @param	OutChangelistsStates	The list of changelists (without their files)
 * @param	OutCLFilesStates		The list of files per changelist
 * @param	OutErrorMessages		Any errors (from StdErr) as an array per-line
 */
bool RunGetChangelists(const EConcurrency::Type InConcurrency, TArray<FPlasticSourceControlChangelistState>& OutChangelistsStates, TArray<TArray<FPlasticSourceControlState>>& OutCLFilesStates, TArray<FString>& OutErrorMessages);

#endif

/**
 * Helper function for various commands to update cached states.
 * @returns true if any states were updated
 */
bool UpdateCachedStates(TArray<FPlasticSourceControlState>&& InStates);

/** 
 * Remove redundant errors (that contain a particular string) and also
 * update the commands success status if all errors were removed.
 */
void RemoveRedundantErrors(FPlasticSourceControlCommand& InCommand, const FString& InFilter);

/**
 * Change LogSourceControl verbosity level at startup and when toggled from the Plastic Source Control Settings
 *
 * Override to Verbose or back to Log, but only if the current log verbosity is not already set to VeryVerbose
 */
void SwitchVerboseLogs(const bool bInEnable);

/**
 * Find the best(longest) common directory between two paths, terminated by a slash, returning an empty string if none.
 * Assumes that both input strings are already normalized paths, slash delimited, for performance reason.
 */
FString FindCommonDirectory(const FString& InPath1, const FString& InPath2);

} // namespace PlasticSourceControlUtils
