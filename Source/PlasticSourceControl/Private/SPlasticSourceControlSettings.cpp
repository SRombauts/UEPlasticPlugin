// Copyright (c) 2016 Codice Software - Sebastien Rombauts (sebastien.rombauts@gmail.com)

#include "PlasticSourceControlPrivatePCH.h"
#include "SPlasticSourceControlSettings.h"
#include "PlasticSourceControlModule.h"
#include "PlasticSourceControlUtils.h"

#define LOCTEXT_NAMESPACE "SPlasticSourceControlSettings"

void SPlasticSourceControlSettings::Construct(const FArguments& InArgs)
{
	FSlateFontInfo Font = FEditorStyle::GetFontStyle(TEXT("SourceControl.LoginWindow.Font"));

	bAutoCreateIgnoreFile = true;
	bAutoInitialCommit = true;

	InitialCommitMessage = LOCTEXT("InitialCommitMessage", "Initial checkin");
	ServerUrl = FText::FromString(TEXT("localhost:8087"));

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage( FEditorStyle::GetBrush("DetailsView.CategoryBottom"))
		.Padding(FMargin(0.0f, 3.0f, 0.0f, 0.0f))
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.FillHeight(1.5f)
			.Padding(2.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(2.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("BinaryPathLabel", "Plastic SCM Path"))
						.ToolTipText(LOCTEXT("BinaryPathLabel_Tooltip", "Path to the Plastic SCM binary"))
						.Font(Font)
					]
				]
				+SHorizontalBox::Slot()
				.FillWidth(2.0f)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(2.0f)
					[
						SNew(SEditableTextBox)
						.Text(this, &SPlasticSourceControlSettings::GetBinaryPathText)
						.ToolTipText(LOCTEXT("BinaryPathLabel_Tooltip", "Path to the Plastic SCM binary"))
						.HintText(LOCTEXT("BinaryPathLabel", "Path to the Plastic SCM binary"))
						.OnTextCommitted(this, &SPlasticSourceControlSettings::OnBinaryPathTextCommited)
						.Font(Font)
					]
				]
			]
			+SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(2.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(2.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("WorkspaceRootLabel", "Root of the workspace"))
						.ToolTipText(LOCTEXT("WorkspaceRootLabel_Tooltip", "Path to the root of the Plastic SCM workspace"))
						.Font(Font)
					]
				]
				+SHorizontalBox::Slot()
				.FillWidth(2.0f)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(2.0f)
					[
						SNew(STextBlock)
						.Text(this, &SPlasticSourceControlSettings::GetPathToWorkspaceRoot)
						.ToolTipText(LOCTEXT("WorkspaceRootLabel_Tooltip", "Path to the root of the Plastic SCM workspace"))
						.Font(Font)
					]
				]
			]
			+SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(2.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(2.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("PlasticUserName", "User Name"))
						.ToolTipText(LOCTEXT("PlasticUserName_Tooltip", "User name configured for the Plastic SCM workspace"))
						.Font(Font)
					]
				]
				+SHorizontalBox::Slot()
				.FillWidth(2.0f)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(2.0f)
					[
						SNew(STextBlock)
						.Text(this, &SPlasticSourceControlSettings::GetUserName)
						.ToolTipText(LOCTEXT("PlasticUserName_Tooltip", "User name configured for the Plastic SCM workspace"))
						.Font(Font)
					]
				]
			]
			+SVerticalBox::Slot()
			.Padding( 5, 5 )
			.AutoHeight()
			.VAlign( VAlign_Center )
			[
				SNew( SSeparator )
			]
			+SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(2.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				.Visibility(this, &SPlasticSourceControlSettings::CanInitializePlasticWorkspace)
				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("WorkspaceNotFound", "Current Project is not contained in a Plastic SCM Workspace. Fill the form below to initialize a new Workspace."))
					.ToolTipText(LOCTEXT("WorkspaceNotFound_Tooltip", "No Workspace found at the level or above the current Project"))
					.Font(Font)
				]
			]
			+SVerticalBox::Slot()
			.FillHeight(1.5f)
			.Padding(2.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				.Visibility(this, &SPlasticSourceControlSettings::CanInitializePlasticWorkspace)
				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(2.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("WorkspaceName", "Workspace Name"))
						.ToolTipText(LOCTEXT("WorkspaceName_Tooltip", "Enter the Name of the new Workspace to create"))
						.Font(Font)
					]
				]
				+SHorizontalBox::Slot()
				.FillWidth(2.0f)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.FillHeight(1.5f)
					.Padding(2.0f)
					[
						SNew(SEditableTextBox)
						.Text(this, &SPlasticSourceControlSettings::GetWorkspaceName)
						.ToolTipText(LOCTEXT("WorkspaceName_Tooltip", "Enter the Name of the new Workspace to create"))
						.HintText(LOCTEXT("WorkspaceName_Hint", "Workspace Name to create"))
						.OnTextCommitted(this, &SPlasticSourceControlSettings::OnWorkspaceNameCommited)
						.Font(Font)
					]
				]
			]
			+SVerticalBox::Slot()
			.FillHeight(1.5f)
			.Padding(2.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				.Visibility(this, &SPlasticSourceControlSettings::CanInitializePlasticWorkspace)
				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(2.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("RepositoryName", "Repository Name"))
						.ToolTipText(LOCTEXT("RepositoryName_Tooltip", "Enter the Name of the new Repository to create"))
						.Font(Font)
					]
				]
				+SHorizontalBox::Slot()
				.FillWidth(2.0f)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(2.0f)
					[
						SNew(SEditableTextBox)
						.Text(this, &SPlasticSourceControlSettings::GetRepositoryName)
						.ToolTipText(LOCTEXT("RepositoryName_Tooltip", "Enter the Name of the new Repository to create"))
						.HintText(LOCTEXT("RepositoryName_Hint", "Repository Name to create"))
						.OnTextCommitted(this, &SPlasticSourceControlSettings::OnRepositoryNameCommited)
						.Font(Font)
					]
				]
			]
			+SVerticalBox::Slot()
			.FillHeight(1.5f)
			.Padding(2.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				.Visibility(this, &SPlasticSourceControlSettings::CanInitializePlasticWorkspace)
				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(2.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ServerUrl", "Server URL address:port"))
						.ToolTipText(LOCTEXT("ServerUrl_Tooltip", "Enter the Server URL in the form address:port (localhost:8087)"))
						.Font(Font)
					]
				]
				+SHorizontalBox::Slot()
				.FillWidth(2.0f)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(2.0f)
					[
						SNew(SEditableTextBox)
						.Text(this, &SPlasticSourceControlSettings::GetServerUrl)
						.ToolTipText(LOCTEXT("ServerUrl_Tooltip", "Enter the Server URL in the form address:port (localhost:8087)"))
						.HintText(LOCTEXT("ServerUrl", "Enter the Server URL"))
						.OnTextCommitted(this, &SPlasticSourceControlSettings::OnServerUrlCommited)
						.Font(Font)
					]
				]
			]
			+SVerticalBox::Slot()
			.FillHeight(1.5f)
			.Padding(2.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				.Visibility(this, &SPlasticSourceControlSettings::CanInitializePlasticWorkspace)
				+SHorizontalBox::Slot()
				.FillWidth(0.1f)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(2.0f)
					.VAlign(VAlign_Center)
					[
						SNew(SCheckBox)
						.ToolTipText(LOCTEXT("CreateIgnoreFile_Tooltip", "Create and add a standard 'ignore.conf' file"))
						.IsChecked(ECheckBoxState::Checked)
						.OnCheckStateChanged(this, &SPlasticSourceControlSettings::OnCheckedCreateIgnoreFile)
					]
				]
				+SHorizontalBox::Slot()
				.FillWidth(2.9f)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(2.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("CreateIgnoreFile", "Add a ignore.conf file"))
						.ToolTipText(LOCTEXT("CreateIgnoreFile_Tooltip", "Create and add a standard 'ignore.conf' file"))
						.Font(Font)
					]
				]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				.Visibility(this, &SPlasticSourceControlSettings::CanInitializePlasticWorkspace)
				+SHorizontalBox::Slot()
				.FillWidth(0.1f)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(2.0f)
					.VAlign(VAlign_Center)
					[
						SNew(SCheckBox)
						.ToolTipText(LOCTEXT("InitialCommit_Tooltip", "Make the initial Plastic SCM checkin"))
						.IsChecked(ECheckBoxState::Checked)
						.OnCheckStateChanged(this, &SPlasticSourceControlSettings::OnCheckedInitialCommit)
					]
				]
				+SHorizontalBox::Slot()
				.FillWidth(0.9f)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(2.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("InitialCommit", "Make the initial Plastic SCM Checkin"))
						.ToolTipText(LOCTEXT("InitialCommit_Tooltip", "Click to make the initial Plastic SCM Checkin"))
						.Font(Font)
					]
				]
				+SHorizontalBox::Slot()
				.FillWidth(2.0f)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.AutoHeight()
					.Padding(2.0f)
					[
						SNew(SMultiLineEditableTextBox)
						.Text(this, &SPlasticSourceControlSettings::GetInitialCommitMessage)
						.ToolTipText(LOCTEXT("InitialCommitMessage_Tooltip", "Enter the message for the initial checkin"))
						.HintText(LOCTEXT("InitialCommitMessage_Hint", "Message for the initial checkin"))
						.OnTextCommitted(this, &SPlasticSourceControlSettings::OnInitialCommitMessageCommited)
						.Font(Font)
					]
				]
			]
			+SVerticalBox::Slot()
			.FillHeight(2.0f)
			.Padding(2.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				.Visibility(this, &SPlasticSourceControlSettings::CanInitializePlasticWorkspace)
				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.FillHeight(2.0f)
					.Padding(2.0f)
					.VAlign(VAlign_Center)
					.AutoHeight()
					[
						SNew(SButton)
						.IsEnabled(this, &SPlasticSourceControlSettings::IsReadyToInitializePlasticWorkspace)
						.Text(LOCTEXT("PlasticInitWorkspace", "Create a new Plastic SCM workspace for the current project"))
						.ToolTipText(LOCTEXT("PlasticInitWorkspace_Tooltip", "Create and initialize a new Plastic SCM workspace and repository for the current project"))
						.OnClicked(this, &SPlasticSourceControlSettings::OnClickedInitializePlasticWorkspace)
						.HAlign(HAlign_Center)
					]
				]
			]
			+SVerticalBox::Slot()
			.FillHeight(2.0f)
			.Padding(2.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				.Visibility(this, &SPlasticSourceControlSettings::CanAddIgnoreFile)
				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.FillHeight(2.0f)
					.Padding(2.0f)
					.VAlign(VAlign_Center)
					.AutoHeight()
					[
						SNew(SButton)
						.Text(LOCTEXT("CreateIgnoreFile", "Add a ignore.conf file"))
						.ToolTipText(LOCTEXT("CreateIgnoreFile_Tooltip", "Create and add a standard 'ignore.conf' file"))
						.OnClicked(this, &SPlasticSourceControlSettings::OnClickedAddIgnoreFile)
						.HAlign(HAlign_Center)
					]
				]
			]
		]
	];
}

