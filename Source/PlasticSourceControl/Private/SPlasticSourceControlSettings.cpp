// Copyright (c) 2016-2017 Codice Software - Sebastien Rombauts (sebastien.rombauts@gmail.com)

#include "PlasticSourceControlPrivatePCH.h"
#include "SPlasticSourceControlSettings.h"
#include "PlasticSourceControlOperations.h"
#include "PlasticSourceControlModule.h"
#include "PlasticSourceControlUtils.h"

#include "Fonts/SlateFontInfo.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Modules/ModuleManager.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "EditorStyleSet.h"

#define LOCTEXT_NAMESPACE "SPlasticSourceControlSettings"

void SPlasticSourceControlSettings::Construct(const FArguments& InArgs)
{
	FSlateFontInfo Font = FEditorStyle::GetFontStyle(TEXT("SourceControl.LoginWindow.Font"));

	bAutoCreateIgnoreFile = true;
	bAutoInitialCommit = true;

	InitialCommitMessage = LOCTEXT("InitialCommitMessage", "Initial checkin");
	ServerUrl = FText::FromString(TEXT("localhost:8087"));
	if (FApp::HasGameName())
	{
		WorkspaceName = FText::FromString(FApp::GetGameName());
		RepositoryName = WorkspaceName;
	}

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage( FEditorStyle::GetBrush("DetailsView.CategoryBottom"))
		.Padding(FMargin(0.0f, 3.0f, 0.0f, 0.0f))
		[
			SNew(SVerticalBox)
			// Path to the Plastic SCM binary
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				.ToolTipText(LOCTEXT("BinaryPathLabel_Tooltip", "Path to the Plastic SCM cli binary"))
				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("BinaryPathLabel", "Plastic SCM Path"))
					.Font(Font)
				]
				+SHorizontalBox::Slot()
				.FillWidth(2.0f)
				[
					SNew(SEditableTextBox)
					.Text(this, &SPlasticSourceControlSettings::GetBinaryPathText)
					.HintText(LOCTEXT("BinaryPathLabel", "Path to the Plastic SCM binary"))
					.OnTextCommitted(this, &SPlasticSourceControlSettings::OnBinaryPathTextCommited)
					.Font(Font)
				]
			]
			// Root of the workspace
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				.ToolTipText(LOCTEXT("WorkspaceRootLabel_Tooltip", "Path to the root of the Plastic SCM workspace"))
				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("WorkspaceRootLabel", "Root of the workspace"))
					.Font(Font)
				]
				+SHorizontalBox::Slot()
				.FillWidth(2.0f)
				[
					SNew(STextBlock)
					.Text(this, &SPlasticSourceControlSettings::GetPathToWorkspaceRoot)
					.Font(Font)
				]
			]
			// User Name
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				.ToolTipText(LOCTEXT("PlasticUserName_Tooltip", "User name configured for the Plastic SCM workspace"))
				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("PlasticUserName", "User Name"))
					.Font(Font)
				]
				+SHorizontalBox::Slot()
				.FillWidth(2.0f)
				[
					SNew(STextBlock)
					.Text(this, &SPlasticSourceControlSettings::GetUserName)
					.Font(Font)
				]
			]
			// Separator
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SSeparator)
			]
			// Explanation text
			+SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(2.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				.Visibility(this, &SPlasticSourceControlSettings::CanInitializePlasticWorkspace)
				.ToolTipText(LOCTEXT("WorkspaceNotFound_Tooltip", "No Workspace found at the level or above the current Project"))
				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("WorkspaceNotFound", "Current Project is not contained in a Plastic SCM Workspace. Fill the form below to initialize a new Workspace."))
					.Font(Font)
				]
			]
			// Workspace and Repository Name
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				.Visibility(this, &SPlasticSourceControlSettings::CanInitializePlasticWorkspace)
				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("WorkspaceRepositoryName", "Workspace and Repository Name"))
					.ToolTipText(LOCTEXT("WorkspaceRepositoryName_Tooltip", "Enter the Name of the new Workspace and Repository to create"))
					.Font(Font)
				]
				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(2.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SEditableTextBox)
					.Text(this, &SPlasticSourceControlSettings::GetWorkspaceName)
					.ToolTipText(LOCTEXT("WorkspaceName_Tooltip", "Enter the Name of the new Workspace to create"))
					.HintText(LOCTEXT("WorkspaceName_Hint", "Name of the Workspace to create"))
					.OnTextCommitted(this, &SPlasticSourceControlSettings::OnWorkspaceNameCommited)
					.Font(Font)
				]
				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(2.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SEditableTextBox)
					.Text(this, &SPlasticSourceControlSettings::GetRepositoryName)
					.ToolTipText(LOCTEXT("RepositoryName_Tooltip", "Enter the Name of the new Repository to use or create"))
					.HintText(LOCTEXT("RepositoryName_Hint", "Name of the Repository to use or create"))
					.OnTextCommitted(this, &SPlasticSourceControlSettings::OnRepositoryNameCommited)
					.Font(Font)
				]
			]
			// Server URL address:port
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				.Visibility(this, &SPlasticSourceControlSettings::CanInitializePlasticWorkspace)
				.ToolTipText(LOCTEXT("ServerUrl_Tooltip", "Enter the Server URL in the form address:port (localhost:8087)"))
				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ServerUrl", "Server URL address:port"))
					.Font(Font)
				]
				+SHorizontalBox::Slot()
				.FillWidth(2.0f)
				[
					SNew(SEditableTextBox)
					.Text(this, &SPlasticSourceControlSettings::GetServerUrl)
					.HintText(LOCTEXT("ServerUrl", "Enter the Server URL"))
					.OnTextCommitted(this, &SPlasticSourceControlSettings::OnServerUrlCommited)
					.Font(Font)
				]
			]
			// Option to add a 'ignore.conf' file at Workspace creation time
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				.Visibility(this, &SPlasticSourceControlSettings::CanInitializePlasticWorkspace)
				.ToolTipText(LOCTEXT("CreateIgnoreFile_Tooltip", "Create and add a standard 'ignore.conf' file"))
				+SHorizontalBox::Slot()
				.FillWidth(0.1f)
				[
					SNew(SCheckBox)
					.IsChecked(ECheckBoxState::Checked)
					.OnCheckStateChanged(this, &SPlasticSourceControlSettings::OnCheckedCreateIgnoreFile)
				]
				+SHorizontalBox::Slot()
				.FillWidth(2.9f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("CreateIgnoreFile", "Add a ignore.conf file"))
					.Font(Font)
				]
			]
			// Option to Make the initial Plastic SCM checkin
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				.Visibility(this, &SPlasticSourceControlSettings::CanInitializePlasticWorkspace)
				.ToolTipText(LOCTEXT("InitialCommit_Tooltip", "Make the initial Plastic SCM checkin"))
				+SHorizontalBox::Slot()
				.FillWidth(0.1f)
				[
					SNew(SCheckBox)
					.IsChecked(ECheckBoxState::Checked)
					.OnCheckStateChanged(this, &SPlasticSourceControlSettings::OnCheckedInitialCommit)
				]
				+SHorizontalBox::Slot()
				.FillWidth(0.9f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("InitialCommit", "Make the initial Plastic SCM Checkin"))
					.Font(Font)
				]
				+SHorizontalBox::Slot()
				.FillWidth(2.0f)
				.Padding(2.0f)
				[
					SNew(SMultiLineEditableTextBox)
					.Text(this, &SPlasticSourceControlSettings::GetInitialCommitMessage)
					.HintText(LOCTEXT("InitialCommitMessage_Hint", "Message for the initial checkin"))
					.OnTextCommitted(this, &SPlasticSourceControlSettings::OnInitialCommitMessageCommited)
					.Font(Font)
				]
			]
			// Button to create a new Workspace
			+SVerticalBox::Slot()
			.FillHeight(2.5f)
			.Padding(4.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				.Visibility(this, &SPlasticSourceControlSettings::CanInitializePlasticWorkspace)
				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SButton)
					.IsEnabled(this, &SPlasticSourceControlSettings::IsReadyToInitializePlasticWorkspace)
					.Text(LOCTEXT("PlasticInitWorkspace", "Create a new Plastic SCM workspace for the current project"))
					.ToolTipText(LOCTEXT("PlasticInitWorkspace_Tooltip", "Create and initialize a new Plastic SCM workspace and repository for the current project"))
					.OnClicked(this, &SPlasticSourceControlSettings::OnClickedInitializePlasticWorkspace)
					.HAlign(HAlign_Center)
					.ContentPadding(6)
				]
			]
			// Button to add a 'ignore.conf' file on an existing Workspace
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
					SNew(SButton)
					.Text(LOCTEXT("CreateIgnoreFile", "Add a ignore.conf file"))
					.ToolTipText(LOCTEXT("CreateIgnoreFile_Tooltip", "Create and add a standard 'ignore.conf' file"))
					.OnClicked(this, &SPlasticSourceControlSettings::OnClickedAddIgnoreFile)
					.HAlign(HAlign_Center)
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
	const bool bChanged = PlasticSourceControl.AccessSettings().SetBinaryPath(InText.ToString());
	if(bChanged)
	{
		// Re-Check provided Plastic binary path for each change
		PlasticSourceControl.GetProvider().CheckPlasticAvailability();
		if (PlasticSourceControl.GetProvider().IsPlasticAvailable())
		{
			PlasticSourceControl.SaveSettings();
		}
	}
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
	// RepositoryName and ServerUrl should also be filled
	const bool bRepositoryNameOk = !RepositoryName.IsEmpty() && !ServerUrl.IsEmpty();
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
	UE_LOG(LogSourceControl, Log, TEXT("InitializePlasticWorkspace(%s, %s, %s) CreateIgnore=%d Commit=%d"),
		*WorkspaceName.ToString(), *RepositoryName.ToString(), *ServerUrl.ToString(), bAutoCreateIgnoreFile, bAutoInitialCommit);

	// 1.a. Create a repository (if not already existing) and a workspace: launch an asynchronous MakeWorkspace operation
	LaunchMakeWorkspaceOperation();

	return FReply::Handled();
}


