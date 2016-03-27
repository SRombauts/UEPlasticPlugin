// Copyright (c) 2016 Codice Software - Sebastien Rombauts (sebastien.rombauts@gmail.com)

#include "PlasticSourceControlPrivatePCH.h"
#include "PlasticSourceControlModule.h"
#include "ModuleManager.h"
#include "ISourceControlModule.h"
#include "PlasticSourceControlOperations.h"
#include "Runtime/Core/Public/Features/IModularFeatures.h"

#define LOCTEXT_NAMESPACE "PlasticSourceControl"

template<typename Type>
static TSharedRef<IPlasticSourceControlWorker, ESPMode::ThreadSafe> CreateWorker()
{
	return MakeShareable( new Type() );
}

void FPlasticSourceControlModule::StartupModule()
{
	// Register our operations
	PlasticSourceControlProvider.RegisterWorker( "Connect", FGetPlasticSourceControlWorker::CreateStatic( &CreateWorker<FPlasticConnectWorker> ) );
//	PlasticSourceControlProvider.RegisterWorker( "UpdateStatus", FGetPlasticSourceControlWorker::CreateStatic( &CreateWorker<FPlasticUpdateStatusWorker> ) );

	// load our settings
	PlasticSourceControlSettings.LoadSettings();

	// Bind our source control provider to the editor
	IModularFeatures::Get().RegisterModularFeature( "SourceControl", &PlasticSourceControlProvider );
}

void FPlasticSourceControlModule::ShutdownModule()
{
	// shut down the provider, as this module is going away
	PlasticSourceControlProvider.Close();

	// unbind provider from editor
	IModularFeatures::Get().UnregisterModularFeature("SourceControl", &PlasticSourceControlProvider);
}

void FPlasticSourceControlModule::SaveSettings()
{
	if (FApp::IsUnattended() || IsRunningCommandlet())
	{
		return;
	}

	PlasticSourceControlSettings.SaveSettings();
}

IMPLEMENT_MODULE(FPlasticSourceControlModule, PlasticSourceControl);

#undef LOCTEXT_NAMESPACE
