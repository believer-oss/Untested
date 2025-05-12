#include "UntestRunTestsCommandlet.h"
#include "Untest.h"
#include "UntestModule.h"

DEFINE_LOG_CATEGORY_STATIC(LogUntestRunTestsCommandlet, Display, All);

struct FUntestRunTestsCommandletOptions
{
	FUntestSearchFilter Filter;
	FString ReportPath;
	bool bNoTimeouts = false;
	bool bIncludeDisabled = false;

	static FUntestRunTestsCommandletOptions FromParams(const FString& Params)
	{
		FUntestRunTestsCommandletOptions Options;

		TArray<FString> Tokens;
		TArray<FString> Switches;
		TMap<FString, FString> SwitchParams;

		UCommandlet::ParseCommandLine(*Params, Tokens, Switches, SwitchParams);

		if (FString* NameFilter = SwitchParams.Find(TEXT("Name")))
		{
			Options.Filter.SearchName = *NameFilter;
		}

		if (FString* ReportPath = SwitchParams.Find(TEXT("ReportPath")))
		{
			Options.ReportPath = *ReportPath;
		}

		if (Switches.Contains(TEXT("NoTimeout")) || Switches.Contains(TEXT("NoTimeouts")))
		{
			Options.bNoTimeouts = true;
		}

		if (Switches.Contains(TEXT("IncludeDisabled")))
		{
			Options.bIncludeDisabled = true;
		}

		return Options;
	}
};

int32 UUntestRunTestsCommandlet::Main(const FString& Params)
{
	// Ensure no other packages that need to load interfere with test timings.
	FlushAsyncLoading();

	const FUntestRunTestsCommandletOptions RunOptions = FUntestRunTestsCommandletOptions::FromParams(Params);

	UE_LOG(LogUntestRunTestsCommandlet, Display, TEXT("Discovering tests with name filter '%s'..."), *RunOptions.Filter.SearchName);

	FUntestModule& Module = FUntestModule::Get();

	TArray<FUntestInfo> Tests = Module.FindTests(RunOptions.Filter);
	if (Tests.IsEmpty())
	{
		UE_LOG(LogUntestRunTestsCommandlet, Error, TEXT("No tests found for Name '%s'."), *RunOptions.Filter.SearchName);
		return 0;
	}

	UE_LOG(LogUntestRunTestsCommandlet, Display, TEXT("Found %d tests to run."), Tests.Num());

	TArray<FString> TestNames;
	TestNames.Reserve(Tests.Num());
	for (FUntestInfo& Info : Tests)
	{
		TestNames.Emplace(Info.Name.ToFull());
	}

	auto OnTestStartedDelegate = FBVOnTestStarted::CreateLambda([](const FUntestName& TestName)
		{
			UE_LOG(LogUntestRunTestsCommandlet, Display, TEXT("Running test: %s"), *TestName.ToFull());
		});

	auto OnTestCompleteDelegate = FBVOnTestComplete::CreateLambda([](const FUntestResults& Results)
		{
			if (Results.Errors.IsEmpty())
			{
				UE_LOG(LogUntestRunTestsCommandlet, Display, TEXT("%s succeeded (%.2fms)"), *Results.TestName.ToFull(), Results.DurationMs);
			}
			else
			{
				UE_LOG(LogUntestRunTestsCommandlet, Error, TEXT("%s failed. Errors:"), *Results.TestName.ToFull());
				for (const FString& Error : Results.Errors)
				{
					UE_LOG(LogUntestRunTestsCommandlet, Error, TEXT("%s"), *Error);
				}
			}
		});

	bool bAreTestsRunning = true;
	bool bAnyFailures = false;
	auto OnAllTestsCompleteDelegate = FBVOnAllTestsComplete::CreateLambda([&bAreTestsRunning, &bAnyFailures](TArrayView<const FUntestResults> AllResults)
		{
			bAreTestsRunning = false;

			int32 NumTests = AllResults.Num();
			int32 NumFailed = 0;
			for (const FUntestResults& Results : AllResults)
			{
				NumFailed += (Results.Errors.Num() > 0) ? 1 : 0;
			}

			if (NumFailed == 0)
			{
				UE_LOG(LogUntestRunTestsCommandlet, Display, TEXT("Test run finished. %d / %d tests succeeded."), NumTests, NumTests);
			}
			else
			{
				int32 NumSucceeded = NumTests - NumFailed;
				UE_LOG(LogUntestRunTestsCommandlet, Display, TEXT("Test run finished. %d / %d tests succeeded. %d tests failed."),
					NumSucceeded, NumTests, NumFailed);
				bAnyFailures = true;
			}
		});

	FUntestRunOpts RunOpts;
	RunOpts.bNoTimeouts = RunOptions.bNoTimeouts;
	RunOpts.bIncludeDisabled = RunOptions.bIncludeDisabled;
	RunOpts.OnTestStarted = OnTestStartedDelegate;
	RunOpts.OnTestComplete = OnTestCompleteDelegate;
	RunOpts.OnAllTestsComplete = OnAllTestsCompleteDelegate;
	if (Module.QueueTests(TestNames, RunOpts) == false)
	{
		UE_LOG(LogUntestRunTestsCommandlet, Error, TEXT("Failed to queue tests for running. Is another system using the test module?"));
		return 1;
	}

	while (bAreTestsRunning)
	{
		CommandletHelpers::TickEngine();
	}

	if (RunOptions.ReportPath.IsEmpty() == false)
	{
		Module.WriteTestReport(*RunOptions.ReportPath);
	}

	return bAnyFailures ? 1 : 0;
}
