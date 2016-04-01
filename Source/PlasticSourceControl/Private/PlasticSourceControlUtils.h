// Copyright (c) 2016 Codice Software - Sebastien Rombauts (sebastien.rombauts@gmail.com)

#pragma once

#include "PlasticSourceControlState.h"
#include "PlasticSourceControlRevision.h"

class FPlasticSourceControlCommand;

/**
 * Helper struct for maintaining temporary files for passing to commands
 */
class FScopedTempFile
{
public:

	/** Constructor - open & write string to temp file */
	FScopedTempFile(const FText& InText);

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

/**
 * Find the path to the Plastic binary: for now relying on the Path to access the "cm" command.
 */
FString FindPlasticBinaryPath();

/**
 * Run a Plastic "version" command to check the availability of the binary.
 * @param InPathToPlasticBinary	The path to the Plastic binary
 * @returns true if the command succeeded and returned no errors
 */
bool CheckPlasticAvailability(const FString& InPathToPlasticBinary);

/**
 * Find the root of the Plastic repository, looking from the GameDir and upward in its parent directories
 * @param InPathToGameDir		The path to the Game Directory
 * @param OutRepositoryRoot		The path to the root directory of the Plastic repository if found, else the path to the GameDir
 * @returns true if the command succeeded and returned no errors
 */
bool FindRootDirectory(const FString& InPathToGameDir, FString& OutRepositoryRoot);

/**
 * Get Plastic current checked-out branch
 * @param	InPathToPlasticBinary	The path to the Plastic binary
 * @param	InRepositoryRoot		The Plastic repository from where to run the command - usually the Game directory (can be empty)
 * @param	OutBranchName			Name of the current checked-out branch (if any, ie. not in detached HEAD)
 */
void GetBranchName(const FString& InPathToPlasticBinary, const FString& InRepositoryRoot, FString& OutBranchName);

/**
 * Run a Plastic command - output is a string TArray.
 *
 * @param	InCommand				The Plastic command - e.g. commit
 * @param	InPathToPlasticBinary	The path to the Plastic binary
 * @param	InRepositoryRoot		The Plastic repository from where to run the command - usually the Game directory (can be empty)
 * @param	InParameters			The parameters to the Plastic command
 * @param	InFiles					The files to be operated on
 * @param	OutResults				The results (from StdOut) as an array per-line
 * @param	OutErrorMessages		Any errors (from StdErr) as an array per-line
 * @returns true if the command succeeded and returned no errors
 */
bool RunCommand(const FString& InCommand, const FString& InPathToPlasticBinary, const FString& InRepositoryRoot, const TArray<FString>& InParameters, const TArray<FString>& InFiles, TArray<FString>& OutResults, TArray<FString>& OutErrorMessages);

/**
 * Run a Plastic "fileinfo" (similar to "status") command and parse it.
 *
 * @param	InPathToPlasticBinary	The path to the Plastic binary
 * @param	InRepositoryRoot	The Plastic repository from where to run the command - usually the Game directory (can be empty)
 * @param	InFiles				The files to be operated on
 * @param	OutErrorMessages	Any errors (from StdErr) as an array per-line
 * @returns true if the command succeeded and returned no errors
 */
bool RunUpdateStatus(const FString& InPathToPlasticBinary, const FString& InRepositoryRoot, const TArray<FString>& InFiles, TArray<FString>& OutErrorMessages, TArray<FPlasticSourceControlState>& OutStates);

/**
 * Helper function for various commands to update cached states.
 * @returns true if any states were updated
 */
bool UpdateCachedStates(const TArray<FPlasticSourceControlState>& InStates);

/** 
 * Remove redundant errors (that contain a particular string) and also
 * update the commands success status if all errors were removed.
 */
void RemoveRedundantErrors(FPlasticSourceControlCommand& InCommand, const FString& InFilter);

}
