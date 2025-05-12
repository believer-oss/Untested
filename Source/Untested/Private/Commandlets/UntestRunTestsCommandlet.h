#pragma once

#include "Commandlets/Commandlet.h"
#include "UntestRunTestsCommandlet.generated.h"

// This commandlet runs all or a subset of tests written with the Untest framework.
//
// Usage:
//
//   UnrealEditor-Cmd.exe <PathToUProject> -run=UntestRunTests [-Name=<FullOrPartialName>] [-ReportPath=<Path>] [-NoTimeout]
//
// Arguments:
//
//   -Name: Optional. Specify either a fully-qualified or partial test name. Performs
//       case-insensitive substring matching on the original test name. For example:
//           -Name=Math.VectorAdd
//           -Name=Math.
//           -Name=VectorAdd
//           -Name=Vector
//           -Name=Add
//
//   -ReportPath: Optional. Specify a path to write a junit-style xml report file. For example:
//           -ReportPath=C:\reports\bvtestreport.xml
//           -ReportPath=Intermediate\Untest\Reports\run1.xml
//
//   -NoTimeout: Optional. Use to suppress failing tests due to timeouts. Handy for ensuring
//       cloud build machines that get timesliced inconsistently don't fail tests for timing
//       out too early.
//
UCLASS()
class UUntestRunTestsCommandlet : public UCommandlet
{
	GENERATED_BODY()

	virtual int32 Main(const FString& Params) override;
};
