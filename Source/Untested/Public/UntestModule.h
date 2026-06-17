#pragma once

#include "Untest.h"

#include "SquidTasks/Task.h"
#include "SquidTasks/TaskManager.h"
#include "Containers/Ticker.h"
#include "Modules/ModuleInterface.h"

struct FUntestUI;

enum class EUntestTypeFlags : uint32;

struct FUntestSearchFilter
{
	FUntestSearchFilter();

	FString SearchName;
	EUntestTypeFlags Types;
};

struct FUntestInfo
{
	FUntestName Name;
	FUntestOpts Opts;
	EUntestTypeFlags TestType;
};

enum class EUntestResult : uint32
{
	Fail,
	Success,
	Skipped,
};

const TCHAR* UntestResultStr(EUntestResult Result);

struct FUntestResults
{
	FUntestName TestName;
	float DurationMs = 0.0;
	EUntestResult Result = EUntestResult::Skipped;
	TArray<FString> Errors;
};

DECLARE_DELEGATE_OneParam(FUntestOnTestStarted, const FUntestName& /*TestName*/);
DECLARE_DELEGATE_OneParam(FUntestOnTestComplete, const FUntestResults& /*Results*/);
DECLARE_DELEGATE_OneParam(FUntestOnAllTestsComplete, TArrayView<const FUntestResults> /*AllResults*/);

struct FUntestRunOpts
{
	bool bNoTimeouts = false;
	bool bIncludeDisabled = false;
	FUntestOnTestStarted OnTestStarted;
	FUntestOnTestComplete OnTestComplete;
	FUntestOnAllTestsComplete OnAllTestsComplete;
};

class FUntestModule : public IModuleInterface
{
public:
	FUntestModule() = default;
	~FUntestModule() = default;

	static FUntestModule& Get();
	static FUntestModule* GetSafe();

	// IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	// FTickerDelegate
	bool Tick(float DeltaTime);

	// For fixtures
	static void RegisterFixture(const FUntestFixtureFactory& Factory);
	static void UnregisterFixture(const FUntestFixtureFactory& Factory);

	// Find/Run test interface
	TArray<FUntestInfo> FindTests(const FUntestSearchFilter& Filter);
	bool QueueTests(TArrayView<const FString> TestNames, const FUntestRunOpts& Opts);
	bool HasRunningTests() const;
	void StopTests();
	TArrayView<const FUntestResults> GetResults() const;
	bool WriteTestReport(const TCHAR* ReportPath) const;
	static bool WriteTestReport(const TCHAR* ReportPath, TArrayView<const FUntestResults> Results);

private:
	using FTestFactoryMap = TMap<FString, const FUntestFixtureFactory*>;

	UntestTask RunTest(TSharedPtr<FUntestFixture> Fixture);
	static FTestFactoryMap& GetTestFactories();

	static FTestFactoryMap* TestFactories;

	FUntestRunOpts RunOpts;
	TArray<FString> QueuedTests;
	TArray<FUntestResults> TestResults;
	TArray<TSharedPtr<FUntestFixture>> RunningTests;
	TArray<TSharedPtr<FUntestFixture>> StoppingTests;

	TUniquePtr<FUntestUI> UI;
};
