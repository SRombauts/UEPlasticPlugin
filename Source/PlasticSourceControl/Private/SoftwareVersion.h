// Copyright (c) 2016-2022 Codice Software

#pragma once

#include "CoreMinimal.h"

/**
 * Software version string in the form "X.Y.Z.C", ie Major.Minor.Patch.Changeset (as returned by GetPlasticScmVersion)
*/
struct FSoftwareVersion
{
	FSoftwareVersion() : String(TEXT("<unknown-version>")) {}

	explicit FSoftwareVersion(FString&& InVersionString);
	FSoftwareVersion(const int& InMajor, const int& InMinor, const int& InPatch, const int& InChangeset);

	FString String;

	int Major = 0;
	int Minor = 0;
	int Patch = 0;
	int Changeset = 0;
};

bool operator==(const FSoftwareVersion& Rhs, const FSoftwareVersion& Lhs);
bool operator<(const FSoftwareVersion& Rhs, const FSoftwareVersion& Lhs);
bool operator>=(const FSoftwareVersion& Rhs, const FSoftwareVersion& Lhs);
