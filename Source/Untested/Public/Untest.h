#pragma once

#if !UE_EDITOR
#error Untest code not enabled in non-editor builds.
#endif

#include "Containers/StaticArray.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "CommonLocalPlayer.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "SceneView.h"
#include "SquidTasks/Task.h"
#include "SquidTasks/TaskManager.h"
#include "UObject/Package.h"
#include "Untest.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogUntest, Display, All);

// For sorting in UI
// TODO move to module header
UENUM()
enum class EUntestTypeFlags : uint32
{
	None = 0x00,
	Unit = 0x01,
	World = 0x02,
	ClientServer = 0x04,
	All = Unit | World | ClientServer,
};

ENUM_CLASS_FLAGS(EUntestTypeFlags)

UENUM()
enum class EUntestFlags : uint32
{
	None = 0x00,
	Disabled = 0x01,				// Will be skipped in all test runs
	Pure = 0x02,					// Has no side effects - can be run multithreaded
	RequiresExternalService = 0x04, // Requires an external service; skipped if PreflightCheck fails
};

ENUM_CLASS_FLAGS(EUntestFlags)

struct FUntestOpts
{
	static constexpr EUntestFlags DefaultFlags = EUntestFlags::None;

	FUntestOpts()
		: TimeoutMs(0.0f), Flags(DefaultFlags) {}
	explicit FUntestOpts(float InTimeoutMs)
		: TimeoutMs(InTimeoutMs), Flags(DefaultFlags) {}
	explicit FUntestOpts(EUntestFlags InFlags)
		: TimeoutMs(0.0f), Flags(InFlags) {}
	FUntestOpts(float InTimeoutMs, EUntestFlags InFlags)
		: TimeoutMs(InTimeoutMs), Flags(InFlags) {}

	bool IsSet(EUntestFlags InFlags) const { return (Flags & InFlags) != EUntestFlags::None; }

	float TimeoutMs;
	EUntestFlags Flags;
};

using UntestTask = Squid::Task<>;

///////////////////////////////////////////////////////////////////////////////////////////////////
// Untest implementation details

UCLASS()
class UUntestGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	void SetWorldContext(FWorldContext* InWorldContext) { WorldContext = InWorldContext; }
	void SetAllowRemoveLocalPlayer(bool bAllow) { bAllowRemoveLocalPlayer = bAllow; }

	// Prevent CleanupGameViewport() from removing test LocalPlayers when their
	// ViewportClient has no real Viewport (UE checks !ViewportClient->Viewport).
	// During explicit teardown, SetAllowRemoveLocalPlayer(true) enables removal.
	virtual bool RemoveLocalPlayer(ULocalPlayer* ExistingPlayer) override
	{
		if (!bAllowRemoveLocalPlayer)
		{
			return false;
		}
		return Super::RemoveLocalPlayer(ExistingPlayer);
	}

private:
	bool bAllowRemoveLocalPlayer = false;
};

UCLASS()
class UUntestViewportClient : public UGameViewportClient
{
	GENERATED_BODY()

public:
	void InitForTest(UWorld* InWorld, UGameInstance* InGameInstance)
	{
		World = InWorld;
		GameInstance = InGameInstance;
	}

	// Slate is not available in commandlet mode, so skip the base implementations that call FSlateApplication::Get().
	virtual void NotifyPlayerAdded(int32 PlayerIndex, ULocalPlayer* AddedPlayer) override {}
	virtual void NotifyPlayerRemoved(int32 PlayerIndex, ULocalPlayer* RemovedPlayer) override {}
	virtual void DetachViewportClient() override {}
};

// PlayerController safe for commandlet mode where FSlateApplication may not be initialized.
// SVirtualJoystick::ShouldDisplayTouchInterface() unconditionally calls FSlateApplication::Get().
UCLASS()
class AUntestPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void CreateTouchInterface() override
	{
		if (!FSlateApplication::IsInitialized())
		{
			return;
		}
		Super::CreateTouchInterface();
	}
};

// Default game mode for client-server tests. Uses AUntestPlayerController to avoid
// Slate assertions in commandlet mode.
UCLASS()
class AUntestGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AUntestGameMode()
	{
		PlayerControllerClass = AUntestPlayerController::StaticClass();
	}
};

struct FUntestFixture;
struct FUntestLineContext;

namespace EUntestWorldType
{
	enum Enum
	{
		Server,
		Client,
		Count,
	};
} // namespace EUntestWorldType

struct UNTESTED_API FUntestName
{
	FString Module;
	FString Category;
	FString Test;

	FString ToFull() const
	{
		return FString::Printf(TEXT("%s.%s.%s"), *Module, *Category, *Test);
	}

	static FUntestName FromFull(const FString& FullName)
	{
		FUntestName Name;
		TArray<FString> Parts;
		FullName.ParseIntoArray(Parts, TEXT("."));
		if (Parts.Num() >= 1)
			Name.Module = Parts[0];
		if (Parts.Num() >= 2)
			Name.Category = Parts[1];
		if (Parts.Num() >= 3)
			Name.Test = Parts[2];
		return Name;
	}
};

struct UNTESTED_API FUntestContext
{
public:
	const FUntestName& GetName() const { return TestName; }

