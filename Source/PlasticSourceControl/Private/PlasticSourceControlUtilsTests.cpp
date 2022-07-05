// Copyright (c) 2016-2022 Codice Software

#include "PlasticSourceControlUtils.h"
#include "PlasticSourceControlProvider.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoftwareVersionEqualUnitTest, "PlasticSCM.SoftwareVersionEqual", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSoftwareVersionEqualUnitTest::RunTest(const FString& Parameters)
{
	FSoftwareVersion VersionParse(TEXT("1.2.3.4"));
	FSoftwareVersion VersionSplit(1, 2, 3, 4);

	TestTrue(TEXT("Equal"), VersionParse == VersionSplit);

	return true; // actual results are returned by TestXxx() macros
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoftwareVersionLessUnitTest, "PlasticSCM.SoftwareVersionLess", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSoftwareVersionLessUnitTest::RunTest(const FString& Parameters)
{
	FSoftwareVersion VersionZero(TEXT("0.0.0.0"));
	FSoftwareVersion VersionTen(TEXT("10.1.19.9999"));
	FSoftwareVersion VersionEleven0(TEXT("11.0.15.13"));
	FSoftwareVersion VersionEleven1(TEXT("11.0.16.13"));
	FSoftwareVersion VersionEleven2(TEXT("11.0.16.123"));
	FSoftwareVersion VersionEleven3(TEXT("11.0.16.1111"));
	FSoftwareVersion VersionEleven4(TEXT("11.0.16.7134"));
	FSoftwareVersion VersionEleven5(TEXT("11.0.16.9999"));
	FSoftwareVersion VersionEleven6(TEXT("11.1.0.0"));
	FSoftwareVersion VersionTwelve(TEXT("12.0.10.0"));

	TestFalse(TEXT("No difference"), VersionZero < VersionZero);
	TestFalse(TEXT("No difference"), VersionTen < VersionTen);
	TestFalse(TEXT("No difference"), VersionEleven4 < VersionEleven4);

	TestTrue(TEXT("Major difference"), VersionZero < VersionTen);
	TestTrue(TEXT("Major difference"), VersionTen < VersionEleven1);
	TestTrue(TEXT("Major difference"), VersionEleven5 < VersionTwelve);
	TestFalse(TEXT("Major difference"), VersionTen < VersionZero);
	TestFalse(TEXT("Major difference"), VersionEleven1 < VersionTen);
	TestFalse(TEXT("Major difference"), VersionTwelve < VersionEleven5);

	TestTrue(TEXT("Minor difference"), VersionEleven5 < VersionEleven6);
	TestFalse(TEXT("Minor difference"), VersionEleven6 < VersionEleven5);

	TestTrue(TEXT("Patch difference"), VersionEleven0 < VersionEleven1);
	TestFalse(TEXT("Patch difference"), VersionEleven1 < VersionEleven0);

	TestTrue(TEXT("Changeset difference"), VersionEleven1 < VersionEleven2);
	TestTrue(TEXT("Changeset difference"), VersionEleven2 < VersionEleven3);
	TestTrue(TEXT("Changeset difference"), VersionEleven3 < VersionEleven4);
	TestTrue(TEXT("Changeset difference"), VersionEleven4 < VersionEleven5);
	TestTrue(TEXT("Changeset difference"), VersionEleven5 < VersionEleven6);
	TestFalse(TEXT("Changeset difference"), VersionEleven2 < VersionEleven1);
	TestFalse(TEXT("Changeset difference"), VersionEleven3 < VersionEleven2);
	TestFalse(TEXT("Changeset difference"), VersionEleven4 < VersionEleven3);
	TestFalse(TEXT("Changeset difference"), VersionEleven5 < VersionEleven4);
	TestFalse(TEXT("Changeset difference"), VersionEleven6 < VersionEleven5);

	return true; // actual results are returned by TestXxx() macros
}

#endif
