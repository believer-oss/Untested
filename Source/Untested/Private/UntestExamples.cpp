#include "UntestExamples.h"
#include "Untest.h"

#include "Engine/DataTable.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

//////////////////////////////////////////////////////////////////////////////////////////////

AUntestExamplePlayerController::AUntestExamplePlayerController()
{
	bReplicates = true;
	bAlwaysRelevant = true;
}

void AUntestExamplePlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUntestExamplePlayerController, ReplicatedInt);
}

void AUntestExamplePlayerController::OnRep_ReplicatedInt()
{
	bWasOnRepCalled = true;
}

void AUntestExamplePlayerController::ClientRPC_Implementation()
{
	bWasClientRpcCalled = true;
}

void AUntestExamplePlayerController::ServerRPC_Implementation()
{
	bWasServerRpcCalled = true;
}

void AUntestExamplePlayerController::NetMulticastRPC_Implementation()
{
	bWasMulticastCalled = true;
}

AUntestExampleGameMode::AUntestExampleGameMode()
{
	PlayerControllerClass = AUntestExamplePlayerController::StaticClass();
}

//////////////////////////////////////////////////////////////////////////////////////////////

// Expects fail a test, but don't cause it to exit immediately. Use for non-fatal errors.
UNTEST_UNIT(Untest, Examples, Expects)
{
	UNTEST_EXPECT_EQ(0xBE, 0xBE);
	UNTEST_EXPECT_NEAR(1.0f, 1.00009f, 0.0001f);
	UNTEST_EXPECT_NE(0x1337, 0xBEEF);
	UNTEST_EXPECT_GT(10, 0);
	UNTEST_EXPECT_GE(10, 10);
	UNTEST_EXPECT_LT(0, 10);
	UNTEST_EXPECT_LE(10, 10);

	UNTEST_EXPECT_TRUE(true);
	UNTEST_EXPECT_FALSE(false);

	// pointer tests
	int32 IntData = 0xBE11E7E4;
	int32* NullPtr = nullptr;
	int32* ValidPtr = &IntData;

	UNTEST_EXPECT_EQ(NullPtr, nullptr);
	UNTEST_EXPECT_NE(ValidPtr, nullptr);

	UNTEST_EXPECT_NULLPTR(NullPtr);
	UNTEST_EXPECT_PTR(ValidPtr);

	// string tests
	const TCHAR* MyTestStr = TEXT("My Test String");
	const TCHAR* MyTestStrCaps = TEXT("MY TEST STRING");
	const TCHAR* ThunderdomeStr = TEXT("Thunderdome");

	FString MyTestFStr = MyTestStr;
	FString MyTestFStrCaps = MyTestStrCaps;
	FString ThunderdomeFStr = ThunderdomeStr;

	// regular _EQ and _NE macros only compare pointers
	UNTEST_EXPECT_NE(*MyTestFStr, MyTestStr);
	UNTEST_EXPECT_NE(*MyTestFStrCaps, MyTestStrCaps);
	UNTEST_EXPECT_NE(*ThunderdomeFStr, ThunderdomeStr);

	UNTEST_EXPECT_NE(*MyTestFStr, MyTestStr);
	UNTEST_EXPECT_NE(*MyTestFStrCaps, MyTestStrCaps);
	UNTEST_EXPECT_NE(*ThunderdomeFStr, ThunderdomeStr);

	// use _STREQ/_STRNE macros to compare strings. case-sensitive by default:
	UNTEST_EXPECT_STREQ(MyTestStr, MyTestStr);
	UNTEST_EXPECT_STRNE(MyTestStr, MyTestStrCaps);
	UNTEST_EXPECT_STRNE(MyTestStr, ThunderdomeStr);

	UNTEST_EXPECT_STREQ(MyTestFStr, MyTestFStr);
	UNTEST_EXPECT_STRNE(MyTestFStr, MyTestStrCaps);
	UNTEST_EXPECT_STRNE(MyTestFStr, ThunderdomeFStr);

	// _STRCASEEQ/_STRCASENE perform case-insensitive comparisons:
	UNTEST_EXPECT_STRCASEEQ(MyTestStr, MyTestFStr);
	UNTEST_EXPECT_STRCASEEQ(MyTestStr, MyTestFStrCaps);
	UNTEST_EXPECT_STRCASENE(MyTestStr, ThunderdomeFStr);

	UNTEST_EXPECT_STRCASEEQ(MyTestFStr, MyTestFStr);
	UNTEST_EXPECT_STRCASEEQ(MyTestFStr, MyTestFStrCaps);
	UNTEST_EXPECT_STRCASENE(MyTestFStr, ThunderdomeFStr);

	// You can also mix/match TCHAR* and FString:
	UNTEST_EXPECT_STRCASEEQ(MyTestStr, MyTestFStrCaps);
	UNTEST_EXPECT_STRCASEEQ(MyTestFStrCaps, MyTestStr);
	UNTEST_EXPECT_STRCASENE(MyTestFStr, ThunderdomeStr);
	UNTEST_EXPECT_STRCASENE(ThunderdomeFStr, MyTestStr);

	// validity testing with any TSharedPtr<>
	TSharedPtr<int32> Valid = MakeShared<int32>(0);
	TSharedPtr<int32> Invalid;

	UNTEST_EXPECT_VALID(Valid);
	UNTEST_EXPECT_INVALID(Invalid);

	co_return;
}

