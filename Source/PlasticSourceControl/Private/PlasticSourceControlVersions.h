// Copyright (c) 2016-2022 Codice Software

#pragma once

#include "CoreMinimal.h"
#include "SoftwareVersion.h"

/**
 * Plastic SCM version strings in the form "X.Y.Z.C", ie Major.Minor.Patch.Changeset (as returned by GetPlasticScmVersion)
*/
namespace PlasticSourceControlVersions
{
	// 11.0.16.7248 add support for --descriptionfile for multi-line descriptions and support for special characters
	// https://www.plasticscm.com/download/releasenotes/11.0.16.7248
	static const FSoftwareVersion NewChangelistFileArgs(TEXT("11.0.16.7248"));

	// 11.0.16.7608 add support for history --limit. It displays the N last revisions of the specified items.
	// https://www.plasticscm.com/download/releasenotes/11.0.16.7608
	static const FSoftwareVersion NewHistoryLimit(TEXT("11.0.16.7608"));

	// 9.0.16.4839 cm changelist 'persistent' flag now contain a '--' prefix.
	// https://www.plasticscm.com/download/releasenotes/9.0.16.4839
	static FSoftwareVersion OldestSupported(TEXT("9.0.16.4839"));

} // namespace PlasticSourceControlVersions