	void AddError(FString Error);
	template <typename T1, typename T2>
	bool Eq(const FUntestLineContext& LineContext, const T1& A, const T2& B);
	template <typename T>
	bool Near(const FUntestLineContext& LineContext, T A, T B, T Epsilon);
	template <typename T1, typename T2>
	bool Ne(const FUntestLineContext& LineContext, const T1& A, const T2& B);
	template <typename T1, typename T2>
	bool StrEq(const FUntestLineContext& LineContext, const T1& A, const T2& B, ESearchCase::Type SearchCase);
	template <typename T1, typename T2>
	bool StrNe(const FUntestLineContext& LineContext, const T1& A, const T2& B, ESearchCase::Type SearchCase);
	template <typename T1, typename T2>
	bool Gt(const FUntestLineContext& LineContext, const T1& A, const T2& B);
	template <typename T1, typename T2>
	bool Ge(const FUntestLineContext& LineContext, const T1& A, const T2& B);
	template <typename T1, typename T2>
	bool Lt(const FUntestLineContext& LineContext, const T1& A, const T2& B);
	template <typename T1, typename T2>
	bool Le(const FUntestLineContext& LineContext, const T1& A, const T2& B);
	template <typename T>
	bool ValidPtr(const FUntestLineContext& LineContext, const T* A);
	template <typename T>
	bool Nullptr(const FUntestLineContext& LineContext, const T* A);
	template <typename T>
	bool IsValid(const FUntestLineContext& LineContext, const T& A);
	template <typename T>
	bool IsInvalid(const FUntestLineContext& LineContext, const T& A);
	bool IsTrue(const FUntestLineContext& LineContext, bool A);
	bool IsFalse(const FUntestLineContext& LineContext, bool A);

	UGameInstance* GetGameInstance(EUntestWorldType::Enum Type);
	UWorld* GetWorld(EUntestWorldType::Enum Type);

	/** Returns client world by index. Index 0 == Worlds[EUntestWorldType::Client]; 1..N-1 == ExtraClientWorlds[Index-1]. Returns nullptr for out-of-range indices. */
	UWorld* GetClientWorld(int32 Index);

private:
	FUntestName TestName;
	double TimeoutMs = 0.0;

	FUntestFixture* Fixture = nullptr;
	Squid::WeakTaskHandle Task;
	TUniquePtr<Squid::TaskManager> TaskManager;
	double TimestampBegin = 0.0;
	double TimestampEnd = 0.0;
	TArray<FString> Errors;

	// Only used for World and ClientServer tests
	TStaticArray<TWeakObjectPtr<UPackage>, EUntestWorldType::Count> Packages;
	TStaticArray<TWeakObjectPtr<UGameInstance>, EUntestWorldType::Count> GameInstances;
	TStaticArray<TWeakObjectPtr<UWorld>, EUntestWorldType::Count> Worlds;
	TArray<TWeakObjectPtr<UObject>> Objects;

	// Additional client slots for N>=2; index 0 mirrors Worlds[EUntestWorldType::Client].
	// Empty for the N==1 default. Parallel arrays so ExtraClient*[i] all describe client (i+1).
	TArray<TWeakObjectPtr<UPackage>> ExtraClientPackages;
	TArray<TWeakObjectPtr<UGameInstance>> ExtraClientGameInstances;
	TArray<TWeakObjectPtr<UWorld>> ExtraClientWorlds;

	friend class FUntestModule;
	friend struct FUntestFixture;
	friend struct FUntestWorldFixture;
	friend struct FUntestClientServerFixture;
};

struct UNTESTED_API FUntestFixtureFactory
{
public:
	FUntestFixtureFactory(FString InModuleName, FString InCategoryName, FString InTestName, EUntestTypeFlags TestType, float DefaultTimeout, FUntestOpts InOpts);
	virtual ~FUntestFixtureFactory();

	const FUntestName& GetName() const { return Name; }
	const FUntestOpts& GetOpts() const { return Opts; }
	EUntestTypeFlags GetType() const { return TestType; }

	// Derived fixtures override these
	virtual TSharedPtr<FUntestFixture> New(const TSharedPtr<FUntestContext>& FixtureContext) const = 0;

private:
	FUntestName Name;
	EUntestTypeFlags TestType;
	FUntestOpts Opts;
};

template <typename T>
struct TUntestFixtureFactory : public FUntestFixtureFactory
{
public:
	TUntestFixtureFactory(FString InModuleName, FString InCategoryName, FString InTestName, EUntestTypeFlags TestType, float DefaultTimeout, FUntestOpts Opts);
	virtual TSharedPtr<FUntestFixture> New(const TSharedPtr<FUntestContext>& FixtureContext) const override;
};

struct FUntestLineContext
{
	FUntestLineContext(const TCHAR* InFile, int32 InLine, const TCHAR* InLhs, const TCHAR* InRhs, bool bInIsAssert)
		: File(InFile)
		, Lhs(InLhs)
		, Rhs(InRhs)
		, Line(InLine)
		, bIsAssert(bInIsAssert)
	{
	}

	const TCHAR* File;
	const TCHAR* Lhs;
	const TCHAR* Rhs;
	int32 Line;
	bool bIsAssert;
};

template <typename T, typename = typename TEnableIf<!TIsCharType<T>::Value>::Type>
inline FString LexToString(const T* Value)
{
	return FString::Printf(TEXT("%p"), Value);
}

namespace Impl
{
	template <typename T, typename Enable = void>
	inline FString ValueToString(const T& Value)
	{
		return FString();
	}

#define DEFINE_VALUETOSTR_ARITHMETIC_FUNC(Type, CastType, Str)                     \
	template <>                                                                    \
	inline FString ValueToString(const Type& Value)                                \
	{                                                                              \
		return FString::Printf(TEXT(" (" Str ") "), static_cast<CastType>(Value)); \
	}