/// 1. Create a repository (if not already existing) and a workspace
void SPlasticSourceControlSettings::LaunchMakeWorkspaceOperation()
{
	TSharedRef<FPlasticMakeWorkspace, ESPMode::ThreadSafe> MakeWorkspaceOperation = ISourceControlOperation::Create<FPlasticMakeWorkspace>();
	MakeWorkspaceOperation->WorkspaceName = WorkspaceName.ToString();
	MakeWorkspaceOperation->RepositoryName = RepositoryName.ToString();
	MakeWorkspaceOperation->ServerUrl = ServerUrl.ToString();

	FPlasticSourceControlModule& PlasticSourceControl = FModuleManager::LoadModuleChecked<FPlasticSourceControlModule>("PlasticSourceControl");
	ECommandResult::Type Result = PlasticSourceControl.GetProvider().Execute(MakeWorkspaceOperation, TArray<FString>(), EConcurrency::Asynchronous, FSourceControlOperationComplete::CreateSP(this, &SPlasticSourceControlSettings::OnSourceControlOperationComplete));
	if (Result == ECommandResult::Succeeded)
	{
		DisplayInProgressNotification(MakeWorkspaceOperation);
	}
	else
	{
		DisplayFailureNotification(MakeWorkspaceOperation->GetName());
	}
}

