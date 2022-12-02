// Copyright Unity Technologies

#pragma once

#include "CoreMinimal.h"

/**
 * Software version string in the form "X.Y.Z.C", ie Major.Minor.Patch.Changeset (as returned by GetPlasticScmVersion)
*/
struct FSoftwareVersion
{
	FSoftwareVersion() : String(TEXT("<unknown-version>")) {}

	explicit FSoftwareVersion(FString&& InVersionString);

	FString String;

	int Major = 0;
	int Minor = 0;
	int Patch = 0;
	int Changeset = 0;
};

bool operator==(const FSoftwareVersion& Rhs, const FSoftwareVersion& Lhs);
bool operator<(const FSoftwareVersion& Rhs, const FSoftwareVersion& Lhs);
bool operator>=(const FSoftwareVersion& Rhs, const FSoftwareVersion& Lhs);
