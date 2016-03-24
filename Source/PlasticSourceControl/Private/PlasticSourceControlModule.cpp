// Copyright (c) 2016 Codice Software - Sebastien Rombauts (sebastien.rombauts@gmail.com)

#include "PlasticSourceControlPrivatePCH.h"
#include "PlasticSourceControlModule.h"
#include "ModuleManager.h"
#include "ISourceControlModule.h"
#include "Runtime/Core/Public/Features/IModularFeatures.h"

#define LOCTEXT_NAMESPACE "PlasticSourceControl"


void FPlasticSourceControlModule::StartupModule()
{
	// Register our operations
//	PlasticSourceControlProvider.RegisterWorker( "UpdateStatus", FGetPlasticSourceControlWorker::CreateStatic( &CreateWorker<FGitUpdateStatusWorker> ) );

	// Bind our source control provider to the editor
//	IModularFeatures::Get().RegisterModularFeature( "SourceControl", &PlasticSourceControlProvider );
}

void FPlasticSourceControlModule::ShutdownModule()
{
	// shut down the provider, as this module is going away
//	PlasticSourceControlProvider.Close();

	// unbind provider from editor
//	IModularFeatures::Get().UnregisterModularFeature("SourceControl", &PlasticSourceControlProvider);
}


IMPLEMENT_MODULE(FPlasticSourceControlModule, PlasticSourceControl);

#undef LOCTEXT_NAMESPACE
