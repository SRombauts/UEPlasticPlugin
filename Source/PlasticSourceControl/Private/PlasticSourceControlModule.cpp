// Copyright (c) 2016-2019 Codice Software - Sebastien Rombauts (sebastien.rombauts@gmail.com)

#include "PlasticSourceControlModule.h"
#include "PlasticSourceControlOperations.h"
#include "Misc/App.h"
#include "Features/IModularFeatures.h"

#define LOCTEXT_NAMESPACE "PlasticSourceControl"

template<typename Type>
static TSharedRef<IPlasticSourceControlWorker, ESPMode::ThreadSafe> CreateWorker()
{
	return MakeShareable( new Type() );
}

void FPlasticSourceControlModule::StartupModule()
{
	// Register our operations (implemented in PlasticSourceControlOperations.cpp by subclassing from Engine\Source\Developer\SourceControl\Public\SourceControlOperations.h)
	PlasticSourceControlProvider.RegisterWorker("Connect", FGetPlasticSourceControlWorker::CreateStatic( &CreateWorker<FPlasticConnectWorker>));
	PlasticSourceControlProvider.RegisterWorker("CheckOut", FGetPlasticSourceControlWorker::CreateStatic(&CreateWorker<FPlasticCheckOutWorker>));
	PlasticSourceControlProvider.RegisterWorker("UpdateStatus", FGetPlasticSourceControlWorker::CreateStatic( &CreateWorker<FPlasticUpdateStatusWorker>));
	PlasticSourceControlProvider.RegisterWorker("MarkForAdd", FGetPlasticSourceControlWorker::CreateStatic(&CreateWorker<FPlasticMarkForAddWorker>));
	PlasticSourceControlProvider.RegisterWorker("Delete", FGetPlasticSourceControlWorker::CreateStatic(&CreateWorker<FPlasticDeleteWorker>));
	PlasticSourceControlProvider.RegisterWorker("Revert", FGetPlasticSourceControlWorker::CreateStatic(&CreateWorker<FPlasticRevertWorker>));
	PlasticSourceControlProvider.RegisterWorker("RevertUnchanged", FGetPlasticSourceControlWorker::CreateStatic(&CreateWorker<FPlasticRevertUnchangedWorker>));
	PlasticSourceControlProvider.RegisterWorker("RevertAll", FGetPlasticSourceControlWorker::CreateStatic(&CreateWorker<FPlasticRevertAllWorker>));
	PlasticSourceControlProvider.RegisterWorker("MakeWorkspace", FGetPlasticSourceControlWorker::CreateStatic(&CreateWorker<FPlasticMakeWorkspaceWorker>));
	PlasticSourceControlProvider.RegisterWorker("Sync", FGetPlasticSourceControlWorker::CreateStatic( &CreateWorker<FPlasticSyncWorker>));
	PlasticSourceControlProvider.RegisterWorker("CheckIn", FGetPlasticSourceControlWorker::CreateStatic(&CreateWorker<FPlasticCheckInWorker>));
	PlasticSourceControlProvider.RegisterWorker("Copy", FGetPlasticSourceControlWorker::CreateStatic(&CreateWorker<FPlasticCopyWorker>));
	PlasticSourceControlProvider.RegisterWorker("Resolve", FGetPlasticSourceControlWorker::CreateStatic(&CreateWorker<FPlasticResolveWorker>));

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
