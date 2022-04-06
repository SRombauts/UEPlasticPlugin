// Copyright (c) 2016-2022 Codice Software

#include "PlasticSourceControlUtils.h"

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFindCommonDirectoryUnitTest, "PlasticSCM.FindCommonDirectory", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFindCommonDirectoryUnitTest::RunTest(const FString& Parameters)
{
	FString CommonDir;
	CommonDir = PlasticSourceControlUtils::FindCommonDirectory(TEXT(""), TEXT(""));
	TestEqual(TEXT("No common dir"), CommonDir, FString(TEXT("")));
	CommonDir = PlasticSourceControlUtils::FindCommonDirectory(TEXT(""), TEXT("/abc/"));
	TestEqual(TEXT("No common dir"), CommonDir, FString(TEXT("")));
	CommonDir = PlasticSourceControlUtils::FindCommonDirectory(TEXT("C:/"), TEXT("D:/"));
	TestEqual(TEXT("No common dir"), CommonDir, FString(TEXT("")));
	CommonDir = PlasticSourceControlUtils::FindCommonDirectory(TEXT("/ab/c/"), TEXT(""));
	TestEqual(TEXT("No common dir"), CommonDir, FString(TEXT("")));

	CommonDir = PlasticSourceControlUtils::FindCommonDirectory(TEXT("/ab/c/"), TEXT("/d/e"));
	TestEqual(TEXT("Root"), CommonDir, FString(TEXT("/")));

	CommonDir = PlasticSourceControlUtils::FindCommonDirectory(TEXT("/a/b/c"), TEXT("/a/b/d"));
	TestEqual(TEXT("Common dir"), CommonDir, FString(TEXT("/a/b/")));
	CommonDir = PlasticSourceControlUtils::FindCommonDirectory(TEXT("/a/b/ccc"), TEXT("/a/b/cde"));
	TestEqual(TEXT("Common dir"), CommonDir, FString(TEXT("/a/b/")));

	CommonDir = PlasticSourceControlUtils::FindCommonDirectory(TEXT("C:/Workspace/Content/Text"), TEXT("C:/Workspace/Content/Textures"));
	TestEqual(TEXT("Common dir"), CommonDir, FString(TEXT("C:/Workspace/Content/")));
	CommonDir = PlasticSourceControlUtils::FindCommonDirectory(TEXT("C:/Workspace/Content/Text/"), TEXT("C:/Workspace/Content/Textures/"));
	TestEqual(TEXT("Common dir"), CommonDir, FString(TEXT("C:/Workspace/Content/")));

	// shows that paths need to finish with a slash in order to be interpreted correctly as a directory
	CommonDir = PlasticSourceControlUtils::FindCommonDirectory(TEXT("C:/Workspace/Content"), TEXT("C:/Workspace/Content/Textures"));
	TestEqual(TEXT("Common dir"), CommonDir, FString(TEXT("C:/Workspace/")));

	return true; // actual results are returned by TestXxx() macros
}

#endif
