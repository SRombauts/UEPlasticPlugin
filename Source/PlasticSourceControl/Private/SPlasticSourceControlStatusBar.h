// Copyright (c) 2024 Unity Technologies

#pragma once

#include "CoreMinimal.h"

#include "Widgets/SCompoundWidget.h"

class FReply;

/**
 * Status bar displaying the name of the current branch
 */
class SPlasticSourceControlStatusBar : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPlasticSourceControlStatusBar)	{}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FText GetStatusBarText() const;

	FReply OnClicked();
};