	// clang-format off
	DEFINE_VALUETOSTR_ARITHMETIC_FUNC(float,    float,    "%f")
	DEFINE_VALUETOSTR_ARITHMETIC_FUNC(double,   double,   "%f")
	DEFINE_VALUETOSTR_ARITHMETIC_FUNC(uint8,    uint32,   "%u")
	DEFINE_VALUETOSTR_ARITHMETIC_FUNC(uint16,   uint32,   "%u")
	DEFINE_VALUETOSTR_ARITHMETIC_FUNC(uint32,   uint32,   "%u")
	DEFINE_VALUETOSTR_ARITHMETIC_FUNC(uint64,   uint64,   "%lu")
	DEFINE_VALUETOSTR_ARITHMETIC_FUNC(int8,     int32,    "%d")
	DEFINE_VALUETOSTR_ARITHMETIC_FUNC(int16,    int32,    "%d")
	DEFINE_VALUETOSTR_ARITHMETIC_FUNC(int32,    int32,    "%d")
	DEFINE_VALUETOSTR_ARITHMETIC_FUNC(int64,    int64,    "%ld")
	DEFINE_VALUETOSTR_ARITHMETIC_FUNC(long,     long,     "%d")
	template <> inline FString ValueToString(const WIDECHAR& Value) { return FString(TEXT(" (")) + FString::Chr(Value) + TEXT(") "); }
	template <> inline FString ValueToString(const ANSICHAR& Value) { return FString::Printf(TEXT(" (%c) "), Value); }
	// clang-format on

#undef DEFINE_VALUETOSTR_ARITHMETIC_FUNC

	template <typename T>
	inline FString ValueToString(const T* Value)
	{
		return Value ? FString::Printf(TEXT(" (%p) "), Value) : ValueToString(nullptr);
	}

	template <>
	inline FString ValueToString(const nullptr_t& Value)
	{
		return TEXT(" (nullptr) ");
	}

	template <>
	inline FString ValueToString(const bool& Value)
	{
		return FString::Printf(TEXT(" (%s) "), *LexToString(Value));
	}

	template <>
	inline FString ValueToString(const FString& Value)
	{
		return FString::Printf(TEXT(" (%s) "), *Value);
	}

	template <typename T1, typename T2>
	inline bool EqualityHelper(const T1& A, const T2& B)
	{
		return A == B;
	}

	template <typename CharType>
	inline bool StrEqualityHelper(const CharType* A, const CharType* B, ESearchCase::Type SearchCase)
	{
		if (SearchCase == ESearchCase::CaseSensitive)
		{
			return TCString<CharType>::Strcmp(A, B) == 0;
		}
		else // ESearchCase::IgnoreCase
		{
			return TCString<CharType>::Stricmp(A, B) == 0;
		}
	}

	inline bool StrEqualityHelper(const TCHAR* A, const FString& B, ESearchCase::Type SearchCase)
	{
		return StrEqualityHelper(A, *B, SearchCase);
	}

	inline bool StrEqualityHelper(const FString& A, const TCHAR* B, ESearchCase::Type SearchCase)
	{
		return StrEqualityHelper(*A, B, SearchCase);
	}

	inline bool StrEqualityHelper(const FString& A, const FString& B, ESearchCase::Type SearchCase)
	{
		return StrEqualityHelper(*A, *B, SearchCase);
	}

} // namespace Impl

inline void FUntestContext::AddError(FString Error)
{
	Errors.Emplace(MoveTemp(Error));
}

template <typename T1, typename T2>
bool FUntestContext::Eq(const FUntestLineContext& LineContext, const T1& A, const T2& B)
{
	if (Impl::EqualityHelper<T1, T2>(A, B))
	{
		return true;
	}

	const TCHAR* Prefix = LineContext.bIsAssert ? TEXT("Assert") : TEXT("Expect");
	FString AStr = Impl::ValueToString(A);
	FString BStr = Impl::ValueToString(B);
	Errors.Emplace(FString::Printf(TEXT("(%s:%d) %s failed: %s%s == %s%s"), LineContext.File, LineContext.Line, Prefix, LineContext.Lhs, *AStr, LineContext.Rhs, *BStr));
	return false;
}

template <typename T>
bool FUntestContext::Near(const FUntestLineContext& LineContext, T A, T B, T Epsilon)
{
	checkf(Epsilon >= T(0), TEXT("Epsilon must not be negative, but got %s"), *Impl::ValueToString(Epsilon));

	if (FMath::Abs(A - B) < Epsilon)
	{
		return true;
	}

	const TCHAR* Prefix = LineContext.bIsAssert ? TEXT("Assert") : TEXT("Expect");
	FString AStr = Impl::ValueToString(A);
	FString BStr = Impl::ValueToString(B);
	FString ToleranceStr = Impl::ValueToString(Epsilon);
	Errors.Emplace(FString::Printf(TEXT("(%s:%d) %s failed: %s%s ≈≈ %s%s (outside epsilon %s)"), LineContext.File, LineContext.Line, Prefix, LineContext.Lhs, *AStr, LineContext.Rhs, *BStr, *ToleranceStr));
	return false;
}

template <typename T1, typename T2>
bool FUntestContext::Ne(const FUntestLineContext& LineContext, const T1& A, const T2& B)
{
	if (!Impl::EqualityHelper(A, B))
	{
		return true;
	}

	const TCHAR* Prefix = LineContext.bIsAssert ? TEXT("Assert") : TEXT("Expect");
	FString AStr = Impl::ValueToString(A);
	FString BStr = Impl::ValueToString(B);
	Errors.Emplace(FString::Printf(TEXT("(%s:%d) %s failed: %s%s != %s%s"), LineContext.File, LineContext.Line, Prefix, LineContext.Lhs, *AStr, LineContext.Rhs, *BStr));
	return false;
}

template <typename T1, typename T2>
bool FUntestContext::Gt(const FUntestLineContext& LineContext, const T1& A, const T2& B)
{
	if (A > B)
	{
		return true;
	}

	const TCHAR* Prefix = LineContext.bIsAssert ? TEXT("Assert") : TEXT("Expect");
	FString AStr = Impl::ValueToString(A);
	FString BStr = Impl::ValueToString(B);
	Errors.Emplace(FString::Printf(TEXT("(%s:%d) %s failed: %s%s > %s%s"), LineContext.File, LineContext.Line, Prefix, LineContext.Lhs, *AStr, LineContext.Rhs, *BStr));
	return false;
}

