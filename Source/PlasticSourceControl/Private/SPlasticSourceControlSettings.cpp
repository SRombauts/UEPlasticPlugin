// Copyright Unity Technologies

#include "SPlasticSourceControlSettings.h"
#include "PlasticSourceControlOperations.h"
#include "PlasticSourceControlModule.h"
#include "PlasticSourceControlUtils.h"

#include "SourceControlOperations.h"
#include "ISourceControlModule.h"
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
#include "Runtime/Launch/Resources/Version.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "Styling/AppStyle.h"
#else
#include "EditorStyleSet.h"
#endif

#define LOCTEXT_NAMESPACE "SPlasticSourceControlSettings"

void SPlasticSourceControlSettings::Construct(const FArguments& InArgs)
{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
	const FSlateFontInfo Font = FAppStyle::GetFontStyle(TEXT("SourceControl.LoginWindow.Font"));
#else
	const FSlateFontInfo Font = FEditorStyle::GetFontStyle(TEXT("SourceControl.LoginWindow.Font"));
#endif

	bAutoCreateIgnoreFile = CanAutoCreateIgnoreFile();
	bAutoInitialCommit = true;

	InitialCommitMessage = LOCTEXT("InitialCommitMessage", "Initial checkin");
	ServerUrl = FText::FromString(TEXT("YourOrganization@cloud"));
	if (FApp::HasProjectName())
	{
		WorkspaceName = FText::FromString(FApp::GetProjectName());
		RepositoryName = WorkspaceName;
	}

	ChildSlot
	[
#if ENGINE_MAJOR_VERSION == 4
	SNew(SBorder)
	.BorderImage(FEditorStyle::GetBrush("DetailsView.CategoryBottom"))
	.Padding(FMargin(0.0f, 3.0f, 0.0f, 0.0f))
	[
#endif
		SNew(SVerticalBox)
		// Versions (Plugin & Plastic SCM) useful eg to help diagnose issues from screenshots
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)
			.ToolTipText(LOCTEXT("PlasticVersions_Tooltip", "Plastic SCM and Plugin versions"))
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("PlasticVersions", "Plastic SCM version"))
				.Font(Font)
			]
			+SHorizontalBox::Slot()
			.FillWidth(2.0f)
			[
				SNew(STextBlock)
				.Text(this, &SPlasticSourceControlSettings::GetVersions)
				.Font(Font)
			]
		]
		// Plastic SCM command line tool not available warning
		+SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(2.0f)
		[
			SNew(STextBlock)
			.Visibility(this, &SPlasticSourceControlSettings::PlasticNotAvailable)
			.ToolTipText(LOCTEXT("PlasticNotAvailable_Tooltip", "Failed to launch Plastic SCM 'cm' command line tool. You need to install it and make sure that 'cm' is on the Path and correctly configured."))
			.Text(LOCTEXT("PlasticNotAvailable", "Plastic SCM Command Line tool 'cm' failed to start:"))
			.Font(Font)
		]
		// Path to the Plastic SCM binary
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)
			.ToolTipText(LOCTEXT("BinaryPathLabel_Tooltip", "Path to the Plastic SCM Command Line tool 'cm' binary"))
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("PathLabel", "Plastic SCM path to cm"))
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
			.ToolTipText(this, &SPlasticSourceControlSettings::GetPathToWorkspaceRoot)
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
		.Padding(2.0f, 5.0f)
		[
			SNew(STextBlock)
			.Visibility(this, &SPlasticSourceControlSettings::CanInitializePlasticWorkspace)
			.ToolTipText(LOCTEXT("WorkspaceNotFound_Tooltip", "No Workspace found at the level or above the current Project. Use the form to create a new one."))
			.Text(LOCTEXT("WorkspaceNotFound", "Current Project is not in a Plastic SCM Workspace. Create a new one:"))
			.Font(Font)
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
				.Text(LOCTEXT("WorkspaceRepositoryName", "Workspace & Repository"))
				.ToolTipText(LOCTEXT("WorkspaceRepositoryName_Tooltip", "Enter the Name of the new Workspace and Repository to create or use"))
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
				.ToolTipText(LOCTEXT("RepositoryName_Tooltip", "Enter the Name of the Repository to use or create"))
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
			.ToolTipText(LOCTEXT("ServerUrl_Tooltip", "Enter the Server URL in the form address:port (eg. YourOrganization@cloud, local, or something like ip:port, eg localhost:8087)"))
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
				.HintText(LOCTEXT("EnterServerUrl", "Enter the Server URL"))
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
			SNew(SCheckBox)
			.Visibility(this, &SPlasticSourceControlSettings::CanInitializePlasticWorkspace)
			.ToolTipText(LOCTEXT("CreateIgnoreFile_Tooltip", "Create and add a standard 'ignore.conf' file"))
			.IsEnabled(this, &SPlasticSourceControlSettings::CanAutoCreateIgnoreFile)
			.IsChecked(bAutoCreateIgnoreFile)
			.OnCheckStateChanged(this, &SPlasticSourceControlSettings::OnCheckedCreateIgnoreFile)
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
			.FillWidth(0.7f)
			[
				SNew(SCheckBox)
				.IsChecked(ECheckBoxState::Checked)
				.OnCheckStateChanged(this, &SPlasticSourceControlSettings::OnCheckedInitialCommit)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("InitialCommit", "Initial Checkin"))
					.Font(Font)
				]
			]
			+SHorizontalBox::Slot()
			.FillWidth(1.4f)
			.Padding(2.0f)
			[
				SNew(SMultiLineEditableTextBox)
				.Text(this, &SPlasticSourceControlSettings::GetInitialCommitMessage)
				.HintText(LOCTEXT("InitialCommitMessage_Hint", "Message for the initial checkin"))
				.OnTextCommitted(this, &SPlasticSourceControlSettings::OnInitialCommitMessageCommited)
				.Font(Font)
			]
		]
		// Option to run an Update Status operation at Editor Startup
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SCheckBox)
			.IsChecked(SPlasticSourceControlSettings::IsUpdateStatusAtStartupChecked())
			.OnCheckStateChanged(this, &SPlasticSourceControlSettings::OnCheckedUpdateStatusAtStartup)
			.ToolTipText(LOCTEXT("UpdateStatusAtStartup_Tooltip", "Run an asynchronous Update Status at Editor startup (can be slow)."))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("UpdateStatusAtStartup", "Update workspace Status at Editor startup"))
				.Font(Font)
			]
		]
		// Option to call History as part of Update Status operation to check for potential recent changes in other branches
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SCheckBox)
			.ToolTipText(LOCTEXT("UpdateStatusOtherBranches_Tooltip", "Enable Update status to detect more recent changes on other branches in order to display warnings (can be slow)."))
			.IsChecked(SPlasticSourceControlSettings::IsUpdateStatusOtherBranchesChecked())
			.OnCheckStateChanged(this, &SPlasticSourceControlSettings::OnCheckedUpdateStatusOtherBranches)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("UpdateStatusOtherBranches", "Update Status also checks history to detect changes on other branches."))
				.Font(Font)
			]
		]
		// Option to enable Source Control Verbose logs
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SCheckBox)
			.ToolTipText(LOCTEXT("EnableVerboseLogs_Tooltip", "Override LogSourceControl default verbosity level to Verbose (except if already set to VeryVerbose)."))
			.IsChecked(SPlasticSourceControlSettings::IsEnableVerboseLogsChecked())
			.OnCheckStateChanged(this, &SPlasticSourceControlSettings::OnCheckedEnableVerboseLogs)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("EnableVerboseLogs", "Enable Source Control Verbose logs"))
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
#if ENGINE_MAJOR_VERSION == 4
	]
