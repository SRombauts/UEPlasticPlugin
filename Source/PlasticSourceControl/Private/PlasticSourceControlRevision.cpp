// Copyright (c) 2016 Codice Software - Sebastien Rombauts (sebastien.rombauts@gmail.com)

#include "PlasticSourceControlPrivatePCH.h"
#include "PlasticSourceControlRevision.h"
#include "PlasticSourceControlModule.h"
#include "PlasticSourceControlProvider.h"
#include "PlasticSourceControlUtils.h"
#include "SPlasticSourceControlSettings.h"

#define LOCTEXT_NAMESPACE "PlasticSourceControl"

bool FPlasticSourceControlRevision::Get( FString& InOutFilename ) const
{
	FPlasticSourceControlModule& PlasticSourceControl = FModuleManager::LoadModuleChecked<FPlasticSourceControlModule>("PlasticSourceControl");
	const FString& PathToPlasticBinary = PlasticSourceControl.AccessSettings().GetBinaryPath();
	const FString& PathToRepositoryRoot = PlasticSourceControl.GetProvider().GetPathToRepositoryRoot();

	// if a filename for the temp file wasn't supplied generate a unique-ish one
	if(InOutFilename.Len() == 0)
	{
		// create the diff dir if we don't already have it (Plastic wont)
		IFileManager::Get().MakeDirectory(*FPaths::DiffDir(), true);
		// create a unique temp file name based on the unique commit Id
		const FString TempFileName = FString::Printf(TEXT("%stemp-%s-%s"), *FPaths::DiffDir(), *CommitId, *FPaths::GetCleanFilename(Filename));
		InOutFilename = FPaths::ConvertRelativePathToFull(TempFileName);
	}

	// Diff against the revision
	const FString Parameter = FString::Printf(TEXT("%s:%s"), *CommitId, *Filename);

	bool bCommandSuccessful;
	if(FPaths::FileExists(InOutFilename))
	{
		bCommandSuccessful = true; // if the temp file already exists, reuse it directly
	}
	else
	{
// TODO
//		bCommandSuccessful = PlasticSourceControlUtils::RunDumpToFile(PathToPlasticBinary, PathToRepositoryRoot, Parameter, InOutFilename);
		bCommandSuccessful = false;
	}
	return bCommandSuccessful;
}

bool FPlasticSourceControlRevision::GetAnnotated( TArray<FAnnotationLine>& OutLines ) const
{
	return false;
}

bool FPlasticSourceControlRevision::GetAnnotated( FString& InOutFilename ) const
{
	return false;
}

const FString& FPlasticSourceControlRevision::GetFilename() const
{
	return Filename;
}

int32 FPlasticSourceControlRevision::GetRevisionNumber() const
{
	return RevisionNumber;
}

const FString& FPlasticSourceControlRevision::GetRevision() const
{
	return ShortCommitId;
}

const FString& FPlasticSourceControlRevision::GetDescription() const
{
	return Description;
}

const FString& FPlasticSourceControlRevision::GetUserName() const
{
	return UserName;
}

const FString& FPlasticSourceControlRevision::GetClientSpec() const
{
	static FString EmptyString(TEXT(""));
	return EmptyString;
}

const FString& FPlasticSourceControlRevision::GetAction() const
{
	return Action;
}

TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FPlasticSourceControlRevision::GetBranchSource() const
{
	// TODO if this revision was copied from some other revision, then that source revision should
	//       be returned here (this should be determined when history is being fetched)
	return nullptr;
}

const FDateTime& FPlasticSourceControlRevision::GetDate() const
{
	return Date;
}

int32 FPlasticSourceControlRevision::GetCheckInIdentifier() const
{
	// in Plastic, revisions apply to the whole repository so (in Perforce terms) the revision *is* the changelist
	return RevisionNumber;
}

int32 FPlasticSourceControlRevision::GetFileSize() const
{
	return FileSize;
}

#undef LOCTEXT_NAMESPACE