template <typename T1, typename T2>
bool FUntestContext::StrEq(const FUntestLineContext& LineContext, const T1& A, const T2& B, ESearchCase::Type SearchCase)
{
	if (Impl::StrEqualityHelper(A, B, SearchCase))
	{
		return true;
	}

	const TCHAR* Prefix = LineContext.bIsAssert ? TEXT("Assert") : TEXT("Expect");
	const TCHAR* SearchCaseStr = (SearchCase == ESearchCase::CaseSensitive) ? TEXT("case sensitive") : TEXT("case insensitive");
	FString AStr = Impl::ValueToString(A);
	FString BStr = Impl::ValueToString(B);
	Errors.Emplace(FString::Printf(TEXT("(%s:%d) %s failed: %s%s != %s%s (%s)"), LineContext.File, LineContext.Line, Prefix, LineContext.Lhs, *AStr, LineContext.Rhs, *BStr, SearchCaseStr));
	return false;
}

template <typename T1, typename T2>
bool FUntestContext::StrNe(const FUntestLineContext& LineContext, const T1& A, const T2& B, ESearchCase::Type SearchCase)
{
	if (!Impl::StrEqualityHelper(A, B, SearchCase))
	{
		return true;
	}

	const TCHAR* Prefix = LineContext.bIsAssert ? TEXT("Assert") : TEXT("Expect");
	const TCHAR* SearchCaseStr = (SearchCase == ESearchCase::CaseSensitive) ? TEXT("case sensitive") : TEXT("case insensitive");
	FString AStr = Impl::ValueToString(A);
	FString BStr = Impl::ValueToString(B);
	Errors.Emplace(FString::Printf(TEXT("(%s:%d) %s failed: %s%s != %s%s (%s)"), LineContext.File, LineContext.Line, Prefix, LineContext.Lhs, *AStr, LineContext.Rhs, *BStr, SearchCaseStr));
	return false;
}

template <typename T1, typename T2>
bool FUntestContext::Ge(const FUntestLineContext& LineContext, const T1& A, const T2& B)
{
	if (A >= B)
	{
		return true;
	}

	const TCHAR* Prefix = LineContext.bIsAssert ? TEXT("Assert") : TEXT("Expect");
	FString AStr = Impl::ValueToString(A);
	FString BStr = Impl::ValueToString(B);
	Errors.Emplace(FString::Printf(TEXT("(%s:%d) %s failed: %s%s >= %s%s"), LineContext.File, LineContext.Line, Prefix, LineContext.Lhs, *AStr, LineContext.Rhs, *BStr));
	return false;
}

template <typename T1, typename T2>
bool FUntestContext::Lt(const FUntestLineContext& LineContext, const T1& A, const T2& B)
{
	if (A < B)
	{
		return true;
	}

	const TCHAR* Prefix = LineContext.bIsAssert ? TEXT("Assert") : TEXT("Expect");
	FString AStr = Impl::ValueToString(A);
	FString BStr = Impl::ValueToString(B);
	Errors.Emplace(FString::Printf(TEXT("(%s:%d) %s failed: %s%s >= %s%s"), LineContext.File, LineContext.Line, Prefix, LineContext.Lhs, *AStr, LineContext.Rhs, *BStr));
	return false;
}

template <typename T1, typename T2>
bool FUntestContext::Le(const FUntestLineContext& LineContext, const T1& A, const T2& B)
{
	if (A <= B)
	{
		return true;
	}

	const TCHAR* Prefix = LineContext.bIsAssert ? TEXT("Assert") : TEXT("Expect");
	FString AStr = Impl::ValueToString(A);
	FString BStr = Impl::ValueToString(B);
	Errors.Emplace(FString::Printf(TEXT("(%s:%d) %s failed: %s%s >= %s%s"), LineContext.File, LineContext.Line, Prefix, LineContext.Lhs, *AStr, LineContext.Rhs, *BStr));
	return false;
}

inline bool FUntestContext::IsTrue(const FUntestLineContext& LineContext, bool A)
{
	if (A)
	{
		return true;
	}

	const TCHAR* Prefix = LineContext.bIsAssert ? TEXT("Assert") : TEXT("Expect");
	Errors.Emplace(FString::Printf(TEXT("(%s:%d) %s failed: %s should be true, but is false"), LineContext.File, LineContext.Line, Prefix, LineContext.Lhs));
	return false;
}

inline bool FUntestContext::IsFalse(const FUntestLineContext& LineContext, bool A)
{
	if (A == false)
	{
		return true;
	}

	const TCHAR* Prefix = LineContext.bIsAssert ? TEXT("Assert") : TEXT("Expect");
	Errors.Emplace(FString::Printf(TEXT("(%s:%d) %s failed: %s should be false, but is true"), LineContext.File, LineContext.Line, Prefix, LineContext.Lhs));
	return false;
}

template <typename T>
bool FUntestContext::ValidPtr(const FUntestLineContext& LineContext, const T* A)
{
	if (A != nullptr)
	{
		return true;
	}

	const TCHAR* Prefix = LineContext.bIsAssert ? TEXT("Assert") : TEXT("Expect");
	Errors.Emplace(FString::Printf(TEXT("(%s:%d) %s failed: %s is nullptr, but should be a valid pointer"), LineContext.File, LineContext.Line, Prefix, LineContext.Lhs));
	return false;
}

template <typename T>
bool FUntestContext::Nullptr(const FUntestLineContext& LineContext, const T* A)
{
	if (A == nullptr)
	{
		return true;
	}

	const TCHAR* Prefix = LineContext.bIsAssert ? TEXT("Assert") : TEXT("Expect");
	Errors.Emplace(FString::Printf(TEXT("(%s:%d) %s failed: %s should be nullptr, but is a valid pointer (%p)"), LineContext.File, LineContext.Line, Prefix, LineContext.Lhs, A));
	return false;
}