#endif
	];
}

EVisibility SPlasticSourceControlSettings::PlasticNotAvailable() const
{
	const FPlasticSourceControlProvider& Provider = FPlasticSourceControlModule::Get().GetProvider();
	return Provider.IsPlasticAvailable() ? EVisibility::Collapsed : EVisibility::Visible;
}

FText SPlasticSourceControlSettings::GetBinaryPathText() const
{
	return FText::FromString(FPlasticSourceControlModule::Get().GetProvider().AccessSettings().GetBinaryPath());
}

void SPlasticSourceControlSettings::OnBinaryPathTextCommited(const FText& InText, ETextCommit::Type InCommitType) const
{
	FPlasticSourceControlProvider& Provider = FPlasticSourceControlModule::Get().GetProvider();
	const bool bChanged = Provider.AccessSettings().SetBinaryPath(InText.ToString());
	if (bChanged)
	{
		// Re-Check provided Plastic binary path for each change
		Provider.CheckPlasticAvailability();
		if (Provider.IsPlasticAvailable())
		{
			Provider.AccessSettings().SaveSettings();
		}
	}
}

FText SPlasticSourceControlSettings::GetVersions() const
{
	const FPlasticSourceControlProvider& Provider = FPlasticSourceControlModule::Get().GetProvider();
	return FText::FromString(Provider.GetPlasticScmVersion().String + TEXT(" (plugin v") + Provider.GetPluginVersion() + TEXT(")"));
}

