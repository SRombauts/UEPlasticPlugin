// Copyright Unity Technologies

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "PlasticSourceControlProjectSettings.generated.h"

/** Project Settings for Plastic SCM Source Control. Saved in Config/DefaultEditor.ini */
UCLASS(config = Editor, defaultconfig, meta = (DisplayName = "Source Control - Plastic SCM"))
class UPlasticSourceControlProjectSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Map Plastic SCM user names (typically e-mail addresses or company domain names) to display names for brevity. */
	UPROPERTY(config, EditAnywhere, Category = "Plastic SCM")
	TMap<FString, FString> UserNameToDisplayName;

	/** Hide the domain part of an username e-mail address (eg @gmail.com) if the UserNameToDisplayName map didn't match (enabled by default). */
	UPROPERTY(config, EditAnywhere, Category = "Plastic SCM")
	bool bHideEmailDomainInUsername = true;

	/** If enabled, you'll be prompted to check out changed files (enabled by default). Checkout is needed to work with Changelists. */
	UPROPERTY(config, EditAnywhere, Category = "Plastic SCM")
	bool bPromptForCheckoutOnChange = true;

	/** If a non-null value is set, limit the maximum number of revisions requested to Plastic SCM to display in the "History" window. */
	UPROPERTY(config, EditAnywhere, Category = "Plastic SCM", meta=(ClampMin=0))
	int32 LimitNumberOfRevisionsInHistory = 50;
};
