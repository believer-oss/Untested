#include "UntestModule.h"
#include "Untest.h"
#include "UI/UntestUI.h"

#include "Modules/ModuleManager.h"
#include "Misc/FileHelper.h"

const TCHAR* UntestResultStr(EUntestResult Result)
{
	switch (Result)
	{
		case EUntestResult::Fail:
			return TEXT("Fail");
		case EUntestResult::Success:
			return TEXT("Success");
		case EUntestResult::Skipped:
			return TEXT("Skipped");
	}
	ensureMsgf(false, TEXT("Unhandled case %u"), static_cast<uint32>(Result));
	return TEXT("<UNKNOWN>");
}

FUntestSearchFilter::FUntestSearchFilter()
	: Types(EUntestTypeFlags::All)
{
}

const FName NAME_Module = TEXT("Untested");

TMap<FString, const FUntestFixtureFactory*>* FUntestModule::TestFactories = nullptr;

FUntestModule& FUntestModule::Get()
{
	return FModuleManager::LoadModuleChecked<FUntestModule>(NAME_Module);
}

FUntestModule* FUntestModule::GetSafe()
{
	return FModuleManager::LoadModulePtr<FUntestModule>(NAME_Module);
}

void FUntestModule::StartupModule()
{
	UI = MakeUnique<FUntestUI>();
}

void FUntestModule::ShutdownModule()
{
	UI = nullptr;
}