/// 2. Add all project files to to Source Control (.uproject, Config/, Content/, Source/ files and ignore.conf if any)
void SPlasticSourceControlSettings::LaunchMarkForAddOperation()
{
	TSharedRef<FMarkForAdd, ESPMode::ThreadSafe> MarkForAddOperation = ISourceControlOperation::Create<FMarkForAdd>();
	FPlasticSourceControlModule& PlasticSourceControl = FModuleManager::LoadModuleChecked<FPlasticSourceControlModule>("PlasticSourceControl");

	// 1.b. Check the new workspace status to enable connection
	PlasticSourceControl.GetProvider().CheckPlasticAvailability();

	if (PlasticSourceControl.GetProvider().IsWorkspaceFound())
	{
		if (bAutoCreateIgnoreFile)
		{
			// 1.c Create a standard "ignore.conf" file with common patterns for a typical Blueprint & C++ project
			CreateIgnoreFile();
		}
		// 2. Add all project files to to Source Control (.uproject, Config/, Content/, Source/ files and ignore.conf if any)
		const TArray<FString> ProjectFiles = GetProjectFiles();
		ECommandResult::Type Result = PlasticSourceControl.GetProvider().Execute(MarkForAddOperation, ProjectFiles, EConcurrency::Asynchronous, FSourceControlOperationComplete::CreateSP(this, &SPlasticSourceControlSettings::OnSourceControlOperationComplete));
		if (Result == ECommandResult::Succeeded)
		{
			DisplayInProgressNotification(MarkForAddOperation);
		}
		else
		{
			DisplayFailureNotification(MarkForAddOperation->GetName());
		}
	}
	else
	{
		DisplayFailureNotification(MarkForAddOperation->GetName());
	}
}

/// 3. Launch an asynchronous "CheckIn" operation and start another ongoing notification
void SPlasticSourceControlSettings::LaunchCheckInOperation()
{
	TSharedRef<FCheckIn, ESPMode::ThreadSafe> CheckInOperation = ISourceControlOperation::Create<FCheckIn>();
	CheckInOperation->SetDescription(InitialCommitMessage);
	FPlasticSourceControlModule& PlasticSourceControl = FModuleManager::LoadModuleChecked<FPlasticSourceControlModule>("PlasticSourceControl");
	const TArray<FString> ProjectFiles = GetProjectFiles();
	ECommandResult::Type Result = PlasticSourceControl.GetProvider().Execute(CheckInOperation, ProjectFiles, EConcurrency::Asynchronous, FSourceControlOperationComplete::CreateSP(this, &SPlasticSourceControlSettings::OnSourceControlOperationComplete));
	if (Result == ECommandResult::Succeeded)
	{
		DisplayInProgressNotification(CheckInOperation);
	}
	else
	{
		DisplayFailureNotification(CheckInOperation->GetName());
	}
}

