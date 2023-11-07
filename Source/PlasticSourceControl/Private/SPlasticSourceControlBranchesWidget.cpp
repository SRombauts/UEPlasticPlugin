// Copyright (c) 2023 Unity Technologies

#include "SPlasticSourceControlBranchesWidget.h"

#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PlasticSourceControlWindow"

// TODO: transition to a proper SMultiColumnTableRow
// see Engine\Source\Editor\SourceControlWindows\Private\SSourceControlChangelistRows.h
// class SFileTableRow : public SMultiColumnTableRow<FChangelistTreeItemPtr>

void SPlasticSourceControlBranchesWidget::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Icons.Search"))
			]
			+SHorizontalBox::Slot()
			.MaxWidth(300)
			[
				SNew(SEditableTextBox)
				.Justification(ETextJustify::Left)
				.HintText(LOCTEXT("Search", "Search"))
				.OnTextChanged(this, &SPlasticSourceControlBranchesWidget::OnSearchTextChanged)
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 5, 0, 0)
		.Expose(ContentSlot)
		[
			CreateContentPanel()
		]
	];

	RegisterActiveTimer(60.f, FWidgetActiveTimerDelegate::CreateSP(this, &SPlasticSourceControlBranchesWidget::UpdateContentSlot));
}

TSharedRef<SWidget> SPlasticSourceControlBranchesWidget::CreateContentPanel()
{
	// TODO: transition to a proper ListView widget
	TSharedRef<SGridPanel> Panel =
		SNew(SGridPanel);

	const float RowMargin = 0.0f;
	const float ColumnMargin = 10.0f;
	const FSlateColor TitleColor = FStyleColors::AccentWhite;
	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 10);
	const FMargin DefaultMarginFirstColumn(ColumnMargin, RowMargin);

	int32 Row = 0;

	const FMargin TitleMargin(0.0f, 10.0f, ColumnMargin, 10.0f);
	const FMargin TitleMarginFirstColumn(ColumnMargin, 10.0f);
	const FMargin DefaultMargin(0.0f, RowMargin, ColumnMargin, RowMargin);

	Panel->AddSlot(0, Row)
	[
		SNew(STextBlock)
	];

	Panel->AddSlot(1, Row)
	[
		SNew(STextBlock)
		.Margin(TitleMarginFirstColumn)
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
		.ColorAndOpacity(TitleColor)
		.Text(LOCTEXT("BranchName", "Name"))
	];

	Panel->AddSlot(2, Row)
	[
		SNew(STextBlock)
		.Margin(TitleMargin)
		.ColorAndOpacity(TitleColor)
		.Font(TitleFont)
		.Text(LOCTEXT("CreateBy", "Created By"))
	];

	Panel->AddSlot(3, Row)
	[
		SNew(STextBlock)
		.Margin(TitleMargin)
		.ColorAndOpacity(TitleColor)
		.Font(TitleFont)
		.Text(LOCTEXT("CreationDate", "Creation date"))
	];

	Panel->AddSlot(4, Row)
	[
		SNew(STextBlock)
		.Margin(TitleMargin)
		.ColorAndOpacity(TitleColor)
		.Font(TitleFont)
		.Text(LOCTEXT("Comment", "Comment"))
	];


	// POC
	for (int32 i = 0; i < 10; i++)
	{
		// TODO: POC fake branch info
		const FString BranchName = (i == 0) ? FString(TEXT("/main")) : FString::Printf(TEXT("/main/scm%d"), 100271 + (i * i));
		const FString BranchCreatedBy = TEXT("sebastien.rombauts@unity3d.com");
		const FString BranchCreationDate = TEXT("23/10/2023 14:24:14");
		const FString BranchComment = FString::Printf(TEXT("Proof of Concept comment for branch %s"), *BranchName);

		// Filter branches on name, author and comment
		if ((BranchName.Find(FilterText) == INDEX_NONE) && (BranchCreatedBy.Find(FilterText) == INDEX_NONE) && (BranchComment.Find(FilterText) == INDEX_NONE))
		{
			continue;
		}

		Row++;

		Panel->AddSlot(0, Row)
		[
			SNew(STextBlock)
		];

		Panel->AddSlot(1, Row)
		.HAlign(HAlign_Left)
		[
			SNew(STextBlock)
			.Margin(DefaultMargin)
			.Text(FText::FromString(BranchName))
		];

		Panel->AddSlot(2, Row)
		.HAlign(HAlign_Left)
		[
			SNew(STextBlock)
			.Margin(DefaultMargin)
			.Text(FText::FromString(BranchCreatedBy))
		];

		Panel->AddSlot(3, Row)
		.HAlign(HAlign_Left)
		[
			SNew(STextBlock)
			.Margin(DefaultMargin)
			.Text(FText::FromString(BranchCreationDate))
		];

		Panel->AddSlot(4, Row)
		.HAlign(HAlign_Left)
		[
			SNew(STextBlock)
			.Margin(DefaultMargin)
			.Text(FText::FromString(BranchComment))
		];
	}

	return Panel;
}

EActiveTimerReturnType SPlasticSourceControlBranchesWidget::UpdateContentSlot(double InCurrentTime, float InDeltaTime)
{
	(*ContentSlot)
		[
			CreateContentPanel()
		];

	SlatePrepass(GetPrepassLayoutScaleMultiplier());

	return EActiveTimerReturnType::Continue;
}

void SPlasticSourceControlBranchesWidget::OnSearchTextChanged(const FText& SearchText)
{
	FilterText = SearchText.ToString();

	UpdateContentSlot(0.0, 0.0f);
}

#undef LOCTEXT_NAMESPACE