bool FUntestModule::Tick(float DeltaTime)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FUntestModule::Tick);

	if (RunningTests.Num() == 0 && QueuedTests.Num() > 0)
	{
		FTestFactoryMap& Factories = GetTestFactories();

		bool bCanQueueNextTest = true;
		while (bCanQueueNextTest && QueuedTests.Num() > 0)
		{
			FString FullTestName = QueuedTests.Pop();
			if (const FUntestFixtureFactory** FactoryPtr = Factories.Find(FullTestName))
			{
				const FUntestFixtureFactory* Factory = *FactoryPtr;
				check(Factory);

				RunOpts.OnTestStarted.ExecuteIfBound(Factory->GetName());

				const FUntestOpts& Opts = Factory->GetOpts();

				if (Opts.IsSet(EUntestFlags::Disabled) && RunOpts.bIncludeDisabled == false)
				{
					FUntestResults Results;
					Results.TestName = Factory->GetName();
					Results.DurationMs = 0.0f;
					Results.Result = EUntestResult::Skipped;

					TestResults.Emplace(MoveTemp(Results));
					RunOpts.OnTestComplete.ExecuteIfBound(TestResults.Last());
					continue;
				}

				TSharedPtr<FUntestContext> TestContext = MakeShared<FUntestContext>();
				TestContext->TestName = Factory->GetName();
				TestContext->TaskManager = MakeUnique<Squid::TaskManager>();
				TestContext->TimeoutMs = Opts.TimeoutMs;
				// NOTE: TestContext->TimestampBegin is set in RunTest() to get a more accurate time since the
				// coroutine always yields at first

				TSharedPtr<FUntestFixture> Fixture = Factory->New(TestContext);
				Squid::Task<> Task = RunTest(Fixture);
				TestContext->Task = TestContext->TaskManager->RunManaged(MoveTemp(Task));
				RunningTests.Emplace(Fixture);

				// TODO run pure tests on a worker thread if needed
				bCanQueueNextTest = Opts.IsSet(EUntestFlags::Pure);
			}
		}
	}

	// Calling update _after_ new tasks have been queued gives them a chance to be finished this frame if they don't
	// need to update
	const double TimesliceBudgetMs = 8.0;
	const double TimestampBegin = FPlatformTime::Seconds();
	for (TSharedPtr<FUntestFixture>& Fixture : RunningTests)
	{
		FUntestContext& Context = Fixture->GetContext();

		{
			FString FullTestName = Context.GetName().ToFull();
			TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FullTestName);

			Context.TaskManager->Update();
		}

		const double Now = FPlatformTime::Seconds();

		const double TestElapsedMs = (Now - Context.TimestampBegin) * 1000.0;
		if (RunOpts.bNoTimeouts == false && TestElapsedMs > Context.TimeoutMs)
		{
			Context.TimestampEnd = Now;

			FString Error;
			if (Context.Task.IsDone())
			{
				Error = FString::Printf(TEXT("Test finished, but overran timeout limit: %.2fms elapsed / %.2fms max"), TestElapsedMs, Context.TimeoutMs);
			}
			else
			{
				Error = FString::Printf(TEXT("Timed out at: %.2fms elapsed / %.2fms max"), TestElapsedMs, Context.TimeoutMs);
				Context.Task.Kill();
				// TODO handle graceful teardown by moving the task stuff into StoppingTests
			}
			Context.Errors.Emplace(MoveTemp(Error));
		}

		const double ElapsedMs = (Now - TimestampBegin) * 1000.0;
		if (ElapsedMs > TimesliceBudgetMs)
		{
			break;
		}
	}

	for (int i = 0; i < RunningTests.Num();)
	{
		FUntestContext& Context = RunningTests[i]->GetContext();
		if (Context.Task.IsDone())
		{
			FUntestResults Results;
			Results.TestName = MoveTemp(Context.TestName);
			Results.DurationMs = (Context.TimestampEnd - Context.TimestampBegin) * 1000.0;
			Results.Result = Context.Errors.IsEmpty() ? EUntestResult::Success : EUntestResult::Fail;
			Results.Errors = MoveTemp(Context.Errors);

			TestResults.Emplace(MoveTemp(Results));

			RunOpts.OnTestComplete.ExecuteIfBound(TestResults.Last());

			RunningTests.RemoveAtSwap(i, EAllowShrinking::No);
		}
		else
		{
			++i;
		}
	}

	for (int i = 0; i < StoppingTests.Num();)
	{
		FUntestContext& Context = StoppingTests[i]->GetContext();
		Context.TaskManager->Update();
		if (Context.Task.IsDone())
		{
			const double Now = FPlatformTime::Seconds();

			FUntestResults Results;
			Results.TestName = MoveTemp(Context.TestName);
			Results.DurationMs = (Now - Context.TimestampBegin) * 1000.0;
			Results.Result = Context.Errors.IsEmpty() ? EUntestResult::Skipped : EUntestResult::Fail;
			Results.Errors = MoveTemp(Context.Errors);

			TestResults.Emplace(MoveTemp(Results));

			RunOpts.OnTestComplete.ExecuteIfBound(TestResults.Last());

			StoppingTests.RemoveAtSwap(i, EAllowShrinking::No);
		}
		else
		{
			++i;
		}
	}

	if (RunningTests.IsEmpty() && QueuedTests.IsEmpty() && StoppingTests.IsEmpty())
	{
		RunOpts.OnAllTestsComplete.ExecuteIfBound(TestResults);

		return false; // unschedule tick
	}

	return true;
}

void FUntestModule::RegisterFixture(const FUntestFixtureFactory& Factory)
{
	FTestFactoryMap& Factories = GetTestFactories();

	FUntestName TestName = Factory.GetName();
	FString FullTestName = TestName.ToFull();
	checkf(Factories.Contains(FullTestName) == false,
		TEXT("Another test has been registered with the name %s. You must choose a unique name."), *FullTestName);
	Factories.Emplace(MoveTemp(FullTestName), &Factory);
}

void FUntestModule::UnregisterFixture(const FUntestFixtureFactory& Factory)
{
	FTestFactoryMap& Factories = GetTestFactories();
	Factories.Remove(Factory.GetName().ToFull());
}