// Asserts fail a test, and cause it to exit immediately. Use for fatal errors.
UNTEST_UNIT(Untest, Examples, Asserts)
{
	UNTEST_ASSERT_EQ(0xBE, 0xBE);
	UNTEST_ASSERT_NEAR(1.0f, 1.00009f, 0.0001f);
	UNTEST_ASSERT_NE(0x1337, 0xBEEF);
	UNTEST_ASSERT_GT(10, 0);
	UNTEST_ASSERT_GE(10, 10);
	UNTEST_ASSERT_LT(0, 10);
	UNTEST_ASSERT_LE(10, 10);
	UNTEST_ASSERT_TRUE(true);
	UNTEST_ASSERT_FALSE(false);

	// null pointer tests must use their own macro due to limitations in how UE handles pointers in UTEST_EQUAL() and UTEST_NOT_EQUAL()
	int32 IntData = 0xBE11E7E4;
	int32* NullPtr = nullptr;
	int32* ValidPtr = &IntData;

	UNTEST_ASSERT_NULLPTR(NullPtr);
	UNTEST_ASSERT_PTR(ValidPtr);

	const TCHAR* MyTestStr = TEXT("MyTest String");
	const TCHAR* MyTestStrCaps = TEXT("MYTEST STRING");
	const TCHAR* ThunderdomeStr = TEXT("Thunderdome");

	FString MyTestFStr = MyTestStr;
	FString MyTestFStrCaps = MyTestStrCaps;
	FString ThunderdomeFStr = ThunderdomeStr;

	// regular _EQ and _NE macros only compare pointers
	UNTEST_ASSERT_NE(*MyTestFStr, MyTestStr);
	UNTEST_ASSERT_NE(*MyTestFStrCaps, MyTestStrCaps);
	UNTEST_ASSERT_NE(*ThunderdomeFStr, ThunderdomeStr);

	UNTEST_ASSERT_NE(*MyTestFStr, MyTestStr);
	UNTEST_ASSERT_NE(*MyTestFStrCaps, MyTestStrCaps);
	UNTEST_ASSERT_NE(*ThunderdomeFStr, ThunderdomeStr);

	// use _STREQ/_STRNE macros to compare strings. case-sensitive by default:
	UNTEST_ASSERT_STREQ(MyTestStr, MyTestStr);
	UNTEST_ASSERT_STRNE(MyTestStr, MyTestStrCaps);
	UNTEST_ASSERT_STRNE(MyTestStr, ThunderdomeStr);

	UNTEST_ASSERT_STREQ(MyTestFStr, MyTestFStr);
	UNTEST_ASSERT_STRNE(MyTestFStr, MyTestStrCaps);
	UNTEST_ASSERT_STRNE(MyTestFStr, ThunderdomeFStr);

	// _STRCASEEQ/_STRCASENE perform case-insensitive comparisons:
	UNTEST_ASSERT_STRCASEEQ(MyTestStr, MyTestFStr);
	UNTEST_ASSERT_STRCASEEQ(MyTestStr, MyTestFStrCaps);
	UNTEST_ASSERT_STRCASENE(MyTestStr, ThunderdomeFStr);

	UNTEST_ASSERT_STRCASEEQ(MyTestFStr, MyTestFStr);
	UNTEST_ASSERT_STRCASEEQ(MyTestFStr, MyTestFStrCaps);
	UNTEST_ASSERT_STRCASENE(MyTestFStr, ThunderdomeFStr);

	// You can also mix/match TCHAR* and FString:
	UNTEST_ASSERT_STRCASEEQ(MyTestStr, MyTestFStrCaps);
	UNTEST_ASSERT_STRCASEEQ(MyTestFStrCaps, MyTestStr);
	UNTEST_ASSERT_STRCASENE(MyTestFStr, ThunderdomeStr);
	UNTEST_ASSERT_STRCASENE(ThunderdomeFStr, MyTestStr);

	// validity testing with any TSharedPtr<>
	TSharedPtr<int32> Valid = MakeShared<int32>(0);
	TSharedPtr<int32> Invalid;

	UNTEST_ASSERT_VALID(Valid);
	UNTEST_ASSERT_INVALID(Invalid);

	co_return;
}

