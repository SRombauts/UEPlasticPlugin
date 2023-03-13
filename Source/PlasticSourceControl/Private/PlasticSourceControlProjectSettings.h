// Copyright Unity Technologies

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "PlasticSourceControlProjectSettings.generated.h"

/** Project Settings for Unity Version Control. Saved in Config/DefaultEditor.ini */
UCLASS(config = Editor, defaultconfig, meta = (DisplayName = "Source Control - Unity Version Control"))
class UPlasticSourceControlProjectSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Map Unity Version Control user names (typically e-mail addresses or company domain names) to display names for brevity. */
	UPROPERTY(config, EditAnywhere, Category = "Unity Version Control")
	TMap<FString, FString> UserNameToDisplayName;

	/** Hide the domain part of an username e-mail address (eg @gmail.com) if the UserNameToDisplayName map didn't match (enabled by default). */
	UPROPERTY(config, EditAnywhere, Category = "Unity Version Control")
	bool bHideEmailDomainInUsername = true;

	/** If enabled, you'll be prompted to check out changed files (enabled by default). Checkout is needed to work with Changelists. */
	UPROPERTY(config, EditAnywhere, Category = "Unity Version Control")
	bool bPromptForCheckoutOnChange = true;

	/** If a non-null value is set, limit the maximum number of revisions requested to Unity Version Control to display in the "History" window. */
	UPROPERTY(config, EditAnywhere, Category = "Unity Version Control", meta=(ClampMin=0))
	int32 LimitNumberOfRevisionsInHistory = 50;
};