TArray<FUntestInfo> FUntestModule::FindTests(const FUntestSearchFilter& Filter)
{
	FTestFactoryMap& Factories = GetTestFactories();

	TArray<FString> AllTestNames;
	Factories.GetKeys(AllTestNames);

	TArray<FUntestInfo> Tests;
	Tests.Reserve(AllTestNames.Num());

	for (FString& TestName : AllTestNames)
	{
		if (Filter.SearchName.IsEmpty() || TestName.Contains(*Filter.SearchName, ESearchCase::IgnoreCase))
		{
			const FUntestFixtureFactory** FactoryPtr = Factories.Find(TestName);
			check(FactoryPtr);
			const FUntestFixtureFactory* Factory = *FactoryPtr;
			if ((Factory->GetType() & Filter.Types) != EUntestTypeFlags::None)
			{
				Tests.Emplace(FUntestInfo{ Factory->GetName(), Factory->GetOpts(), Factory->GetType() });
			}
		}
	}

	return Tests;
}

bool FUntestModule::QueueTests(TArrayView<const FString> TestNames, const FUntestRunOpts& Opts)
{
	// We only run one set of tests at a time - this prevents multiple systems from
	// fighting over running the same set of tests.
	if (HasRunningTests() == false)
	{
		RunOpts = Opts;
		QueuedTests.Append(TestNames);
		Algo::Reverse(QueuedTests);
		TestResults.Reset();

		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FUntestModule::Tick));
		return true;
	}

	return false;
}

bool FUntestModule::HasRunningTests() const
{
	return QueuedTests.Num() > 0 || RunningTests.Num() > 0 || StoppingTests.Num() > 0;
}

void FUntestModule::StopTests()
{
	if (StoppingTests.Num() > 0)
	{
		return;
	}

	QueuedTests.Reset();

	for (TSharedPtr<FUntestFixture>& Fixture : RunningTests)
	{
		FUntestContext& Context = Fixture->GetContext();
		Context.TaskManager->KillAllTasks();

		const FString& FullTestName = Context.GetName().ToFull();
		UntestTask TeardownTask = Fixture->TeardownFixture(FullTestName);
		Context.TaskManager->RunManaged(MoveTemp(TeardownTask));
		Context.Task = TeardownTask;

		StoppingTests.Emplace(Fixture);
	}

	RunningTests.Reset();
}

TArrayView<const FUntestResults> FUntestModule::GetResults() const
{
	return TestResults;
}

