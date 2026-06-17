#include "UntestRunTestsCommandlet.h"
#include "Untest.h"
#include "UntestModule.h"
#include "Misc/FileHelper.h"

DEFINE_LOG_CATEGORY_STATIC(LogUntestRunTestsCommandlet, Display, All);

struct FUntestRunTestsCommandletOptions
{
	FUntestSearchFilter Filter;
	FString ReportPath;
	FString TestsFile; // File containing exact test names to run (one per line)
	bool bNoTimeouts = false;
	bool bIncludeDisabled = false;
	bool bListTests = false;

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

		if (FString* TestsFilePath = SwitchParams.Find(TEXT("TestsFile")))
		{
			Options.TestsFile = *TestsFilePath;
		}

		if (Switches.Contains(TEXT("NoTimeout")) || Switches.Contains(TEXT("NoTimeouts")))
		{
			Options.bNoTimeouts = true;
		}

		if (Switches.Contains(TEXT("IncludeDisabled")))
		{
			Options.bIncludeDisabled = true;
		}

		if (Switches.Contains(TEXT("ListTests")))
		{
			Options.bListTests = true;
		}

		return Options;
	}
};

int32 UUntestRunTestsCommandlet::Main(const FString& Params)
{
	// Ensure no other packages that need to load interfere with test timings.
	FlushAsyncLoading();

	const FUntestRunTestsCommandletOptions RunOptions = FUntestRunTestsCommandletOptions::FromParams(Params);

	FUntestModule& Module = FUntestModule::Get();

	TArray<FString> TestNames;

	// If a TestsFile is provided, read exact test names from it (bypasses filter-based discovery)
	if (!RunOptions.TestsFile.IsEmpty())
	{
		UE_LOG(LogUntestRunTestsCommandlet, Display, TEXT("Reading test names from file: %s"), *RunOptions.TestsFile);

		FString FileContents;
		if (!FFileHelper::LoadFileToString(FileContents, *RunOptions.TestsFile))
		{
			UE_LOG(LogUntestRunTestsCommandlet, Error, TEXT("Failed to read tests file: %s"), *RunOptions.TestsFile);
			return 1;
		}

		TArray<FString> Lines;
		FileContents.ParseIntoArrayLines(Lines, true);

		for (const FString& Line : Lines)
		{
			FString TrimmedLine = Line.TrimStartAndEnd();
			if (!TrimmedLine.IsEmpty() && !TrimmedLine.StartsWith(TEXT("#")))
			{
				TestNames.Add(TrimmedLine);
			}
		}

		if (TestNames.IsEmpty())
		{
			UE_LOG(LogUntestRunTestsCommandlet, Error, TEXT("No test names found in file: %s"), *RunOptions.TestsFile);
			return 0;
		}

		UE_LOG(LogUntestRunTestsCommandlet, Display, TEXT("Loaded %d test names from file."), TestNames.Num());
	}
	else
	{
		// Standard filter-based discovery
		UE_LOG(LogUntestRunTestsCommandlet, Display, TEXT("Discovering tests with name filter '%s'..."), *RunOptions.Filter.SearchName);

		TArray<FUntestInfo> Tests = Module.FindTests(RunOptions.Filter);
		if (Tests.IsEmpty())
		{
			UE_LOG(LogUntestRunTestsCommandlet, Error, TEXT("No tests found for Name '%s'."), *RunOptions.Filter.SearchName);
			return 0;
		}

		UE_LOG(LogUntestRunTestsCommandlet, Display, TEXT("Found %d tests to run."), Tests.Num());

		TestNames.Reserve(Tests.Num());
		for (FUntestInfo& Info : Tests)
		{
			TestNames.Emplace(Info.Name.ToFull());
		}
	}

	// If -ListTests flag is provided, just list the tests and exit
	if (RunOptions.bListTests)
	{
		UE_LOG(LogUntestRunTestsCommandlet, Display, TEXT("Available tests:"));
		for (const FString& TestName : TestNames)
		{
			UE_LOG(LogUntestRunTestsCommandlet, Display, TEXT("  - %s"), *TestName);
		}

		// Use the existing test report infrastructure for consistent output
		// Create "Skipped" results for each discovered test
		if (!RunOptions.ReportPath.IsEmpty())
		{
			TArray<FUntestResults> ListResults;
			ListResults.Reserve(TestNames.Num());

			for (const FString& TestName : TestNames)
			{
				FUntestResults& Result = ListResults.AddDefaulted_GetRef();
				Result.TestName = FUntestName::FromFull(TestName);
				Result.Result = EUntestResult::Skipped;
				Result.DurationMs = 0.0f;
			}

			// Use static method to write report without modifying module state
			if (FUntestModule::WriteTestReport(*RunOptions.ReportPath, ListResults))
			{
				UE_LOG(LogUntestRunTestsCommandlet, Display, TEXT("Test list written to: %s"), *RunOptions.ReportPath);
			}
			else
			{
				UE_LOG(LogUntestRunTestsCommandlet, Error, TEXT("Failed to write test list to: %s"), *RunOptions.ReportPath);
				return 1;
			}
		}

		return 0;
	}

	UE_LOG(LogUntestRunTestsCommandlet, Display, TEXT("Found %d tests to run."), TestNames.Num());

	bool bAreTestsRunning = true;
	bool bAnyFailures = false;
	bool bReportWritten = false;

	auto OnTestStartedDelegate = FUntestOnTestStarted::CreateLambda([](const FUntestName& TestName)
		{
			UE_LOG(LogUntestRunTestsCommandlet, Display, TEXT("Running test: %s"), *TestName.ToFull());
		});

	auto OnTestCompleteDelegate = FUntestOnTestComplete::CreateLambda([&Module, &RunOptions, &bReportWritten](const FUntestResults& Results)
		{
			if (Results.Result == EUntestResult::Skipped)
			{
				UE_LOG(LogUntestRunTestsCommandlet, Display, TEXT("%s skipped."), *Results.TestName.ToFull());
			}
			else if (Results.Errors.IsEmpty())
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

			// Write report incrementally after each test to avoid losing results if process exits early
			if (RunOptions.ReportPath.IsEmpty() == false)
			{
				Module.WriteTestReport(*RunOptions.ReportPath);
				bReportWritten = true;
			}
		});

	auto OnAllTestsCompleteDelegate = FUntestOnAllTestsComplete::CreateLambda([&bAreTestsRunning, &RunOptions, &Module, &bReportWritten, &bAnyFailures](TArrayView<const FUntestResults> AllResults)
		{
			bAreTestsRunning = false;

			int32 NumTests = AllResults.Num();
			int32 NumFailed = 0;
			int32 NumSkipped = 0;
			for (const FUntestResults& Results : AllResults)
			{
				if (Results.Result == EUntestResult::Fail)
				{
					NumFailed++;
				}
				else if (Results.Result == EUntestResult::Skipped)
				{
					NumSkipped++;
				}
			}

			int32 NumSucceeded = NumTests - NumFailed - NumSkipped;

			if (NumFailed == 0)
			{
				UE_LOG(LogUntestRunTestsCommandlet, Display, TEXT("Test run finished. %d / %d tests succeeded, %d skipped."),
					NumSucceeded, NumTests, NumSkipped);
			}
			else
			{
				UE_LOG(LogUntestRunTestsCommandlet, Display, TEXT("Test run finished. %d / %d tests succeeded, %d skipped, %d failed."),
					NumSucceeded, NumTests, NumSkipped, NumFailed);
				bAnyFailures = true;
			}

			// Write the report immediately to avoid losing results if the process exits early
			if (RunOptions.ReportPath.IsEmpty() == false && bReportWritten == false)
			{
				UE_LOG(LogUntestRunTestsCommandlet, Display, TEXT("Writing test report to: %s"), *RunOptions.ReportPath);
				if (Module.WriteTestReport(*RunOptions.ReportPath))
				{
					UE_LOG(LogUntestRunTestsCommandlet, Display, TEXT("Test report written successfully."));
					bReportWritten = true;
				}
				else
				{
					UE_LOG(LogUntestRunTestsCommandlet, Error, TEXT("Failed to write test report."));
				}
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

	// If report wasn't written in OnAllTestsComplete (shouldn't happen), write it now
	if (RunOptions.ReportPath.IsEmpty() == false && bReportWritten == false)
	{
		UE_LOG(LogUntestRunTestsCommandlet, Warning, TEXT("Report not written in OnAllTestsComplete, writing now as fallback..."));
		Module.WriteTestReport(*RunOptions.ReportPath);
	}

	return bAnyFailures ? 1 : 0;
}