UNTEST_UNIT_OPTS(Untest, Examples, CoroutineSuspend, UNTEST_TIMEOUTMS(32.0))
{
	co_await Squid::Suspend();
}

UNTEST_UNIT_OPTS(Untest, Examples, DisabledMacro, UNTEST_DISABLED())
{
	co_return;
}

UNTEST_UNIT_OPTS(Untest, Examples, DisabledFlag, UNTEST_FLAGS(EUntestFlags::Disabled))
{
	co_return;
}

UNTEST_UNIT_OPTS(Untest, Examples, PureMacro, UNTEST_PURE())
{
	co_return;
}

UNTEST_UNIT_OPTS(Untest, Examples, PureFlag, UNTEST_FLAGS(EUntestFlags::Pure))
{
	co_return;
}

UNTEST_UNIT_PURE(Untest, Examples, PureDecl)
{
	co_return;
}

UNTEST_UNIT_OPTS(Untest, Examples, Timeout, UNTEST_TIMEOUTMS(50))
{
	co_await Squid::Suspend();
	co_await Squid::Suspend();
	co_await Squid::Suspend();
}

UNTEST_UNIT_OPTS(Untest, Examples, TimeoutFlags, UNTEST_TIMEOUTMS_FLAGS(50, EUntestFlags::Pure))
{
	co_await Squid::Suspend();
	co_await Squid::Suspend();
	co_await Squid::Suspend();
}

// If you want, you can define the flags elsewhere and reuse them
const FUntestOpts TimeoutFlags2Opts = FUntestOpts(50, EUntestFlags::Pure);

UNTEST_UNIT_OPTS(Untest, Examples, TimeoutFlags2, TimeoutFlags2Opts)
{
	co_await Squid::Suspend();
	co_await Squid::Suspend();
	co_await Squid::Suspend();
}

class FExampleCustomUnitTestFixture : public FBVUnitTestFixture
{
	virtual UntestTask Setup(FUntestContext& TestContext) override
	{
		PresetData = MakeShared<int>(1337);
		co_return;
	}

	virtual UntestTask Teardown(FUntestContext& TestContext) override
	{
		PresetData = nullptr;
		co_return;
	}

	// Any data or functions used in the test must be declared public or protected
public:
	TSharedPtr<int> PresetData;
};

UNTEST_UNIT_F(FExampleCustomUnitTestFixture, Untest, Examples, CustomFixture)
{
	UNTEST_ASSERT_VALID(PresetData);
	UNTEST_EXPECT_EQ(*PresetData, 1337);

	co_return;
}

UNTEST_UNIT_F_OPTS(FExampleCustomUnitTestFixture, Untest, Examples, CustomFixtureFlags, TimeoutFlags2Opts)
{
	UNTEST_ASSERT_VALID(PresetData);
	UNTEST_EXPECT_EQ(*PresetData, 1337);

	co_return;
}

UNTEST_WORLD(Untest, Examples, WorldSimple)
{
	UGameInstance* GI = UNTEST_GET_GAMEINSTANCE();
	UWorld* World = UNTEST_GET_WORLD();

	UNTEST_ASSERT_PTR(GI);
	UNTEST_ASSERT_PTR(World);

	UNTEST_EXPECT_TRUE(UNTEST_IS_SERVER());
	UNTEST_EXPECT_FALSE(UNTEST_IS_CLIENT());

	UNTEST_EXPECT_TRUE(GI->IsDedicatedServerInstance());
	UNTEST_EXPECT_EQ(World->GetNetMode(), NM_DedicatedServer);
}