FText SPlasticSourceControlSettings::GetBinaryPathText() const
{
	FPlasticSourceControlModule& PlasticSourceControl = FModuleManager::LoadModuleChecked<FPlasticSourceControlModule>("PlasticSourceControl");
	return FText::FromString(PlasticSourceControl.AccessSettings().GetBinaryPath());
}

void SPlasticSourceControlSettings::OnBinaryPathTextCommited(const FText& InText, ETextCommit::Type InCommitType) const
{
	FPlasticSourceControlModule& PlasticSourceControl = FModuleManager::LoadModuleChecked<FPlasticSourceControlModule>("PlasticSourceControl");
	PlasticSourceControl.AccessSettings().SetBinaryPath(InText.ToString());
	PlasticSourceControl.SaveSettings();
}

FText SPlasticSourceControlSettings::GetPathToWorkspaceRoot() const
{
	FPlasticSourceControlModule& PlasticSourceControl = FModuleManager::LoadModuleChecked<FPlasticSourceControlModule>("PlasticSourceControl");
	return FText::FromString(PlasticSourceControl.GetProvider().GetPathToWorkspaceRoot());
}

FText SPlasticSourceControlSettings::GetUserName() const
{
	FPlasticSourceControlModule& PlasticSourceControl = FModuleManager::LoadModuleChecked<FPlasticSourceControlModule>("PlasticSourceControl");
	return FText::FromString(PlasticSourceControl.GetProvider().GetUserName());
}