bool FUntestModule::WriteTestReport(const TCHAR* ReportPath) const
{
	struct FTestStats
	{
		int32 NumTests = 0;
		int32 NumFailed = 0;
		int32 NumSkipped = 0;
		double TotalDurationSecs = 0;
	};

	struct FTestCategoryResults
	{
		FTestStats Stats;
		TArray<const FUntestResults*> Results;
	};

	struct FTestModuleResults
	{
		FTestStats Stats;
		TSortedMap<FString, FTestCategoryResults> Categories;
	};

	FTestStats TotalStats;
	TSortedMap<FString, FTestModuleResults> Modules;
	for (const FUntestResults& Result : TestResults)
	{
		FTestModuleResults& ModuleResults = Modules.FindOrAdd(Result.TestName.Module);
		FTestCategoryResults& CategoryResults = ModuleResults.Categories.FindOrAdd(Result.TestName.Category);
		CategoryResults.Results.Add(&Result);

		++TotalStats.NumTests;
		++ModuleResults.Stats.NumTests;
		++CategoryResults.Stats.NumTests;

		const double DurationSecs = Result.DurationMs / 1000.0;

		TotalStats.TotalDurationSecs += DurationSecs;
		ModuleResults.Stats.TotalDurationSecs += DurationSecs;
		CategoryResults.Stats.TotalDurationSecs += DurationSecs;

		if (Result.Result == EUntestResult::Fail)
		{
			++TotalStats.NumFailed;
			++ModuleResults.Stats.NumFailed;
			++CategoryResults.Stats.NumFailed;
		}
		else if (Result.Result == EUntestResult::Skipped)
		{
			++TotalStats.NumSkipped;
			++ModuleResults.Stats.NumSkipped;
			++CategoryResults.Stats.NumSkipped;
		}
	}

	TStringBuilder<512> Xml;
	Xml.Append(TEXT("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"));
	Xml.Appendf(TEXT("<testsuites tests=\"%d\" failures=\"%d\" time=\"%.2f\">\n"),
		TotalStats.NumTests, TotalStats.NumFailed, TotalStats.TotalDurationSecs);
	for (auto&& ModuleResults : Modules)
	{
		const FTestStats& ModuleStats = ModuleResults.Value.Stats;
		Xml.Appendf(TEXT("\t<testsuite name=\"%s\" tests=\"%d\" failures=\"%d\" time=\"%.2f\">\n"),
			*ModuleResults.Key, ModuleStats.NumTests, ModuleStats.NumFailed, ModuleStats.TotalDurationSecs);
		for (auto&& CategoryResults : ModuleResults.Value.Categories)
		{
			const FTestStats& CategoryStats = CategoryResults.Value.Stats;
			Xml.Appendf(TEXT("\t\t<testsuite name=\"%s\" tests=\"%d\" failures=\"%d\" time=\"%.2f\">\n"),
				*CategoryResults.Key, CategoryStats.NumTests, CategoryStats.NumFailed, CategoryStats.TotalDurationSecs);
			CategoryResults.Value.Results.Sort([](const FUntestResults& A, const FUntestResults& B)
				{
					return A.TestName.Test < B.TestName.Test;
				});

			for (const FUntestResults* Test : CategoryResults.Value.Results)
			{
				Xml.Appendf(TEXT("\t\t\t<testcase name=\"%s\" classname=\"%s\" time=\"%.2f\">\n"),
					*Test->TestName.Test, *Test->TestName.ToFull(), Test->DurationMs / 1000.0);
				if (Test->Result == EUntestResult::Skipped)
				{
					Xml.Append(TEXT("\t\t\t\t<skipped/>\n"));
				}
				else if (Test->Result == EUntestResult::Fail)
				{
					if (Test->Errors.Num() > 0)
					{
						for (const FString& Error : Test->Errors)
						{
							Xml.Appendf(TEXT("\t\t\t\t<failure>%s</failure>\n"), *Error);
						}
					}
					else
					{
						Xml.Append(TEXT("\t\t\t\t<failure>Failed for unknown reasons</failure>\n"));
					}
				}
				Xml.Append(TEXT("\t\t\t</testcase>\n"));
			}

			Xml.Append(TEXT("\t\t</testsuite>\n"));
		}
		Xml.Append(TEXT("\t</testsuite>\n"));
	}
	Xml.Append(TEXT("</testsuites>\n"));

	bool bSuccess = FFileHelper::SaveStringToFile(Xml.ToString(), ReportPath);
	return bSuccess;
}

FUntestModule::FTestFactoryMap& FUntestModule::GetTestFactories()
{
	if (TestFactories == nullptr)
	{
		TestFactories = new FTestFactoryMap();
	}
	return *TestFactories;
}

UntestTask FUntestModule::RunTest(TSharedPtr<FUntestFixture> Fixture)
{
	Fixture->GetContext().TimestampBegin = FPlatformTime::Seconds();

	co_await Fixture->SetupFixture(Fixture->GetContext().GetName().ToFull());

	if (Fixture->GetContext().Errors.IsEmpty())
	{
		co_await Fixture->RunFixture(Fixture->GetContext().GetName().ToFull());
	}
	co_await Fixture->TeardownFixture(Fixture->GetContext().GetName().ToFull());

	Fixture->GetContext().TimestampEnd = FPlatformTime::Seconds();
}

IMPLEMENT_MODULE(FUntestModule, Untested);
