// Copyright (c) 2016-2022 Codice Software

#include "SoftwareVersion.h"

FSoftwareVersion::FSoftwareVersion(FString&& InVersionString)
{
	String = MoveTemp(InVersionString);
	TArray<FString> Parts;
	const int32 N = String.ParseIntoArray(Parts, TEXT("."));
	if (N == 4)
	{
		Major = FCString::Atoi(*Parts[0]);
		Minor = FCString::Atoi(*Parts[1]);
		Patch = FCString::Atoi(*Parts[2]);
		Changeset = FCString::Atoi(*Parts[3]);
	}
}

FSoftwareVersion::FSoftwareVersion(const int& InMajor, const int& InMinor, const int& InPatch, const int& InChangeset)
{
	String = FString::Printf(TEXT("%d.%d.%d.%d"), InMajor, InMinor, InPatch);
	Major = InMajor;
	Minor = InMinor;
	Patch = InPatch;
	Changeset = InChangeset;
}

bool operator==(const FSoftwareVersion& Rhs, const FSoftwareVersion& Lhs)
{
	return (Rhs.Major == Lhs.Major) && (Rhs.Minor == Lhs.Minor) && (Rhs.Patch == Lhs.Patch) && (Rhs.Changeset == Lhs.Changeset);
}

bool operator<(const FSoftwareVersion& Rhs, const FSoftwareVersion& Lhs)
{
	if (Rhs.Major < Lhs.Major) return true;
	if (Rhs.Major > Lhs.Major) return false;
	if (Rhs.Minor < Lhs.Minor) return true;
	if (Rhs.Minor > Lhs.Minor) return false;
	if (Rhs.Patch < Lhs.Patch) return true;
	if (Rhs.Patch > Lhs.Patch) return false;
	if (Rhs.Changeset < Lhs.Changeset) return true;
	if (Rhs.Changeset > Lhs.Changeset) return false;
	return false; // Equal
}
