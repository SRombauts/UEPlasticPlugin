// Copyright (c) 2023 Unity Technologies

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SCompoundWidget.h"

class FReply;
class SPlasticSourceControlBranchesWidget;
class SWindow;

class SPlasticSourceControlCreateBranch : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPlasticSourceControlCreateBranch)
		: _BranchesWidget()
		, _ParentWindow()
		, _ParentBranchName()
	{}

		SLATE_ARGUMENT(TSharedPtr<SPlasticSourceControlBranchesWidget>, BranchesWidget)
		SLATE_ARGUMENT(TSharedPtr<SWindow>, ParentWindow)
		SLATE_ARGUMENT(FString, ParentBranchName)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	void OnCheckedSwitchWorkspace(ECheckBoxState InState)
	{
		bSwitchWorkspace = (InState == ECheckBoxState::Checked);
	}

	FReply CreateClicked();
	FReply CancelClicked();

private:
	FString ParentBranchName;
	FString NewBranchName;
	FString NewBranchComment;
	bool bSwitchWorkspace = true;

	TWeakPtr<SPlasticSourceControlBranchesWidget> BranchesWidget;
	TWeakPtr<SWindow> ParentWindow;
};