template <typename T>
bool FUntestContext::IsValid(const FUntestLineContext& LineContext, const T& A)
{
	if (A.IsValid())
	{
		return true;
	}

	const TCHAR* Prefix = LineContext.bIsAssert ? TEXT("Assert") : TEXT("Expect");
	Errors.Emplace(FString::Printf(TEXT("(%s:%d) %s failed: %s.IsValid() should be true, but is false"), LineContext.File, LineContext.Line, Prefix, LineContext.Lhs));
	return false;
}

template <typename T>
bool FUntestContext::IsInvalid(const FUntestLineContext& LineContext, const T& A)
{
	if (!A.IsValid())
	{
		return true;
	}

	const TCHAR* Prefix = LineContext.bIsAssert ? TEXT("Assert") : TEXT("Expect");
	Errors.Emplace(FString::Printf(TEXT("(%s:%d) %s failed: %s.IsValid() should be false, but is true"), LineContext.File, LineContext.Line, Prefix, LineContext.Lhs));
	return false;
}

inline UGameInstance* FUntestContext::GetGameInstance(EUntestWorldType::Enum Type)
{
	return GameInstances[static_cast<int32>(Type)].Get();
}

inline UWorld* FUntestContext::GetWorld(EUntestWorldType::Enum Type)
{
	return Worlds[static_cast<int32>(Type)].Get();
}

inline UWorld* FUntestContext::GetClientWorld(int32 Index)
{
	if (Index < 0)
	{
		return nullptr;
	}
	if (Index == 0)
	{
		return Worlds[static_cast<int32>(EUntestWorldType::Client)].Get();
	}
	const int32 ExtraIndex = Index - 1;
	if (!ExtraClientWorlds.IsValidIndex(ExtraIndex))
	{
		return nullptr;
	}
	return ExtraClientWorlds[ExtraIndex].Get();
}

#define UNTEST_IMPL_NAME(Module, Category, TestName, FixtureType) Module##Category##TestName##_TestFixture

