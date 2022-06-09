// Copyright (c) 2016-2022 Codice Software

#include "PlasticSourceControlModule.h"

#include "IPlasticSourceControlWorker.h"
#include "PlasticSourceControlOperations.h"
#include "Misc/App.h"
#include "Features/IModularFeatures.h"

#define LOCTEXT_NAMESPACE "PlasticSourceControl"

void FPlasticSourceControlModule::StartupModule()
{
	// Register our operations (implemented in PlasticSourceControlOperations.cpp by sub-classing from Engine\Source\Developer\SourceControl\Public\SourceControlOperations.h)
	IPlasticSourceControlWorker::RegisterWorkers(PlasticSourceControlProvider);

	// Bind our source control provider to the editor
	IModularFeatures::Get().RegisterModularFeature("SourceControl", &PlasticSourceControlProvider);
}

void FPlasticSourceControlModule::ShutdownModule()
{
	// shut down the provider, as this module is going away
	PlasticSourceControlProvider.Close();

	// unbind provider from editor
	IModularFeatures::Get().UnregisterModularFeature("SourceControl", &PlasticSourceControlProvider);
}

IMPLEMENT_MODULE(FPlasticSourceControlModule, PlasticSourceControl);

#undef LOCTEXT_NAMESPACE