/// Delegate called when a source control operation has completed: launch the next one and manage notifications
void SPlasticSourceControlSettings::OnSourceControlOperationComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult)
{
	RemoveInProgressNotification();

	// Report result with a notification
	if (InResult == ECommandResult::Succeeded)
	{
		DisplaySuccessNotification(InOperation->GetName());
	}
	else
	{
		DisplayFailureNotification(InOperation->GetName());
	}

	// Launch the following asynchrounous operation
	if ((InOperation->GetName() == "MakeWorkspace") && (InResult == ECommandResult::Succeeded) && bAutoInitialCommit)
	{
		// 2. Add .uproject, Config/, Content/ and Source/ files (and ignore.conf if any)
		LaunchMarkForAddOperation();
	}
	else if ((InOperation->GetName() == "MarkForAdd") && (InResult == ECommandResult::Succeeded) && bAutoInitialCommit)
	{
		// 3. optional initial Asynchronous commit with custom message: launch a "CheckIn" Operation
		LaunchCheckInOperation();
	}
}

// Display an ongoing notification during the whole operation
void SPlasticSourceControlSettings::DisplayInProgressNotification(const FSourceControlOperationRef& InOperation)
{
	FNotificationInfo Info(InOperation->GetInProgressString());
	Info.bFireAndForget = false;
	Info.ExpireDuration = 0.0f;
	Info.FadeOutDuration = 1.0f;
	OperationInProgressNotification = FSlateNotificationManager::Get().AddNotification(Info);
	if (OperationInProgressNotification.IsValid())
	{
		OperationInProgressNotification.Pin()->SetCompletionState(SNotificationItem::CS_Pending);
	}
}

// Remove the ongoing notification at the end of the operation
void SPlasticSourceControlSettings::RemoveInProgressNotification()
{
	if (OperationInProgressNotification.IsValid())
	{
		OperationInProgressNotification.Pin()->ExpireAndFadeout();
		OperationInProgressNotification.Reset();
	}
}

// Display a temporary success notification at the end of the operation
void SPlasticSourceControlSettings::DisplaySuccessNotification(const FName& InOperationName)
{
	const FText NotificationText = FText::Format(LOCTEXT("InitWorkspace_Success", "{0} operation was successfull!"), FText::FromName(InOperationName));
	FNotificationInfo Info(NotificationText);
	Info.bUseSuccessFailIcons = true;
	Info.Image = FEditorStyle::GetBrush(TEXT("NotificationList.SuccessImage"));
	FSlateNotificationManager::Get().AddNotification(Info);
	// @todo temporary debug log
	UE_LOG(LogSourceControl, Log, TEXT("%s"), *NotificationText.ToString());
}

// Display a temporary failure notification at the end of the operation
void SPlasticSourceControlSettings::DisplayFailureNotification(const FName& InOperationName)
{
	const FText NotificationText = FText::Format(LOCTEXT("InitWorkspace_Failure", "Error: {0} operation failed!"), FText::FromName(InOperationName));
	FNotificationInfo Info(NotificationText);
	Info.ExpireDuration = 8.0f;
	FSlateNotificationManager::Get().AddNotification(Info);
	// @todo temporary debug log
	UE_LOG(LogSourceControl, Error, TEXT("%s"), *NotificationText.ToString());
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
		PlasticSourceControlUtils::RunCommand(TEXT("add"), Parameters, Files, EConcurrency::Synchronous, InfoMessages, ErrorMessages);
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

/** List of files to add to Source Control (.uproject, Config/, Content/, Source/ files and ignore.conf if any) */
TArray<FString> SPlasticSourceControlSettings::GetProjectFiles() const
{
	TArray<FString> ProjectFiles;
	ProjectFiles.Add(FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath()));
	ProjectFiles.Add(FPaths::ConvertRelativePathToFull(FPaths::GameConfigDir()));
	ProjectFiles.Add(FPaths::ConvertRelativePathToFull(FPaths::GameContentDir()));
	if (FPaths::DirectoryExists(FPaths::GameSourceDir()))
	{
		ProjectFiles.Add(FPaths::ConvertRelativePathToFull(FPaths::GameSourceDir()));
	}
	if (bAutoCreateIgnoreFile)
	{
		ProjectFiles.Add(GetIgnoreFileName());
	}
	return ProjectFiles;
}

#undef LOCTEXT_NAMESPACE