#define UNTEST_UNIT_IMPL_FIXTURE(Module, Category, TestName, FixtureType, Opts)                                                                        \
	struct UNTEST_IMPL_NAME(Module, Category, TestName, _TestFixture) : public FixtureType                                                             \
	{                                                                                                                                                  \
		static_assert(TIsDerivedFrom<FixtureType, FUntestUnitFixture>::IsDerived, "Only fixtures inheriting from FUntestUnitFixture are allowed");     \
		virtual UntestTask Run(FUntestContext& TestContext) override;                                                                                  \
	};                                                                                                                                                 \
	TUntestFixtureFactory<UNTEST_IMPL_NAME(Module, Category, TestName, _TestFixture)> Module##Category##TestName##_TestFixtureFactory =                \
		TUntestFixtureFactory<UNTEST_IMPL_NAME(Module, Category, TestName, _TestFixture)>(                                                             \
			TEXT(#Module), TEXT(#Category), TEXT(#TestName), FixtureType::TestType(), FixtureType::DefaultTimeoutMs(), Opts);                          \
	UntestTask UNTEST_IMPL_NAME(Module, Category, TestName, _TestFixture)::Run(FUntestContext& TestContext)

#define UNTEST_WORLD_IMPL_FIXTURE(Module, Category, TestName, FixtureType, Opts)                                                                       \
	struct UNTEST_IMPL_NAME(Module, Category, TestName, _TestFixture) : public FixtureType                                                             \
	{                                                                                                                                                  \
		static_assert(TIsDerivedFrom<FixtureType, FUntestWorldFixture>::IsDerived, "Only fixtures inheriting from FUntestWorldFixture are allowed");   \
		virtual UntestTask Run(FUntestContext& TestContext, const EUntestWorldType::Enum _WorldType) override;                                         \
	};                                                                                                                                                 \
	TUntestFixtureFactory<UNTEST_IMPL_NAME(Module, Category, TestName, _TestFixture)> Module##Category##TestName##_TestFixtureFactory =                \
		TUntestFixtureFactory<UNTEST_IMPL_NAME(Module, Category, TestName, _TestFixture)>(                                                             \
			TEXT(#Module), TEXT(#Category), TEXT(#TestName), FixtureType::TestType(), FixtureType::DefaultTimeoutMs(), Opts);                          \
	UntestTask UNTEST_IMPL_NAME(Module, Category, TestName, _TestFixture)::Run(FUntestContext& TestContext, const EUntestWorldType::Enum _WorldType)

#define UNTEST_CLIENTSERVER_IMPL_FIXTURE(Module, Category, TestName, FixtureType, Opts)                                                                                \
	struct UNTEST_IMPL_NAME(Module, Category, TestName, _TestFixture) : public FixtureType                                                                             \
	{                                                                                                                                                                  \
		static_assert(TIsDerivedFrom<FixtureType, FUntestClientServerFixture>::IsDerived, "Only fixtures inheriting from FUntestClientServerFixture are allowed");     \
		virtual UntestTask Run(FUntestContext& TestContext, const EUntestWorldType::Enum _WorldType) override;                                                         \
	};                                                                                                                                                                 \
	TUntestFixtureFactory<UNTEST_IMPL_NAME(Module, Category, TestName, _TestFixture)> Module##Category##TestName##_TestFixtureFactory =                                \
		TUntestFixtureFactory<UNTEST_IMPL_NAME(Module, Category, TestName, _TestFixture)>(                                                                             \
			TEXT(#Module), TEXT(#Category), TEXT(#TestName), FixtureType::TestType(), FixtureType::DefaultTimeoutMs(), Opts);                                          \
	UntestTask UNTEST_IMPL_NAME(Module, Category, TestName, _TestFixture)::Run(FUntestContext& TestContext, const EUntestWorldType::Enum _WorldType)

#define UNTEST_LINE_CONTEXT(A, B, bIsAssert) (FUntestLineContext(TEXT(__FILE__), __LINE__, TEXT(A), TEXT(B), bIsAssert))
#define UNTEST_LINE_CONTEXT_EXPECT(A, B) UNTEST_LINE_CONTEXT(A, B, false)
#define UNTEST_LINE_CONTEXT_ASSERT(A, B) UNTEST_LINE_CONTEXT(A, B, true)

///////////////////////////////////////////////////////////////////////////////////////////////////
// Convenience macros to use with the _OPTS versions of the test declaration macros
#define UNTEST_TIMEOUTMS(DurationMs) (FUntestOpts(static_cast<float>(DurationMs)))
#define UNTEST_FLAGS(Flags) (FUntestOpts(static_cast<EUntestFlags>(Flags)))
#define UNTEST_TIMEOUTMS_FLAGS(DurationMs, Flags) (FUntestOpts((DurationMs), static_cast<EUntestFlags>(Flags)))
#define UNTEST_DISABLED() (FUntestOpts(EUntestFlags::Disabled))
#define UNTEST_PURE() (FUntestOpts(EUntestFlags::Pure))

///////////////////////////////////////////////////////////////////////////////////////////////////
// Declare tests using these macros.
// For example:
//
// UNTEST_UNIT(ModuleName, SuiteName, TestName)
// {
//     EXPECT_EQ(4, 2 + 2);
//     EXPECT_NE(5, 2 + 2);
//     co_return;
// }
//
// UNTEST_CLIENTSERVER(ModuleName, SuiteName, TestName)
// {
//     EXPECT_NE(UNTEST_GET_GAMEINSTANCE(), nullptr);
//     EXPECT_NE(UNTEST_GET_WORLD(), nullptr);
//     co_return;
// }

#define UNTEST_UNIT(Module, Category, TestName) UNTEST_UNIT_IMPL_FIXTURE(Module, Category, TestName, FUntestUnitFixture, FUntestOpts())
#define UNTEST_UNIT_OPTS(Module, Category, TestName, Opts) UNTEST_UNIT_IMPL_FIXTURE(Module, Category, TestName, FUntestUnitFixture, Opts)
#define UNTEST_UNIT_PURE(Module, Category, TestName) UNTEST_UNIT_IMPL_FIXTURE(Module, Category, TestName, FUntestUnitFixture, UNTEST_PURE())
#define UNTEST_UNIT_F(FixtureType, Module, Category, TestName) UNTEST_UNIT_IMPL_FIXTURE(Module, Category, TestName, FixtureType, FUntestOpts())
#define UNTEST_UNIT_F_OPTS(FixtureType, Module, Category, TestName, Opts) UNTEST_UNIT_IMPL_FIXTURE(Module, Category, TestName, FixtureType, Opts)

#define UNTEST_WORLD(Module, Category, TestName) UNTEST_WORLD_IMPL_FIXTURE(Module, Category, TestName, FUntestWorldFixture, FUntestOpts())
#define UNTEST_WORLD_OPTS(Module, Category, TestName, Opts) UNTEST_WORLD_IMPL_FIXTURE(Module, Category, TestName, FUntestWorldFixture, Opts)
#define UNTEST_WORLD_F(FixtureType, Module, Category, TestName) UNTEST_WORLD_IMPL_FIXTURE(Module, Category, TestName, FixtureType, FUntestOpts())
#define UNTEST_WORLD_F_OPTS(FixtureType, Module, Category, TestName, Opts) UNTEST_WORLD_IMPL_FIXTURE(Module, Category, TestName, FixtureType, Opts)

#define UNTEST_CLIENTSERVER(Module, Category, TestName) UNTEST_CLIENTSERVER_IMPL_FIXTURE(Module, Category, TestName, FUntestClientServerFixture, FUntestOpts())
#define UNTEST_CLIENTSERVER_OPTS(Module, Category, TestName, Opts) UNTEST_CLIENTSERVER_IMPL_FIXTURE(Module, Category, TestName, FUntestClientServerFixture, Opts)
#define UNTEST_CLIENTSERVER_F(FixtureType, Module, Category, TestName) UNTEST_CLIENTSERVER_IMPL_FIXTURE(Module, Category, TestName, FixtureType, FUntestOpts())
#define UNTEST_CLIENTSERVER_F_OPTS(FixtureType, Module, Category, TestName, Opts) UNTEST_CLIENTSERVER_IMPL_FIXTURE(Module, Category, TestName, FixtureType, Opts)

///////////////////////////////////////////////////////////////////////////////////////////////////
// Expects fail a test, but don't cause it to exit immediately. Use for non-fatal errors.

#define UNTEST_EXPECT_EQ(Actual, Expected) (void)TestContext.Eq(UNTEST_LINE_CONTEXT_EXPECT(#Actual, #Expected), Actual, Expected)
#define UNTEST_EXPECT_NE(Actual, Expected) (void)TestContext.Ne(UNTEST_LINE_CONTEXT_EXPECT(#Actual, #Expected), Actual, Expected)
#define UNTEST_EXPECT_NEAR(Actual, Expected, Tolerance) (void)TestContext.Near(UNTEST_LINE_CONTEXT_EXPECT(#Actual, #Expected), Actual, Expected, Tolerance)
#define UNTEST_EXPECT_STREQ(Actual, Expected) (void)TestContext.StrEq(UNTEST_LINE_CONTEXT_EXPECT(#Actual, #Expected), Actual, Expected, ESearchCase::CaseSensitive)
#define UNTEST_EXPECT_STRNE(Actual, Expected) (void)TestContext.StrNe(UNTEST_LINE_CONTEXT_EXPECT(#Actual, #Expected), Actual, Expected, ESearchCase::CaseSensitive)
#define UNTEST_EXPECT_STRCASEEQ(Actual, Expected) (void)TestContext.StrEq(UNTEST_LINE_CONTEXT_EXPECT(#Actual, #Expected), Actual, Expected, ESearchCase::IgnoreCase)
#define UNTEST_EXPECT_STRCASENE(Actual, Expected) (void)TestContext.StrNe(UNTEST_LINE_CONTEXT_EXPECT(#Actual, #Expected), Actual, Expected, ESearchCase::IgnoreCase)
#define UNTEST_EXPECT_GT(A, B) (void)TestContext.Gt(UNTEST_LINE_CONTEXT_EXPECT(#A, #B), A, B)
#define UNTEST_EXPECT_GE(A, B) (void)TestContext.Ge(UNTEST_LINE_CONTEXT_EXPECT(#A, #B), A, B)
#define UNTEST_EXPECT_LT(A, B) (void)TestContext.Lt(UNTEST_LINE_CONTEXT_EXPECT(#A, #B), A, B)
#define UNTEST_EXPECT_LE(A, B) (void)TestContext.Le(UNTEST_LINE_CONTEXT_EXPECT(#A, #B), A, B)
#define UNTEST_EXPECT_TRUE(A) (void)TestContext.IsTrue(UNTEST_LINE_CONTEXT_EXPECT(#A, "none"), A)
#define UNTEST_EXPECT_FALSE(A) (void)TestContext.IsFalse(UNTEST_LINE_CONTEXT_EXPECT(#A, "none"), A)
#define UNTEST_EXPECT_PTR(A) (void)TestContext.ValidPtr(UNTEST_LINE_CONTEXT_EXPECT(#A, "none"), A)
#define UNTEST_EXPECT_NULLPTR(A) (void)TestContext.Nullptr(UNTEST_LINE_CONTEXT_EXPECT(#A, "none"), A)
#define UNTEST_EXPECT_VALID(A) (void)TestContext.IsValid(UNTEST_LINE_CONTEXT_EXPECT(#A, "none"), A)
#define UNTEST_EXPECT_INVALID(A) (void)TestContext.IsInvalid(UNTEST_LINE_CONTEXT_EXPECT(#A, "none"), A)

///////////////////////////////////////////////////////////////////////////////////////////////////
// Asserts fail a test, and cause it to exit immediately. Use for fatal errors.

// clang-format off
#define UNTEST_ASSERT_EQ(Actual, Expected) do { if (!TestContext.Eq(UNTEST_LINE_CONTEXT_ASSERT(#Actual, #Expected), Actual, Expected)) co_return; } while (0)
#define UNTEST_ASSERT_NE(Actual, Expected) do { if (!TestContext.Ne(UNTEST_LINE_CONTEXT_ASSERT(#Actual, #Expected), Actual, Expected)) co_return; } while (0)
#define UNTEST_ASSERT_NEAR(Actual, Expected, Tolerance) do { if (!TestContext.Near(UNTEST_LINE_CONTEXT_ASSERT(#Actual, #Expected), Actual, Expected, Tolerance)) co_return; } while (0)
#define UNTEST_ASSERT_STREQ(Actual, Expected) do { if (!TestContext.StrEq(UNTEST_LINE_CONTEXT_ASSERT(#Actual, #Expected), Actual, Expected, ESearchCase::CaseSensitive)) co_return; } while (0)
#define UNTEST_ASSERT_STRNE(Actual, Expected) do { if (!TestContext.StrNe(UNTEST_LINE_CONTEXT_ASSERT(#Actual, #Expected), Actual, Expected, ESearchCase::CaseSensitive)) co_return; } while (0)
#define UNTEST_ASSERT_STRCASEEQ(Actual, Expected) do { if (!TestContext.StrEq(UNTEST_LINE_CONTEXT_ASSERT(#Actual, #Expected), Actual, Expected, ESearchCase::IgnoreCase)) co_return; } while (0)
#define UNTEST_ASSERT_STRCASENE(Actual, Expected) do { if (!TestContext.StrNe(UNTEST_LINE_CONTEXT_ASSERT(#Actual, #Expected), Actual, Expected, ESearchCase::IgnoreCase)) co_return; } while (0)
#define UNTEST_ASSERT_GT(A, B) do { if (!TestContext.Gt(UNTEST_LINE_CONTEXT_ASSERT(#A, #B), A, B)) co_return; } while (0)
#define UNTEST_ASSERT_GE(A, B) do { if (!TestContext.Ge(UNTEST_LINE_CONTEXT_ASSERT(#A, #B), A, B)) co_return; } while (0)
#define UNTEST_ASSERT_LT(A, B) do { if (!TestContext.Lt(UNTEST_LINE_CONTEXT_ASSERT(#A, #B), A, B)) co_return; } while (0)
#define UNTEST_ASSERT_LE(A, B) do { if (!TestContext.Le(UNTEST_LINE_CONTEXT_ASSERT(#A, #B), A, B)) co_return; } while (0)
#define UNTEST_ASSERT_TRUE(A) do { if (!TestContext.IsTrue(UNTEST_LINE_CONTEXT_ASSERT(#A, "none"), A)) co_return; } while (0)
#define UNTEST_ASSERT_FALSE(A) do { if (!TestContext.IsFalse(UNTEST_LINE_CONTEXT_ASSERT(#A, "none"), A)) co_return; } while (0)
#define UNTEST_ASSERT_PTR(A) do { if (!TestContext.ValidPtr(UNTEST_LINE_CONTEXT_ASSERT(#A, "none"), A)) co_return; } while (0)
#define UNTEST_ASSERT_NULLPTR(A) do { if (!TestContext.Nullptr(UNTEST_LINE_CONTEXT_ASSERT(#A, "none"), A)) co_return; } while (0)
#define UNTEST_ASSERT_VALID(A) do { if (!TestContext.IsValid(UNTEST_LINE_CONTEXT_ASSERT(#A, "none"), A)) co_return; } while (0)
#define UNTEST_ASSERT_INVALID(A) do { if (!TestContext.IsInvalid(UNTEST_LINE_CONTEXT_ASSERT(#A, "none"), A)) co_return; } while (0)

// clang-format on

///////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers for ClientServer tests

#define UNTEST_GET_GAMEINSTANCE() (TestContext.GetGameInstance(_WorldType))
#define UNTEST_GET_WORLD() (TestContext.GetWorld(_WorldType))
#define UNTEST_IS_CLIENT() (_WorldType == EUntestWorldType::Client)
#define UNTEST_IS_SERVER() (_WorldType == EUntestWorldType::Server)

///////////////////////////////////////////////////////////////////////////////////////////////////
// You can derive directly from these fixtures if you want to override startup/shutdown and add
// local data members. Specify tests for these fixtures with UNTEST_F().

struct UNTESTED_API FUntestFixture
{
public:
	virtual ~FUntestFixture() = default;

	// Called before Setup. Return false to skip with a reason.
	virtual bool PreflightCheck(FString& OutSkipReason) const { return true; }

	// For test framework only
	virtual UntestTask SetupFixture(const FString TestName);
	virtual UntestTask RunFixture(const FString TestName) = 0;
	virtual UntestTask TeardownFixture(const FString TestName);

	// Should only be called on creation by the factory
	void SetContext(TSharedPtr<FUntestContext> InContext);

	FUntestContext& GetContext() { return *FixtureContext; }

	// Derived fixtures override these functions
	virtual UntestTask Setup(FUntestContext& TestContext);
	virtual UntestTask Teardown(FUntestContext& TestContext);

private:
	TSharedPtr<FUntestContext> FixtureContext;

	friend struct FUntestUnitFixture;
	friend struct FUntestClientServerFixture;
};

struct UNTESTED_API FUntestUnitFixture : public FUntestFixture
{
public:
	static EUntestTypeFlags TestType() { return EUntestTypeFlags::Unit; }
	static float DefaultTimeoutMs() { return 0.5f; }

	// Derived fixtures may override these functions to inject code at each of these steps
	virtual UntestTask RunFixture(const FString TestName) override;

	// Internal usage only
	virtual UntestTask Run(FUntestContext& TestContext) = 0;
};

struct FUntestGameClasses
{
	TSubclassOf<UUntestGameInstance> GameInstanceClass;
	TSubclassOf<AGameModeBase> GameModeClass;
};

struct UNTESTED_API FUntestWorldFixture : public FUntestFixture
{
public:
	static EUntestTypeFlags TestType() { return EUntestTypeFlags::World; }
	static float DefaultTimeoutMs() { return 10000.0f; }

	virtual ~FUntestWorldFixture();

	// Override to customize GameInstance and GameMode classes
	virtual FUntestGameClasses GetGameClasses() const { return FUntestGameClasses(); }

	// Override to request LocalPlayer creation in the test world
	virtual bool ShouldCreateLocalPlayer() const { return false; }

	// Override to specify the LocalPlayer class to create
	virtual TSubclassOf<ULocalPlayer> GetLocalPlayerClass() const;

	// Derived fixtures may override these functions to inject code at each of these steps
	virtual UntestTask SetupFixture(const FString TestName) override;
	virtual UntestTask RunFixture(const FString TestName) override;
	virtual UntestTask TeardownFixture(const FString TestName) override;

	// Helper functions for making UObjects
	template <typename T>
	T* NewTestObject();

	// Internal usage only

	// _WorldType here for macro compatibility. In practice, it's always EUntestWorldType::Server.
	virtual UntestTask Run(FUntestContext& TestContext, const EUntestWorldType::Enum _WorldType) = 0;

	void TeardownWorld();
};

struct UNTESTED_API FUntestClientServerFixture : public FUntestFixture
{
	static EUntestTypeFlags TestType() { return EUntestTypeFlags::ClientServer; }
	static float DefaultTimeoutMs() { return 10000.0f; }

	virtual ~FUntestClientServerFixture();

	// Derived fixtures may override these functions to inject code at each of these steps
	virtual UntestTask SetupFixture(const FString TestName) override;
	virtual UntestTask TeardownFixture(const FString TestName) override;
	virtual UntestTask RunFixture(const FString TestName) override;

	// Internal usage only
	virtual FUntestGameClasses GetGameClasses() const { return FUntestGameClasses(); };
	virtual UntestTask Run(FUntestContext& TestContext, const EUntestWorldType::Enum _WorldType) = 0;

	// Override to opt into N-client topology. Default is 1 to preserve every existing caller.
	// _WorldType passed into Run() remains either Server or Client; Run() is invoked once per client
	// when N>=2, with the same _WorldType==Client and the per-iteration ClientTask consuming each.
	virtual int32 GetNumClients() const { return 1; }

	// For fixture usage only
	void TeardownClientServer();
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// Inline implementation

template <typename T>
TUntestFixtureFactory<T>::TUntestFixtureFactory(FString InModuleName, FString InCategoryName, FString InTestName, EUntestTypeFlags TestType, float DefaultTimeout, FUntestOpts InOpts)
	: FUntestFixtureFactory(InModuleName, InCategoryName, InTestName, TestType, DefaultTimeout, InOpts)
{
}

template <typename T>
TSharedPtr<FUntestFixture> TUntestFixtureFactory<T>::New(const TSharedPtr<FUntestContext>& FixtureContext) const
{
	TSharedPtr<FUntestFixture> Fixture = MakeShared<T>();
	Fixture->SetContext(FixtureContext);
	return Fixture;
}

template <typename T>
T* FUntestWorldFixture::NewTestObject()
{
	FUntestContext& Context = GetContext();
	UWorld* World = Context.GetWorld(EUntestWorldType::Server);
	UClass* ObjClass = T::StaticClass();
	FName UniqueName = MakeUniqueObjectName(World, ObjClass);

	const FString TestName = Context.GetName().ToFull();
	TStringBuilder<256> ObjName;
	ObjName.Appendf(TEXT("%s_%s"), *UniqueName.ToString(), *TestName);

	T* Obj = NewObject<T>(World, FName(ObjName.ToString()));
	if (Obj)
	{
		Context.Objects.Add(Obj);
	}
	return Obj;
}
