// Copyright (c) 2023 Unity Technologies

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

// Widget displaying the list of branches in the tab window, see FPlasticSourceControlBranchesWindow
class SPlasticSourceControlBranchesWidget : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SPlasticSourceControlBranchesWidget) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedRef<SWidget> CreateContentPanel();
	EActiveTimerReturnType UpdateContentSlot(double InCurrentTime, float InDeltaTime);
	SVerticalBox::FSlot* ContentSlot = nullptr;

	FString FilterText;
	void OnSearchTextChanged(const FText& SearchText);
};
