// Copyright Unity Technologies

#pragma once

#include "CoreMinimal.h"
#include "SoftwareVersion.h"

/**
 * List of key versions of Plastic SCM enabling new features for "cm" CLI.
 *
 * Plastic SCM version strings in the form "X.Y.Z.C", ie Major.Minor.Patch.Changeset (as returned by GetPlasticScmVersion)
*/
namespace PlasticSourceControlVersions
{
	// Oldest supported version of "cm" (from 2021/01/05)
	// 9.0.16.4839 cm changelist 'persistent' flag now contain a '--' prefix.
	// https://www.plasticscm.com/download/releasenotes/9.0.16.4839
	static FSoftwareVersion OldestSupported(TEXT("9.0.16.4839"));

	// 11.0.16.7248 add support for --descriptionfile for multi-line descriptions and support for special characters
	// https://www.plasticscm.com/download/releasenotes/11.0.16.7248
	static const FSoftwareVersion NewChangelistFileArgs(TEXT("11.0.16.7248"));

	// 11.0.16.7504 add support for 'cm shelveset apply' to only unshelved the desired changes from a shelve
	// https://www.plasticscm.com/download/releasenotes/11.0.16.7504
	static const FSoftwareVersion ShelvesetApplySelection(TEXT("11.0.16.7504"));

	// 11.0.16.7608 add support for history --limit. It displays the N last revisions of the specified items.
	// https://www.plasticscm.com/download/releasenotes/11.0.16.7608
	static const FSoftwareVersion NewHistoryLimit(TEXT("11.0.16.7608"));

} // namespace PlasticSourceControlVersions