FText SPlasticSourceControlSettings::GetPathToWorkspaceRoot() const
{
	return FText::FromString(FPlasticSourceControlModule::Get().GetProvider().GetPathToWorkspaceRoot());
}

FText SPlasticSourceControlSettings::GetUserName() const
{
	return FText::FromString(FPlasticSourceControlModule::Get().GetProvider().GetUserName());
}


EVisibility SPlasticSourceControlSettings::CanInitializePlasticWorkspace() const
{
	const FPlasticSourceControlProvider& Provider = FPlasticSourceControlModule::Get().GetProvider();
	const bool bPlasticAvailable = Provider.IsPlasticAvailable();
	const bool bPlasticWorkspaceFound = Provider.IsWorkspaceFound();
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

bool SPlasticSourceControlSettings::CanAutoCreateIgnoreFile() const
{
	const bool bIgnoreFileFound = FPaths::FileExists(GetIgnoreFileName());
	return !bIgnoreFileFound;
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

	FPlasticSourceControlProvider& Provider = FPlasticSourceControlModule::Get().GetProvider();
	ECommandResult::Type Result = Provider.Execute(MakeWorkspaceOperation, TArray<FString>(), EConcurrency::Asynchronous, FSourceControlOperationComplete::CreateSP(this, &SPlasticSourceControlSettings::OnSourceControlOperationComplete));
	if (Result == ECommandResult::Succeeded)
	{
		DisplayInProgressNotification(MakeWorkspaceOperation->GetInProgressString());
	}
	else
	{
		DisplayFailureNotification(MakeWorkspaceOperation->GetName());
	}
}

/// 2. Add all project files to Source Control (.uproject, Config/, Content/, Source/ files and ignore.conf if any)
void SPlasticSourceControlSettings::LaunchMarkForAddOperation()
{
	TSharedRef<FMarkForAdd, ESPMode::ThreadSafe> MarkForAddOperation = ISourceControlOperation::Create<FMarkForAdd>();
	FPlasticSourceControlProvider& Provider = FPlasticSourceControlModule::Get().GetProvider();

	// 1.b. Check the new workspace status to enable connection
	Provider.CheckPlasticAvailability();

	if (Provider.IsWorkspaceFound())
	{
		if (bAutoCreateIgnoreFile)
		{
			// 1.c Create a standard "ignore.conf" file with common patterns for a typical Blueprint & C++ project
			CreateIgnoreFile();
		}
		// 2. Add all project files to Source Control (.uproject, Config/, Content/, Source/ files and ignore.conf if any)
		const TArray<FString> ProjectFiles = GetProjectFiles();
		ECommandResult::Type Result = Provider.Execute(MarkForAddOperation, ProjectFiles, EConcurrency::Asynchronous, FSourceControlOperationComplete::CreateSP(this, &SPlasticSourceControlSettings::OnSourceControlOperationComplete));
		if (Result == ECommandResult::Succeeded)
		{
			DisplayInProgressNotification(MarkForAddOperation->GetInProgressString());
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
	FPlasticSourceControlProvider& Provider = FPlasticSourceControlModule::Get().GetProvider();
	const TArray<FString> ProjectFiles = GetProjectFiles(); // Note: listing files and folders is only needed for the update status operation following the checkin to know on what to operate
	ECommandResult::Type Result = Provider.Execute(CheckInOperation, ProjectFiles, EConcurrency::Asynchronous, FSourceControlOperationComplete::CreateSP(this, &SPlasticSourceControlSettings::OnSourceControlOperationComplete));
	if (Result == ECommandResult::Succeeded)
	{
		DisplayInProgressNotification(CheckInOperation->GetInProgressString());
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

	// Launch the following asynchronous operation
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
void SPlasticSourceControlSettings::DisplayInProgressNotification(const FText& InOperationInProgressString)
{
	if (!OperationInProgressNotification.IsValid())
	{
		FNotificationInfo Info(InOperationInProgressString);
		Info.bFireAndForget = false;
		Info.ExpireDuration = 0.0f;
		Info.FadeOutDuration = 1.0f;
		OperationInProgressNotification = FSlateNotificationManager::Get().AddNotification(Info);
		if (OperationInProgressNotification.IsValid())
		{
			OperationInProgressNotification.Pin()->SetCompletionState(SNotificationItem::CS_Pending);
		}
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
	const FText NotificationText = FText::Format(LOCTEXT("InitWorkspace_Success", "{0} operation was successful!"), FText::FromName(InOperationName));
	FNotificationInfo Info(NotificationText);
	Info.bUseSuccessFailIcons = true;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
	Info.Image = FAppStyle::GetBrush(TEXT("NotificationList.SuccessImage"));
#else
	Info.Image = FEditorStyle::GetBrush(TEXT("NotificationList.SuccessImage"));
#endif
	FSlateNotificationManager::Get().AddNotification(Info);
	UE_LOG(LogSourceControl, Verbose, TEXT("%s"), *NotificationText.ToString());
}

// Display a temporary failure notification at the end of the operation
void SPlasticSourceControlSettings::DisplayFailureNotification(const FName& InOperationName)
{
	const FText NotificationText = FText::Format(LOCTEXT("InitWorkspace_Failure", "Error: {0} operation failed!"), FText::FromName(InOperationName));
	FNotificationInfo Info(NotificationText);
	Info.ExpireDuration = 8.0f;
	FSlateNotificationManager::Get().AddNotification(Info);
	UE_LOG(LogSourceControl, Error, TEXT("%s"), *NotificationText.ToString());
}

/** Delegate to check for presence of a Plastic ignore.conf file to an existing Plastic SCM workspace */
EVisibility SPlasticSourceControlSettings::CanAddIgnoreFile() const
{
	const bool bPlasticWorkspaceFound = FPlasticSourceControlModule::Get().GetProvider().IsWorkspaceFound();
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

void SPlasticSourceControlSettings::OnCheckedUpdateStatusAtStartup(ECheckBoxState NewCheckedState)
{
	FPlasticSourceControlSettings& PlasticSettings = FPlasticSourceControlModule::Get().GetProvider().AccessSettings();
	PlasticSettings.SetUpdateStatusAtStartup(NewCheckedState == ECheckBoxState::Checked);
	PlasticSettings.SaveSettings();
}

ECheckBoxState SPlasticSourceControlSettings::IsUpdateStatusAtStartupChecked() const
{
	FPlasticSourceControlSettings& PlasticSettings = FPlasticSourceControlModule::Get().GetProvider().AccessSettings();
	return PlasticSettings.GetUpdateStatusAtStartup() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SPlasticSourceControlSettings::OnCheckedUpdateStatusOtherBranches(ECheckBoxState NewCheckedState)
{
	FPlasticSourceControlSettings& PlasticSettings = FPlasticSourceControlModule::Get().GetProvider().AccessSettings();
	PlasticSettings.SetUpdateStatusOtherBranches(NewCheckedState == ECheckBoxState::Checked);
	PlasticSettings.SaveSettings();
}

ECheckBoxState SPlasticSourceControlSettings::IsUpdateStatusOtherBranchesChecked() const
{
	FPlasticSourceControlSettings& PlasticSettings = FPlasticSourceControlModule::Get().GetProvider().AccessSettings();
	return PlasticSettings.GetUpdateStatusOtherBranches() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SPlasticSourceControlSettings::OnCheckedEnableVerboseLogs(ECheckBoxState NewCheckedState)
{
	FPlasticSourceControlSettings& PlasticSettings = FPlasticSourceControlModule::Get().GetProvider().AccessSettings();
	PlasticSettings.SetEnableVerboseLogs(NewCheckedState == ECheckBoxState::Checked);
	PlasticSettings.SaveSettings();

	PlasticSourceControlUtils::SwitchVerboseLogs(NewCheckedState == ECheckBoxState::Checked);
}

ECheckBoxState SPlasticSourceControlSettings::IsEnableVerboseLogsChecked() const
{
	FPlasticSourceControlSettings& PlasticSettings = FPlasticSourceControlModule::Get().GetProvider().AccessSettings();
	return PlasticSettings.GetEnableVerboseLogs() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

/** Path to the "ignore.conf" file */
const FString SPlasticSourceControlSettings::GetIgnoreFileName() const
{
	const FString PathToWorkspaceRoot = FPlasticSourceControlModule::Get().GetProvider().GetPathToWorkspaceRoot();
	const FString IgnoreFileName = FPaths::Combine(*PathToWorkspaceRoot, TEXT("ignore.conf"));
	return IgnoreFileName;
}

/** Create a standard "ignore.conf" file with common patterns for a typical Blueprint & C++ project */
bool SPlasticSourceControlSettings::CreateIgnoreFile() const
{
	const FString IgnoreFileContent = TEXT("Binaries\nBuild\nDerivedDataCache\nIntermediate\nSaved\nScript\nenc_temp_folder\n.idea\n.vscode\n.vs\n.vsconfig\n.ignore\n*.VC.db\n*.opensdf\n*.opendb\n*.sdf\n*.sln\n*.suo\n*.code-workspace\n*.xcodeproj\n*.xcworkspace");
	return FFileHelper::SaveStringToFile(IgnoreFileContent, *GetIgnoreFileName(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

/** List of files to add to Source Control (.uproject, Config/, Content/, Source/ files and ignore.conf if any) */
TArray<FString> SPlasticSourceControlSettings::GetProjectFiles() const
{
	TArray<FString> ProjectFiles;
	ProjectFiles.Add(FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath()));
	ProjectFiles.Add(FPaths::ConvertRelativePathToFull(FPaths::ProjectConfigDir()));
	ProjectFiles.Add(FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir()));
	if (FPaths::DirectoryExists(FPaths::GameSourceDir()))
	{
		ProjectFiles.Add(FPaths::ConvertRelativePathToFull(FPaths::GameSourceDir()));
	}
	if (FPaths::FileExists(GetIgnoreFileName()))
	{
		ProjectFiles.Add(GetIgnoreFileName());
	}
	return ProjectFiles;
}

#undef LOCTEXT_NAMESPACE