EVisibility SPlasticSourceControlSettings::CanInitializePlasticWorkspace() const
{
	FPlasticSourceControlModule& PlasticSourceControl = FModuleManager::LoadModuleChecked<FPlasticSourceControlModule>("PlasticSourceControl");
	const bool bPlasticAvailable = PlasticSourceControl.GetProvider().IsPlasticAvailable();
	const bool bPlasticWorkspaceFound = PlasticSourceControl.GetProvider().IsWorkspaceFound();
	return (bPlasticAvailable && !bPlasticWorkspaceFound) ? EVisibility::Visible : EVisibility::Collapsed;
}

bool SPlasticSourceControlSettings::IsReadyToInitializePlasticWorkspace() const
{
	// Workspace Name cannot be left empty
	const bool bWorkspaceNameOk = !WorkspaceName.IsEmpty();
	// Either RepositoryName should be empty, or ServerUrl should also be filled
	const bool bRepositoryNameOk = RepositoryName.IsEmpty() || !ServerUrl.IsEmpty();
	// If Initial Commit is requested, checkin message cannot be empty
	const bool bInitialCommitOk = (!bAutoInitialCommit || !InitialCommitMessage.IsEmpty());
	return bWorkspaceNameOk && bRepositoryNameOk && bInitialCommitOk;
}


