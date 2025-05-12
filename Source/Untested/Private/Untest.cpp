#include "Untest.h"
#include "UntestModule.h"

#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/NetDriver.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "UnrealEdGlobals.h"

DEFINE_LOG_CATEGORY(LogUntest);

#define BV_FIXTURE_TASK_NAME(InTestName) TASK_NAME(__FUNCTION__, [InTestName]() { \
	return TestName;                                                              \
})

///////////////////////////////////////////////////////////////////////////////////////////////////
// FUntestFixtureFactory

FUntestFixtureFactory::FUntestFixtureFactory(FString InModuleName, FString InCategoryName, FString InTestName, EUntestTypeFlags InTestType, float InDefaultTimeout, FUntestOpts InOpts)
	: TestType(InTestType)
	, Opts(InOpts)
{
	Name.Module = InModuleName;
	Name.Category = InCategoryName;
	Name.Test = InTestName;

	Opts.TimeoutMs = (InOpts.TimeoutMs == 0.0f) ? InDefaultTimeout : InOpts.TimeoutMs; // 0.0 means the user didn't want to set a specific timeout
	FUntestModule::RegisterFixture(*this);
}

FUntestFixtureFactory::~FUntestFixtureFactory()
{
	FUntestModule::UnregisterFixture(*this);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// FUntestFixture

UntestTask FUntestFixture::SetupFixture(const FString TestName)
{
	BV_FIXTURE_TASK_NAME(TestName);

	co_await Setup(*FixtureContext);
}

UntestTask FUntestFixture::TeardownFixture(const FString TestName)
{
	BV_FIXTURE_TASK_NAME(TestName);

	co_await Teardown(*FixtureContext);
}

void FUntestFixture::SetContext(TSharedPtr<FUntestContext> InContext)
{
	checkf(FixtureContext.IsValid() == false, TEXT("Test fixture contexts are only allowed to be set once on creation in the fixture factory."));
	FixtureContext = InContext;
}

UntestTask FUntestFixture::Setup(FUntestContext& TestContext)
{
	return UntestTask();
}

UntestTask FUntestFixture::Teardown(FUntestContext& TestContext)
{
	return UntestTask();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// FBVUnitTestFixture

UntestTask FBVUnitTestFixture::RunFixture(const FString TestName)
{
	BV_FIXTURE_TASK_NAME(TestName);

	co_await Run(GetContext());
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// FBVWorldTestFixture

FBVWorldTestFixture::~FBVWorldTestFixture()
{
	TeardownWorld();
}

UntestTask FBVWorldTestFixture::SetupFixture(const FString TestName)
{
	BV_FIXTURE_TASK_NAME(TestName);

	FUntestContext& TestContext = GetContext();

	const FString PackageName = FString::Printf(TEXT("/Untest/TestPackage_%s"), *TestName);
	UPackage* Package = NewObject<UPackage>(nullptr, *PackageName);
	Package->AddToRoot();
	Package->MarkAsFullyLoaded();
	TestContext.Packages[EUntestWorldType::Server] = Package;

	const FString GameInstanceName = FString::Printf(TEXT("UntestGameInstance_%s"), *TestName);

	UUntestGameInstance* GameInstance = CastChecked<UUntestGameInstance>(NewObject<UUntestGameInstance>(GetTransientPackage(), *GameInstanceName));
	TestContext.GameInstances[EUntestWorldType::Server] = GameInstance;

	const bool bInformEngineOfWorld = false;
	const bool bAddToRoot = false;
	const bool bSkipInitWorld = true;
	const FString WorldName = FString::Printf(TEXT("UntestWorld_%s"), *TestName);
	UWorld* World = UWorld::CreateWorld(EWorldType::PIE, bInformEngineOfWorld, *WorldName, Package, bAddToRoot, ERHIFeatureLevel::Num, nullptr, bSkipInitWorld);
	if (World == nullptr)
	{
		TestContext.AddError(TEXT("Failed to create test world"));
		co_return;
	}
	TestContext.Worlds[EUntestWorldType::Server] = World;

	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.RunAsDedicated = true;
	WorldContext.OwningGameInstance = GameInstance;
	WorldContext.SetCurrentWorld(World);

	GameInstance->SetWorldContext(&WorldContext);
	GameInstance->Init();
	GameInstance->ClearFlags(RF_Standalone);
	GameInstance->AddToRoot();

	World->SetGameInstance(GameInstance);
	World->InitWorld();
	World->SetPlayInEditorInitialNetMode(NM_DedicatedServer);
	World->InitializeActorsForPlay(FURL());
	if (IsValid(World->GetWorldSettings()))
	{
		// Need to do this manually since world doesn't have a game mode
		World->GetWorldSettings()->NotifyBeginPlay();
		World->GetWorldSettings()->NotifyMatchStarted();
	}
	World->BeginPlay();

	co_await Setup(TestContext);
}

UntestTask FBVWorldTestFixture::RunFixture(const FString TestName)
{
	BV_FIXTURE_TASK_NAME(TestName);

	FUntestContext& TestContext = GetContext();
	UntestTask Task = Run(TestContext, EUntestWorldType::Server);

	double LastTimestamp = FPlatformTime::Seconds();

	auto Func = [this, &Task, &LastTimestamp]()
	{
		const double Now = FPlatformTime::Seconds();
		const double DeltaSeconds = Now - LastTimestamp;
		LastTimestamp = Now;

		FUntestContext& TestContext = GetContext();

		TestContext.Worlds[EUntestWorldType::Server]->Tick(LEVELTICK_All, DeltaSeconds);
		Task.Resume();

		return Task.IsDone();
	};

	co_await Squid::WaitUntil(Func);
}

UntestTask FBVWorldTestFixture::TeardownFixture(const FString TestName)
{
	BV_FIXTURE_TASK_NAME(TestName);

	FUntestContext& TestContext = GetContext();
	co_await Teardown(TestContext);

	TeardownWorld();
}

void FBVWorldTestFixture::TeardownWorld()
{
	FUntestContext& TestContext = GetContext();

	for (TWeakObjectPtr<UObject> ObjPtr : TestContext.Objects)
	{
		if (UObject* Obj = ObjPtr.Get())
		{
			Obj->ConditionalBeginDestroy();
		}
	}

	// See https://minifloppy.it/posts/2024/automated-testing-specs-ue5/#uworld-fixture
	if (UWorld* World = TestContext.Worlds[EUntestWorldType::Server].Get())
	{
		World->BeginTearingDown();

		// DestroyWorld doesn't do this and instead waits for GC to clear everything up
		for (auto It = TActorIterator<AActor>(World); It; ++It)
		{
			It->Destroy();
		}

		GEngine->DestroyWorldContext(World);
		World->RemoveFromRoot();
		World->DestroyWorld(false /*bInformEngineOfWorld*/);
	}

	if (UGameInstance* GameInstance = TestContext.GameInstances[EUntestWorldType::Server].Get())
	{
		GameInstance->Shutdown();
		GameInstance->RemoveFromRoot();
		GameInstance->ConditionalBeginDestroy();
	}

	if (UPackage* Package = TestContext.Packages[EUntestWorldType::Server].Get())
	{
		Package->RemoveFromRoot();
		Package->ConditionalBeginDestroy();
	}

	TestContext.Worlds[EUntestWorldType::Server].Reset();
	TestContext.GameInstances[EUntestWorldType::Server].Reset();
	TestContext.Packages[EUntestWorldType::Server].Reset();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// FBVClientServerTestFixture

FBVClientServerTestFixture::~FBVClientServerTestFixture()
{
	// If the test timeouts, it won't get a chance to run the normal teardown logic, so we attempt
	// to run it again here just in case.
	TeardownClientServer();
}

UntestTask FBVClientServerTestFixture::SetupFixture(const FString TestName)
{
	BV_FIXTURE_TASK_NAME(TestName);

	const FUntestGameClasses DefaultClasses = GetGameClasses();
	const FUntestGameClasses Classes = {
		DefaultClasses.GameInstanceClass ? DefaultClasses.GameInstanceClass : TSubclassOf<UUntestGameInstance>(UUntestGameInstance::StaticClass()),
		DefaultClasses.GameModeClass ? DefaultClasses.GameModeClass : TSubclassOf<AGameModeBase>(AGameModeBase::StaticClass()),
	};

	FUntestContext& TestContext = GetContext();

	FString PackageCommonName = FString::Printf(TEXT("TestPackage_%s"), *TestName);
	PackageCommonName.ReplaceCharInline('.', '_'); // UE seems to replace the final . with a : so just use underscores for consistency

	// See UEditorEngine::CreateInnerProcessPIEGameInstance() for the reference code for this setup logic
	for (int32 TestWorldType = EUntestWorldType::Server; TestWorldType != EUntestWorldType::Count; ++TestWorldType)
	{
		const ENetMode NetMode = (TestWorldType == EUntestWorldType::Server) ? NM_DedicatedServer : NM_Client;
		const TCHAR* NetModeStr = (TestWorldType == EUntestWorldType::Server) ? TEXT("Server") : TEXT("Client");

		// UE expects that replicated worlds are owned by a parent package, since they need to have a stable name for networking. However
		// the world creation process will spawn actors into the parent package that have fixed names, which isn't allowed as all uobjects
		// must have unique names. PIE cheats by having the names of the packages be distinct, but remapping them when doing replication
		// so that the names match up, so we will hook into that system here. See UEditorEngine::NetworkRemapPath() and its usage in
		// PackageMapClient.cpp
		const FString PIEPackagePrefix = UWorld::BuildPIEPackagePrefix(TestWorldType);
		const FString PackageName = FString::Printf(TEXT("/Untest/%s%s"), *PIEPackagePrefix, *PackageCommonName);
		FSoftObjectPath::AddPIEPackageName(FName(*PackageName));

		// Add PKG_NewlyCreated flag to this package so we don't try to resolve its linker as it is unsaved duplicated world package
		UPackage* Package = NewObject<UPackage>(nullptr, *PackageName);
		Package->SetPackageFlags(PKG_NewlyCreated);
		Package->AddToRoot();
		Package->MarkAsFullyLoaded();

		TestContext.Packages[TestWorldType] = Package;

		UUntestGameInstance* GameInstance = CastChecked<UUntestGameInstance>(NewObject<UUntestGameInstance>(Package, Classes.GameInstanceClass));
		TestContext.GameInstances[TestWorldType] = GameInstance;

		GameInstance->Init();
		GameInstance->ClearFlags(RF_Standalone);
		GameInstance->AddToRoot();

		FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
		GameInstance->SetWorldContext(&WorldContext);
		WorldContext.PIEInstance = TestWorldType;
		WorldContext.bWaitingOnOnlineSubsystem = false;

		WorldContext.PIEWorldFeatureLevel = GEditor->PreviewPlatform.GetEffectivePreviewFeatureLevel();
		WorldContext.RunAsDedicated = (TestWorldType == EUntestWorldType::Server);
		WorldContext.bIsPrimaryPIEInstance = false;
		WorldContext.OwningGameInstance = GameInstance;

		const bool bInformEngineOfWorld = false;
		const FName WorldName = FName(*FString::Printf(TEXT("TestWorld_%s"), *TestName));
		const bool bAddToRoot = false;

		UWorld* World = UWorld::CreateWorld(EWorldType::Game, bInformEngineOfWorld, WorldName, Package, bAddToRoot, ERHIFeatureLevel::Num, nullptr, true /*bSkipInitWorld*/);
		if (World == nullptr)
		{
			TestContext.AddError(TEXT("Failed to create test world for netmode"));
			co_return;
		}

		World->GetWorldSettings()->DefaultGameMode = Classes.GameModeClass;
		World->SetGameInstance(GameInstance);
		World->ClearFlags(RF_Standalone);
		World->SetPlayInEditorInitialNetMode(NetMode);
		World->bAllowAudioPlayback = false;
		World->bIsNameStableForNetworking = true;
		WorldContext.SetCurrentWorld(World);

		TestContext.Worlds[TestWorldType] = World;

		const ULevelEditorPlaySettings* DefaultSettings = GetDefault<ULevelEditorPlaySettings>();
		uint16 ServerPort = 0;
		DefaultSettings->GetServerPort(ServerPort);
		const FString URLString = FString::Printf(TEXT("127.0.0.1:%hu"), ServerPort);
		FURL URL = FURL(nullptr, *URLString, TRAVEL_Absolute);
		// URL.Map = TEXT("/Game/Developers/Test/Levels/Default");
		URL.Port = ServerPort;

		if (NetMode == NM_DedicatedServer)
		{
			World->InitWorld(UWorld::InitializationValues());

			// Finish server world loading and open net connection
			check(World->GetAuthGameMode() == nullptr);

			FActorSpawnParameters SpawnInfo;
			SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			SpawnInfo.ObjectFlags |= RF_Transient; // We never want to save game modes into a map

			World->SetGameMode(URL);

			// AGameModeBase* GameMode = World->SpawnActor<AGameModeBase>(SpawnInfo);
			// if (GameMode == nullptr)
			// {
			// 	TestContext.AddError(TEXT("Failed to spawn GameMode"));
			// }

			// Make sure "always loaded" sub-levels are fully loaded
			// TODO make sure this is OK???
			World->FlushLevelStreaming(EFlushLevelStreamingType::Visibility);

			// TODO should create AI system?
			// World->CreateAISystem();

			World->InitializeActorsForPlay(URL, true /*bResetTime*/, nullptr /*FRegisterComponentContext*/);

			// calling it after InitializeActorsForPlay has been called to have all potential bounding boxed initialized
			// TODO should initialize navigation system??
			// FNavigationSystem::AddNavigationSystemToWorld(*World, FNavigationSystemRunMode::PIEMode);

			// TODO should we do this?
			// GEngine->BlockTillLevelStreamingCompleted(World);

			// See UGameInstance::EnableListenServer() for this logic. We can't call it directly because it requires
			// the World to be PIE.
			URL.AddOption(TEXT("Listen"));
			check(World->GetNetDriver() == nullptr);

			// This actually opens the port
			if (World->Listen(URL) == false)
			{
				TestContext.AddError(TEXT("Failed to start listen server"));
				co_return;
			}
			check(World->GetNetMode() == NM_DedicatedServer);

			World->BeginPlay();
			World->GetNetDriver()->bNoTimeouts = true;
		}
		else
		{
			// If any of our game instances need this then we'll need to rework the below code
			check(WorldContext.OwningGameInstance->DelayPendingNetGameTravel() == false);

			// Connect client world to server
			UPendingNetGame* PendingNetGame = NewObject<UPendingNetGame>();
			WorldContext.PendingNetGame = PendingNetGame; // need to set this because InitNetDriver looks at it
			PendingNetGame->Initialize(URL);
			PendingNetGame->InitNetDriver();
			PendingNetGame->NetDriver->bNoTimeouts = true;

			double LastTimestamp = FPlatformTime::Seconds();

			auto TryConnectFunc = [&TestContext, PendingNetGame, &LastTimestamp]()
			{
				const double Now = FPlatformTime::Seconds();
				const double DeltaSeconds = Now - LastTimestamp;
				LastTimestamp = Now;

				UWorld* ServerWorld = TestContext.Worlds[EUntestWorldType::Server].Get();
				ServerWorld->Tick(LEVELTICK_All, DeltaSeconds); // give server NetDriver a chance to respond to requests
				PendingNetGame->Tick(DeltaSeconds);

				check(PendingNetGame->bSentJoinRequest == false);
				return PendingNetGame->bSuccessfullyConnected;
			};
			co_await Squid::WaitUntil(TryConnectFunc);

			PendingNetGame->NetDriver->NetDriverName = NAME_GameNetDriver;
			World->SetNetDriver(PendingNetGame->NetDriver);
			PendingNetGame->NetDriver->SetWorld(World);

			PendingNetGame->SendJoin();
			PendingNetGame->NetDriver = NULL;
			PendingNetGame->ConditionalBeginDestroy();
			PendingNetGame = nullptr;
			WorldContext.PendingNetGame = nullptr;

			World->FlushLevelStreaming(EFlushLevelStreamingType::Visibility);
			World->InitWorld(UWorld::InitializationValues());

			// These level collections are created in InitWorld, and their NetDriver will eventually override the one in
			// the UWorld, so we set it up as soon as possible
			if (FLevelCollection* SourceCollection = World->FindCollectionByType(ELevelCollectionType::DynamicSourceLevels))
			{
				SourceCollection->SetNetDriver(World->GetNetDriver());
			}
			if (FLevelCollection* StaticCollection = World->FindCollectionByType(ELevelCollectionType::StaticLevels))
			{
				StaticCollection->SetNetDriver(World->GetNetDriver());
			}

			World->InitializeActorsForPlay(URL, true /*bResetTime*/, nullptr /*FRegisterComponentContext*/);

			// Networked connections require a player controller, which asserts a ULocalPlayer exists
			{
				const FPlatformUserId UserId = IPlatformInputDeviceMapper::Get().GetPrimaryPlatformUser();

				ULocalPlayer* NewPlayer = NewObject<ULocalPlayer>(GEngine, ULocalPlayer::StaticClass());
				int32 InsertIndex = GameInstance->AddLocalPlayer(NewPlayer, UserId);
				check(InsertIndex != INDEX_NONE);
			}
		}

		WorldContext.LastURL = URL;
	}

	co_await Setup(TestContext);
}

UntestTask FBVClientServerTestFixture::RunFixture(const FString TestName)
{
	BV_FIXTURE_TASK_NAME(TestName);

	UntestTask ServerTask = Run(GetContext(), EUntestWorldType::Server);
	UntestTask ClientTask = Run(GetContext(), EUntestWorldType::Client);

	double LastTimestamp = FPlatformTime::Seconds();

	auto Func = [this, &ServerTask, &ClientTask, &LastTimestamp]()
	{
		const double Now = FPlatformTime::Seconds();
		const double DeltaSeconds = Now - LastTimestamp;
		LastTimestamp = Now;

		FUntestContext& TestContext = GetContext();

		TestContext.Worlds[EUntestWorldType::Server]->Tick(LEVELTICK_All, DeltaSeconds);
		ServerTask.Resume();

		TestContext.Worlds[EUntestWorldType::Client]->Tick(LEVELTICK_All, DeltaSeconds);
		ClientTask.Resume();

		const bool bIsServerDone = ServerTask.IsDone();
		const bool bIsClientDone = ClientTask.IsDone();
		return bIsServerDone && bIsClientDone;
	};

	co_await Squid::WaitUntil(Func);
}

UntestTask FBVClientServerTestFixture::TeardownFixture(const FString TestName)
{
	BV_FIXTURE_TASK_NAME(TestName);

	FUntestContext& TestContext = GetContext();
	co_await Teardown(TestContext);

	TeardownClientServer();
}

void FBVClientServerTestFixture::TeardownClientServer()
{
	FUntestContext& TestContext = GetContext();

	for (TWeakObjectPtr<UObject> ObjPtr : TestContext.Objects)
	{
		if (UObject* Obj = ObjPtr.Get())
		{
			Obj->ConditionalBeginDestroy();
		}
	}

	for (int32 TestWorldType = EUntestWorldType::Server; TestWorldType != EUntestWorldType::Count; ++TestWorldType)
	{
		UGameInstance* GameInstance = TestContext.GameInstances[TestWorldType].Get();
		UWorld* World = TestContext.Worlds[TestWorldType].Get();
		UPackage* Package = TestContext.Packages[TestWorldType].Get();

		if (World)
		{
			World->BeginTearingDown();

			// DestroyWorld doesn't do this and instead waits for GC to clear everything up
			for (auto It = TActorIterator<AActor>(World); It; ++It)
			{
				It->Destroy();
			}

			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false /*bInformEngineOfWorld*/);
		}

		if (GameInstance)
		{
			GameInstance->Shutdown();
			GameInstance->RemoveFromRoot();
			GameInstance->ConditionalBeginDestroy();
		}

		if (Package)
		{
			Package->RemoveFromRoot();
			Package->ConditionalBeginDestroy();
		}

		TestContext.Worlds[TestWorldType].Reset();
		TestContext.GameInstances[TestWorldType].Reset();
		TestContext.Packages[TestWorldType].Reset();
	}

	FSoftObjectPath::ClearPIEPackageNames();

	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
}
