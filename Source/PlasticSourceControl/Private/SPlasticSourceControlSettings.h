// Copyright (c) 2016 Codice Software - Sebastien Rombauts (sebastien.rombauts@gmail.com)

#pragma once

class SPlasticSourceControlSettings : public SCompoundWidget
{
public:
	
	SLATE_BEGIN_ARGS(SPlasticSourceControlSettings) {}
	
	SLATE_END_ARGS()

public:

	void Construct(const FArguments& InArgs);

private:

	/** Delegate to get cm binary path from settings */
	FText GetBinaryPathText() const;

	/** Delegate to commit cm binary path to settings */
	void OnBinaryPathTextCommited(const FText& InText, ETextCommit::Type InCommitType) const;
};