void SPlasticSourceControlSettings::OnWorkspaceNameCommited(const FText& InText, ETextCommit::Type InCommitType)
{
	WorkspaceName = InText;
}
FText SPlasticSourceControlSettings::GetWorkspaceName() const
{
	return WorkspaceName;
}

void SPlasticSourceControlSettings::OnRepositoryNameCommited(const FText& InText, ETextCommit::Type InCommitType)
{
	RepositoryName = InText;
}
FText SPlasticSourceControlSettings::GetRepositoryName() const
{
	return RepositoryName;
}

void SPlasticSourceControlSettings::OnServerUrlCommited(const FText& InText, ETextCommit::Type InCommitType)
{
	ServerUrl = InText;
}
FText SPlasticSourceControlSettings::GetServerUrl() const
{
	return ServerUrl;
}

void SPlasticSourceControlSettings::OnCheckedCreateIgnoreFile(ECheckBoxState NewCheckedState)
{
	bAutoCreateIgnoreFile = (NewCheckedState == ECheckBoxState::Checked);
}

void SPlasticSourceControlSettings::OnCheckedInitialCommit(ECheckBoxState NewCheckedState)
{
	bAutoInitialCommit = (NewCheckedState == ECheckBoxState::Checked);
}

void SPlasticSourceControlSettings::OnInitialCommitMessageCommited(const FText& InText, ETextCommit::Type InCommitType)
{
	InitialCommitMessage = InText;
}

FText SPlasticSourceControlSettings::GetInitialCommitMessage() const
{
	return InitialCommitMessage;
}


FReply SPlasticSourceControlSettings::OnClickedInitializePlasticWorkspace()
{
	FPlasticSourceControlModule& PlasticSourceControl = FModuleManager::LoadModuleChecked<FPlasticSourceControlModule>("PlasticSourceControl");
	const FString& PathToPlasticBinary = PlasticSourceControl.AccessSettings().GetBinaryPath();
	TArray<FString> InfoMessages;
	TArray<FString> ErrorMessages;
	bool bResult;

	UE_LOG(LogSourceControl, Log, TEXT("InitializePlasticWorkspace(%s, %s, %s) CreateIgnore=%d Commit=%d"),
		*WorkspaceName.ToString(), *RepositoryName.ToString(), *ServerUrl.ToString(), bAutoCreateIgnoreFile, bAutoInitialCommit);

	if (!RepositoryName.IsEmpty())
	{
		TArray<FString> Parameters;
		Parameters.Add(ServerUrl.ToString());
		Parameters.Add(RepositoryName.ToString());
		PlasticSourceControlUtils::RunCommand(TEXT("makerepository"), Parameters, TArray<FString>(), InfoMessages, ErrorMessages);
	}
	{
		TArray<FString> Parameters;
		Parameters.Add(WorkspaceName.ToString());
		Parameters.Add(TEXT(".")); // current path, ie. GameDir
		if (!RepositoryName.IsEmpty())
		{
			// working only if repository already exists
			Parameters.Add(FString::Printf(TEXT("--repository=rep:%s@repserver:%s"), *RepositoryName.ToString(), *ServerUrl.ToString()));
		}
		bResult = PlasticSourceControlUtils::RunCommand(TEXT("makeworkspace"), Parameters, TArray<FString>(), InfoMessages, ErrorMessages);
	}
	if (bResult)
	{
		// Check the new workspace status to enable connection
		PlasticSourceControl.GetProvider().CheckPlasticAvailability();
	}
	if (PlasticSourceControl.GetProvider().IsWorkspaceFound())
	{
		TArray<FString> ProjectFiles;
		ProjectFiles.Add(FPaths::GetCleanFilename(FPaths::GetProjectFilePath()));
		ProjectFiles.Add(FPaths::GetCleanFilename(FPaths::GameConfigDir()));
		ProjectFiles.Add(FPaths::GetCleanFilename(FPaths::GameContentDir()));
		if (FPaths::DirectoryExists(FPaths::GameSourceDir()))
		{
			ProjectFiles.Add(FPaths::GetCleanFilename(FPaths::GameSourceDir()));
		}
		if (bAutoCreateIgnoreFile)
		{
			// Create a standard "ignore.conf" file with common patterns for a typical Blueprint & C++ project
			if (CreateIgnoreFile())
			{
				ProjectFiles.Add(TEXT("ignore.conf"));
			}
		}
		{
			// Add .uproject, Config/, Content/ and Source/ files (and ignore.conf if any)
			TArray<FString> Parameters;
			Parameters.Add(TEXT("-R"));
			bResult = PlasticSourceControlUtils::RunCommand(TEXT("add"), Parameters, ProjectFiles, InfoMessages, ErrorMessages);
		}
		if (bAutoInitialCommit && bResult)
		{
			UE_LOG(LogSourceControl, Log, TEXT("FCheckIn(%s)"), *InitialCommitMessage.ToString());

			// optional initial checkin with custom message: launch a "CheckIn" Operation
			ISourceControlModule& SourceControl = FModuleManager::LoadModuleChecked<ISourceControlModule>("SourceControl");
			ISourceControlProvider& Provider = SourceControl.GetProvider();
			CheckInOperation = ISourceControlOperation::Create<FCheckIn>();
			CheckInOperation->SetDescription(InitialCommitMessage);
			Provider.Execute(CheckInOperation.ToSharedRef(), EConcurrency::Asynchronous, FSourceControlOperationComplete::CreateSP(this, &SPlasticSourceControlSettings::OnSourceControlOperationComplete));
		}
	}
	return FReply::Handled();
}