UNTEST_WORLD(Untest, Examples, WorldMakeObjects)
{
	TArray<UDataTable*> Tables;
	TArray<FString> ObjNames;
	for (int i = 0; i < 5; ++i)
	{
		UDataTable* Table = NewTestObject<UDataTable>();
		UNTEST_ASSERT_PTR(Table);

		ObjNames.AddUnique(Table->GetName());
		Tables.AddUnique(Table);
	}

	UNTEST_EXPECT_EQ(ObjNames.Num(), Tables.Num());
}

// This function runs concurrently with a server and client after the client connects.
UNTEST_CLIENTSERVER(Untest, Examples, ClientServerSimple)
{
	UGameInstance* GI = UNTEST_GET_GAMEINSTANCE();
	UWorld* World = UNTEST_GET_WORLD();

	UNTEST_ASSERT_PTR(GI);
	UNTEST_ASSERT_PTR(World);

	if (UNTEST_IS_SERVER())
	{
		UNTEST_EXPECT_TRUE(GI->IsDedicatedServerInstance());
		UNTEST_EXPECT_EQ(World->GetNetMode(), NM_DedicatedServer);
	}

	if (UNTEST_IS_CLIENT())
	{
		UNTEST_EXPECT_FALSE(GI->IsDedicatedServerInstance());
		UNTEST_EXPECT_EQ(World->GetNetMode(), NM_Client);
	}

	co_return;
}

struct UntestClientServerReplicationFixture : public FBVClientServerTestFixture
{
	virtual FUntestGameClasses GetGameClasses() const override
	{
		return FUntestGameClasses{
			nullptr,							   // GameInstance class
			AUntestExampleGameMode::StaticClass(), // uses the AUntestExamplePlayerController class
		};
	}
};

UNTEST_CLIENTSERVER_F(UntestClientServerReplicationFixture, Untest, Examples, ClientServerReplication)
{
	UWorld* World = UNTEST_GET_WORLD();

	if (UNTEST_IS_SERVER())
	{
		AUntestExamplePlayerController* Actor = nullptr;
		for (TActorIterator<AUntestExamplePlayerController> It(World); It; ++It)
		{
			if (AUntestExamplePlayerController* WorldActor = *It)
			{
				Actor = WorldActor;
				break;
			}
		}

		UNTEST_ASSERT_PTR(Actor);

		// yield until server rpc is called from client
		co_await Squid::WaitUntil([Actor]()
			{
				return Actor->bWasServerRpcCalled;
			});

		Actor->ReplicatedInt = 1337;
		Actor->bWasServerRpcCalled = false;

		co_await Squid::WaitUntil([Actor]()
			{
				return Actor->bWasServerRpcCalled;
			});

		Actor->bWasServerRpcCalled = false;
		Actor->ClientRPC();

		co_await Squid::WaitUntil([Actor]()
			{
				return Actor->bWasServerRpcCalled;
			});

		Actor->bWasServerRpcCalled = false;
		Actor->NetMulticastRPC();

		UNTEST_EXPECT_TRUE(Actor->bWasMulticastCalled);

		co_await Squid::WaitUntil([Actor]()
			{
				return Actor->bWasServerRpcCalled;
			});
	}

	if (UNTEST_IS_CLIENT())
	{
		// yield until the actor replicated from the server world
		co_await Squid::WaitUntil([World]()
			{
				for (TActorIterator<AUntestExamplePlayerController> It(World); It; ++It)
				{
					if (AActor* WorldActor = *It)
					{
						return true;
					}
				}
				return false;
			});

		AUntestExamplePlayerController* Actor = nullptr;
		for (TActorIterator<AUntestExamplePlayerController> It(World); It; ++It)
		{
			if (AUntestExamplePlayerController* WorldActor = *It)
			{
				Actor = WorldActor;
				break;
			}
		}
		UNTEST_ASSERT_PTR(Actor);

		Actor->ServerRPC();

		co_await Squid::WaitUntil([Actor]()
			{
				return Actor->bWasOnRepCalled;
			});

		UNTEST_EXPECT_EQ(Actor->ReplicatedInt, 1337);
		Actor->ServerRPC();

		co_await Squid::WaitUntil([Actor]()
			{
				return Actor->bWasClientRpcCalled;
			});

		Actor->ServerRPC();

		co_await Squid::WaitUntil([Actor]()
			{
				return Actor->bWasMulticastCalled;
			});

		Actor->ServerRPC();
	}

	co_return;
}
