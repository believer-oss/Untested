#include "Untest.h"
#include "UntestModule.h"

#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/NetDriver.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "UnrealEdGlobals.h"

DEFINE_LOG_CATEGORY(LogUntest);

#define UNTEST_FIXTURE_TASK_NAME(InTestName) TASK_NAME(__FUNCTION__, [InTestName]() { \
	return TestName;                                                                  \
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
	UNTEST_FIXTURE_TASK_NAME(TestName);

	co_await Setup(*FixtureContext);
}

UntestTask FUntestFixture::TeardownFixture(const FString TestName)
{
	UNTEST_FIXTURE_TASK_NAME(TestName);

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
// FUntestUnitFixture

UntestTask FUntestUnitFixture::RunFixture(const FString TestName)
{
	UNTEST_FIXTURE_TASK_NAME(TestName);

	co_await Run(GetContext());
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// FUntestWorldFixture

FUntestWorldFixture::~FUntestWorldFixture()
{
	TeardownWorld();
}

TSubclassOf<ULocalPlayer> FUntestWorldFixture::GetLocalPlayerClass() const
{
	return UCommonLocalPlayer::StaticClass();
}

UntestTask FUntestWorldFixture::SetupFixture(const FString TestName)
{
	UNTEST_FIXTURE_TASK_NAME(TestName);

	FUntestContext& TestContext = GetContext();

	const FString PackageName = FString::Printf(TEXT("/Untest/TestPackage_%s"), *TestName);
	UPackage* Package = NewObject<UPackage>(nullptr, *PackageName);
	Package->AddToRoot();
	Package->MarkAsFullyLoaded();
	TestContext.Packages[EUntestWorldType::Server] = Package;

	const FUntestGameClasses DefaultClasses = GetGameClasses();
	TSubclassOf<UUntestGameInstance> GameInstanceClass = DefaultClasses.GameInstanceClass
		? DefaultClasses.GameInstanceClass
		: TSubclassOf<UUntestGameInstance>(UUntestGameInstance::StaticClass());

	const FString GameInstanceName = FString::Printf(TEXT("UntestGameInstance_%s"), *TestName);

	UUntestGameInstance* GameInstance = CastChecked<UUntestGameInstance>(NewObject<UUntestGameInstance>(GetTransientPackage(), GameInstanceClass, *GameInstanceName));
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

	// Set custom GameMode if specified
	if (DefaultClasses.GameModeClass)
	{
		World->GetWorldSettings()->DefaultGameMode = DefaultClasses.GameModeClass;
		World->SetGameMode(FURL());
	}

	World->InitializeActorsForPlay(FURL());
	if (IsValid(World->GetWorldSettings()))
	{
		// Need to do this manually since world doesn't have a game mode
		World->GetWorldSettings()->NotifyBeginPlay();
		World->GetWorldSettings()->NotifyMatchStarted();
	}
	World->BeginPlay();

	// Optionally create a LocalPlayer for tests that need ULocalPlayerSubsystem derivatives.
	// We set WorldContext.GameViewport temporarily during AddLocalPlayer so that
	// PlayerAdded() passes it to the LocalPlayer — required for GetGameInstance() during
	// subsystem initialization. We null it immediately after to prevent the editor engine
	// tick's CleanupGameViewport() from destroying it.
	// The LocalPlayer retains ViewportClient as a UPROPERTY reference (set by PlayerAdded).
	if (ShouldCreateLocalPlayer())
	{
		TSubclassOf<ULocalPlayer> LocalPlayerClass = GetLocalPlayerClass();
		if (!LocalPlayerClass)
		{
			LocalPlayerClass = UCommonLocalPlayer::StaticClass();
		}

		TGuardValue<bool> EditorGuard(GIsEditor, false);
		UUntestViewportClient* VC = NewObject<UUntestViewportClient>(GEngine);
		VC->InitForTest(World, GameInstance);
		WorldContext.GameViewport = VC;

		const FPlatformUserId UserId = IPlatformInputDeviceMapper::Get().GetPrimaryPlatformUser();
		ULocalPlayer* NewPlayer = NewObject<ULocalPlayer>(GEngine, LocalPlayerClass);
		int32 InsertIndex = GameInstance->AddLocalPlayer(NewPlayer, UserId);

		// Remove from WorldContext immediately — the LocalPlayer keeps VC alive via ViewportClient.
		WorldContext.GameViewport = nullptr;

		if (InsertIndex == INDEX_NONE)
		{
			TestContext.AddError(TEXT("Failed to add LocalPlayer to GameInstance"));
			co_return;
		}
	}

	co_await Setup(TestContext);
}

UntestTask FUntestWorldFixture::RunFixture(const FString TestName)
{
	UNTEST_FIXTURE_TASK_NAME(TestName);

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

UntestTask FUntestWorldFixture::TeardownFixture(const FString TestName)
{
	UNTEST_FIXTURE_TASK_NAME(TestName);

	FUntestContext& TestContext = GetContext();
	co_await Teardown(TestContext);

	TeardownWorld();
}

void FUntestWorldFixture::TeardownWorld()
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

	// Remove local players BEFORE world teardown so their subsystems can
	// deinitialize while the world is still valid.
	if (UGameInstance* GameInstance = TestContext.GameInstances[EUntestWorldType::Server].Get())
	{
		if (UUntestGameInstance* TestGI = Cast<UUntestGameInstance>(GameInstance))
		{
			TestGI->SetAllowRemoveLocalPlayer(true);
		}
		while (GameInstance->GetLocalPlayers().Num() > 0)
		{
			GameInstance->RemoveLocalPlayer(GameInstance->GetLocalPlayers().Last());
		}
	}

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
// FUntestClientServerFixture

FUntestClientServerFixture::~FUntestClientServerFixture()
{
	// If the test timeouts, it won't get a chance to run the normal teardown logic, so we attempt
	// to run it again here just in case.
	TeardownClientServer();
}

UntestTask FUntestClientServerFixture::SetupFixture(const FString TestName)
{
	UNTEST_FIXTURE_TASK_NAME(TestName);

	const FUntestGameClasses DefaultClasses = GetGameClasses();
	const FUntestGameClasses Classes = {
		DefaultClasses.GameInstanceClass ? DefaultClasses.GameInstanceClass : TSubclassOf<UUntestGameInstance>(UUntestGameInstance::StaticClass()),
		DefaultClasses.GameModeClass ? DefaultClasses.GameModeClass : TSubclassOf<AGameModeBase>(AUntestGameMode::StaticClass()),
	};

	FUntestContext& TestContext = GetContext();

	FString PackageCommonName = FString::Printf(TEXT("TestPackage_%s"), *TestName);
	PackageCommonName.ReplaceCharInline('.', '_'); // UE seems to replace the final . with a : so just use underscores for consistency

	const int32 NumClients = FMath::Max(1, GetNumClients());

	// Total iterations: 1 server + NumClients clients. The first iteration is server (PIEInstance==Server==0),
	// then each subsequent iteration is a client (PIEInstance==1, 2, ...). Storage:
	//   - Server iteration writes into Worlds[Server] / GameInstances[Server] / Packages[Server].
	//   - First client iteration writes into Worlds[Client] / GameInstances[Client] / Packages[Client].
	//   - Additional client iterations append into ExtraClient* arrays.
	// This preserves EUntestWorldType::Count==2 and the macro contract for UNTEST_IS_SERVER/CLIENT.
	const int32 TotalIterations = 1 + NumClients;
	for (int32 IterIndex = 0; IterIndex < TotalIterations; ++IterIndex)
	{
		const bool bIsServerIter = (IterIndex == 0);
		const int32 ClientIndex = bIsServerIter ? INDEX_NONE : (IterIndex - 1); // 0..NumClients-1
		const ENetMode NetMode = bIsServerIter ? NM_DedicatedServer : NM_Client;
		const TCHAR* NetModeStr = bIsServerIter ? TEXT("Server") : TEXT("Client");

		// PIEInstance must be unique per world (Server==0, Client0==1, Client1==2, ...). The PIE package
		// prefix derives from this index so each client gets a distinct package name.
		const int32 PIEInstanceIndex = IterIndex;

		// UE expects that replicated worlds are owned by a parent package, since they need to have a stable name for networking. However
		// the world creation process will spawn actors into the parent package that have fixed names, which isn't allowed as all uobjects
		// must have unique names. PIE cheats by having the names of the packages be distinct, but remapping them when doing replication
		// so that the names match up, so we will hook into that system here. See UEditorEngine::NetworkRemapPath() and its usage in
		// PackageMapClient.cpp
		const FString PIEPackagePrefix = UWorld::BuildPIEPackagePrefix(PIEInstanceIndex);
		const FString PackageName = FString::Printf(TEXT("/Untest/%s%s"), *PIEPackagePrefix, *PackageCommonName);
		FSoftObjectPath::AddPIEPackageName(FName(*PackageName));

		// Add PKG_NewlyCreated flag to this package so we don't try to resolve its linker as it is unsaved duplicated world package
		UPackage* Package = NewObject<UPackage>(nullptr, *PackageName);
		Package->SetPackageFlags(PKG_NewlyCreated);
		Package->AddToRoot();
		Package->MarkAsFullyLoaded();

		// Create WorldContext and World BEFORE GameInstance->Init() so that subsystem
		// ShouldCreateSubsystem() checks (e.g., checking NetMode) can see the World.
		// The World uses bSkipInitWorld=true so early creation is safe.
		FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
		WorldContext.PIEInstance = PIEInstanceIndex;
		WorldContext.bWaitingOnOnlineSubsystem = false;
		WorldContext.PIEWorldFeatureLevel = GEditor->PreviewPlatform.GetEffectivePreviewFeatureLevel();
		WorldContext.RunAsDedicated = bIsServerIter;
		WorldContext.bIsPrimaryPIEInstance = false;

		const bool bInformEngineOfWorld = false;
		const FName WorldName = FName(*FString::Printf(TEXT("TestWorld_%s_%s%d"), *TestName, NetModeStr, bIsServerIter ? 0 : ClientIndex));
		const bool bAddToRoot = false;

		UWorld* World = UWorld::CreateWorld(EWorldType::Game, bInformEngineOfWorld, WorldName, Package, bAddToRoot, ERHIFeatureLevel::Num, nullptr, true /*bSkipInitWorld*/);
		if (World == nullptr)
		{
			TestContext.AddError(TEXT("Failed to create test world for netmode"));
			co_return;
		}

		World->GetWorldSettings()->DefaultGameMode = Classes.GameModeClass;
		World->ClearFlags(RF_Standalone);
		World->SetPlayInEditorInitialNetMode(NetMode);
		World->bAllowAudioPlayback = false;
		World->bIsNameStableForNetworking = true;
		WorldContext.SetCurrentWorld(World);

		// Now create GameInstance with the World already visible, so subsystem
		// ShouldCreateSubsystem() can query World->GetNetMode().
		UUntestGameInstance* GameInstance = CastChecked<UUntestGameInstance>(NewObject<UUntestGameInstance>(Package, Classes.GameInstanceClass));

		GameInstance->SetWorldContext(&WorldContext);
		WorldContext.OwningGameInstance = GameInstance;
		World->SetGameInstance(GameInstance);

		GameInstance->Init();
		GameInstance->ClearFlags(RF_Standalone);
		GameInstance->AddToRoot();

		// Stash into the appropriate slot now that the GI is initialized.
		if (bIsServerIter)
		{
			TestContext.Packages[EUntestWorldType::Server] = Package;
			TestContext.GameInstances[EUntestWorldType::Server] = GameInstance;
			TestContext.Worlds[EUntestWorldType::Server] = World;
		}
		else if (ClientIndex == 0)
		{
			TestContext.Packages[EUntestWorldType::Client] = Package;
			TestContext.GameInstances[EUntestWorldType::Client] = GameInstance;
			TestContext.Worlds[EUntestWorldType::Client] = World;
		}
		else
		{
			TestContext.ExtraClientPackages.Add(Package);
			TestContext.ExtraClientGameInstances.Add(GameInstance);
			TestContext.ExtraClientWorlds.Add(World);
		}

		const ULevelEditorPlaySettings* DefaultSettings = GetDefault<ULevelEditorPlaySettings>();
		uint16 ServerPort = 0;
		DefaultSettings->GetServerPort(ServerPort);
		const FString URLString = FString::Printf(TEXT("127.0.0.1:%hu"), ServerPort);
		FURL URL = FURL(nullptr, *URLString, TRAVEL_Absolute);
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

			// Make sure "always loaded" sub-levels are fully loaded
			// TODO make sure this is OK???
			World->FlushLevelStreaming(EFlushLevelStreamingType::Visibility);

			World->InitializeActorsForPlay(URL, true /*bResetTime*/, nullptr /*FRegisterComponentContext*/);

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

			// Networked connections require a player controller, which asserts a ULocalPlayer exists.
			// Temporarily set viewport on WorldContext during AddLocalPlayer for subsystem init.
			{
				TGuardValue<bool> EditorGuard(GIsEditor, false);
				UUntestViewportClient* VC = NewObject<UUntestViewportClient>(GEngine);
				VC->InitForTest(World, GameInstance);
				WorldContext.GameViewport = VC;

				const FPlatformUserId UserId = IPlatformInputDeviceMapper::Get().GetPrimaryPlatformUser();
				UCommonLocalPlayer* NewPlayer = NewObject<UCommonLocalPlayer>(GEngine, UCommonLocalPlayer::StaticClass());
				int32 InsertIndex = GameInstance->AddLocalPlayer(NewPlayer, UserId);
				check(InsertIndex != INDEX_NONE);

				// Disable player view to prevent Niagara (and other rendering systems)
				// from calling GetProjectionData with the sentinel FViewport* during world ticks.
				NewPlayer->SetIsPlayerViewEnabled(false);

				WorldContext.GameViewport = nullptr;
			}
		}

		WorldContext.LastURL = URL;
	}

	co_await Setup(TestContext);
}

UntestTask FUntestClientServerFixture::RunFixture(const FString TestName)
{
	UNTEST_FIXTURE_TASK_NAME(TestName);

	const int32 NumClients = FMath::Max(1, GetNumClients());

	UntestTask ServerTask = Run(GetContext(), EUntestWorldType::Server);

	// One ClientTask per client world. The macro layer always passes _WorldType==Client; tests
	// that need to differentiate among clients use UNTEST_IS_CLIENT() to short-circuit.
	// N clients => N independent client tasks.
	TArray<UntestTask> ClientTasks;
	ClientTasks.Reserve(NumClients);
	for (int32 i = 0; i < NumClients; ++i)
	{
		ClientTasks.Add(Run(GetContext(), EUntestWorldType::Client));
	}

	double LastTimestamp = FPlatformTime::Seconds();

	auto Func = [this, &ServerTask, &ClientTasks, &LastTimestamp]()
	{
		const double Now = FPlatformTime::Seconds();
		const double DeltaSeconds = Now - LastTimestamp;
		LastTimestamp = Now;

		FUntestContext& TestContext = GetContext();

		if (UWorld* ServerWorld = TestContext.Worlds[EUntestWorldType::Server].Get())
		{
			ServerWorld->Tick(LEVELTICK_All, DeltaSeconds);
		}
		ServerTask.Resume();

		const int32 NumLocalClients = ClientTasks.Num();
		for (int32 i = 0; i < NumLocalClients; ++i)
		{
			if (UWorld* ClientWorld = TestContext.GetClientWorld(i))
			{
				ClientWorld->Tick(LEVELTICK_All, DeltaSeconds);
			}
			ClientTasks[i].Resume();
		}

		bool bAllDone = ServerTask.IsDone();
		for (int32 i = 0; i < NumLocalClients && bAllDone; ++i)
		{
			bAllDone = bAllDone && ClientTasks[i].IsDone();
		}
		return bAllDone;
	};

	co_await Squid::WaitUntil(Func);
}

UntestTask FUntestClientServerFixture::TeardownFixture(const FString TestName)
{
	UNTEST_FIXTURE_TASK_NAME(TestName);

	FUntestContext& TestContext = GetContext();
	co_await Teardown(TestContext);

	TeardownClientServer();
}

void FUntestClientServerFixture::TeardownClientServer()
{
	FUntestContext& TestContext = GetContext();

	for (TWeakObjectPtr<UObject> ObjPtr : TestContext.Objects)
	{
		if (UObject* Obj = ObjPtr.Get())
		{
			Obj->ConditionalBeginDestroy();
		}
	}

	auto TeardownOneSlot = [](UGameInstance* GameInstance, UWorld* World, UPackage* Package)
	{
		// Remove local players BEFORE world teardown so their subsystems can
		// deinitialize while the world is still valid. UUntestGameInstance blocks
		// RemoveLocalPlayer by default to prevent accidental removal during
		// CleanupGameViewport; we explicitly enable it here for teardown.
		if (GameInstance)
		{
			if (UUntestGameInstance* TestGI = Cast<UUntestGameInstance>(GameInstance))
			{
				TestGI->SetAllowRemoveLocalPlayer(true);
			}
			while (GameInstance->GetLocalPlayers().Num() > 0)
			{
				GameInstance->RemoveLocalPlayer(GameInstance->GetLocalPlayers().Last());
			}
		}

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
	};

	// Tear down the canonical Server / Client(==index 0) slots first.
	for (int32 TestWorldType = EUntestWorldType::Server; TestWorldType != EUntestWorldType::Count; ++TestWorldType)
	{
		UGameInstance* GameInstance = TestContext.GameInstances[TestWorldType].Get();
		UWorld* World = TestContext.Worlds[TestWorldType].Get();
		UPackage* Package = TestContext.Packages[TestWorldType].Get();

		TeardownOneSlot(GameInstance, World, Package);

		TestContext.Worlds[TestWorldType].Reset();
		TestContext.GameInstances[TestWorldType].Reset();
		TestContext.Packages[TestWorldType].Reset();
	}

	// Tear down extra clients (indices 1..N-1) using the same teardown body.
	const int32 NumExtraClients = TestContext.ExtraClientWorlds.Num();
	for (int32 i = 0; i < NumExtraClients; ++i)
	{
		UGameInstance* GameInstance = TestContext.ExtraClientGameInstances.IsValidIndex(i) ? TestContext.ExtraClientGameInstances[i].Get() : nullptr;
		UWorld* World = TestContext.ExtraClientWorlds[i].Get();
		UPackage* Package = TestContext.ExtraClientPackages.IsValidIndex(i) ? TestContext.ExtraClientPackages[i].Get() : nullptr;

		TeardownOneSlot(GameInstance, World, Package);
	}

	TestContext.ExtraClientWorlds.Reset();
	TestContext.ExtraClientGameInstances.Reset();
	TestContext.ExtraClientPackages.Reset();

	FSoftObjectPath::ClearPIEPackageNames();
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
}