void SPlasticSourceControlSettings::OnSourceControlOperationComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult)
{
	if (InResult == ECommandResult::Succeeded)
	{
		check(InOperation->GetName() == "CheckIn");
		check(CheckInOperation == StaticCastSharedRef<FCheckIn>(InOperation));

		UE_LOG(LogSourceControl, Log, TEXT("Initial CheckIn succeeded: '%s'"), *CheckInOperation->GetSuccessMessage().ToString());
	}
	else
	{
		UE_LOG(LogSourceControl, Error, TEXT("Initial CheckIn failed!"));
	}

// TODO:	GetWorkspacesOperation.Reset();
}

/** Delegate to check for presence of a Plastic ignore.conf file to an existing Plastic SCM workspace */
EVisibility SPlasticSourceControlSettings::CanAddIgnoreFile() const
{
	FPlasticSourceControlModule& PlasticSourceControl = FModuleManager::LoadModuleChecked<FPlasticSourceControlModule>("PlasticSourceControl");
	const bool bPlasticWorkspaceFound = PlasticSourceControl.GetProvider().IsWorkspaceFound();
	const bool bIgnoreFileFound = FPaths::FileExists(GetIgnoreFileName());
	return (bPlasticWorkspaceFound && !bIgnoreFileFound) ? EVisibility::Visible : EVisibility::Collapsed;
}

/** Delegate to add a Plastic ignore.conf file to an existing Plastic SCM workspace */
FReply SPlasticSourceControlSettings::OnClickedAddIgnoreFile() const
{
	if (CreateIgnoreFile())
	{
		// Add ignore.conf to Plastic SCM
		TArray<FString> InfoMessages;
		TArray<FString> ErrorMessages;
		TArray<FString> Parameters;
		Parameters.Add(TEXT("-R"));
		TArray<FString> Files;
		Files.Add(TEXT("ignore.conf"));
		PlasticSourceControlUtils::RunCommand(TEXT("add"), Parameters, Files, InfoMessages, ErrorMessages);
	}
	return FReply::Handled();
}


/** Path to the "ignore.conf" file */
const FString& SPlasticSourceControlSettings::GetIgnoreFileName() const
{
	static const FString PathToGameDir = FPaths::ConvertRelativePathToFull(FPaths::GameDir());
	static const FString IgnoreFileName = FPaths::Combine(*PathToGameDir, TEXT("ignore.conf"));
	return IgnoreFileName;
}

/** Create a standard "ignore.conf" file with common patterns for a typical Blueprint & C++ project */
bool SPlasticSourceControlSettings::CreateIgnoreFile() const
{
	const FString IgnoreFileContent = TEXT("Binaries\nBuild\nDerivedDataCache\nIntermediate\nSaved\n.vs\n*.VC.db\n*.opensdf\n*.opendb\n*.sdf\n*.sln\n*.suo\n*.xcodeproj\n*.xcworkspace");
	return FFileHelper::SaveStringToFile(IgnoreFileContent, *GetIgnoreFileName(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}


#undef LOCTEXT_NAMESPACE
