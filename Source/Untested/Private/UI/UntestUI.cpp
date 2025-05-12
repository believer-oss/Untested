#include "UntestUI.h"

// UI is only allowed in windows editor because some features (such as SMultilineEditableText)
// are apparently not allowed on other platforms
#if WITH_EDITOR && PLATFORM_WINDOWS

#include "Untest.h"
#include "UntestModule.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/Commands.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateColor.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"
#include "Styling/StarshipCoreStyle.h"
#include "Styling/StyleColors.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Text/SMultilineEditableText.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SExpanderArrow.h"
#include "Widgets/Views/STreeView.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "UntestUI"

// NOTE: A lot of this code ripped from SAutomationWindow.cpp and friends

const FName NAME_UntestTab = TEXT("UntestUI");

//////////////////////////////////////////////////////////////////////////
// Untest Style - ripped from AutomationWindowStyle.cpp

class FUntestStyle : public FSlateStyleSet
{
public:
	virtual const FName& GetStyleSetName() const override;

	static const FUntestStyle& Get();
	static void Shutdown();

	~FUntestStyle();

private:
	FUntestStyle();

	static FName StyleName;
	static TUniquePtr<FUntestStyle> Inst;
};

FName FUntestStyle::StyleName("FUntestStyle");
TUniquePtr<FUntestStyle> FUntestStyle::Inst(nullptr);

const FName& FUntestStyle::GetStyleSetName() const
{
	return StyleName;
}

const FUntestStyle& FUntestStyle::Get()
{
	if (!Inst.IsValid())
	{
		Inst = TUniquePtr<FUntestStyle>(new FUntestStyle);
	}
	return *(Inst.Get());
}

void FUntestStyle::Shutdown()
{
	Inst.Reset();
}

FUntestStyle::FUntestStyle()
	: FSlateStyleSet(StyleName)
{
	const FVector2D Icon16x16(16.0f, 16.0f);
	const FVector2D Icon20x20(20.0f, 20.0f);
	const FVector2D Icon40x40(40.0f, 40.0f);

	SetParentStyleName(FAppStyle::GetAppStyleSetName());

	SetContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

	{
		FTextBlockStyle NormalText = FAppStyle::GetWidgetStyle<FTextBlockStyle>("NormalText");

		Set("Untest.Header", FTextBlockStyle(NormalText).SetFont(DEFAULT_FONT("Mono", 12)).SetColorAndOpacity(FStyleColors::ForegroundHeader));

		Set("Untest.Normal", FTextBlockStyle(NormalText).SetFont(DEFAULT_FONT("Mono", 9)).SetColorAndOpacity(FStyleColors::Foreground));

		Set("Untest.Warning", FTextBlockStyle(NormalText).SetFont(DEFAULT_FONT("Mono", 9)).SetColorAndOpacity(FStyleColors::Warning));

		Set("Untest.Error", FTextBlockStyle(NormalText).SetFont(DEFAULT_FONT("Mono", 9)).SetColorAndOpacity(FStyleColors::Error));

		Set("Untest.ReportHeader", FTextBlockStyle(NormalText).SetFont(DEFAULT_FONT("Mono", 10)).SetColorAndOpacity(FStyleColors::ForegroundHeader));

		// state of individual tests
		Set("Untest.Disabled", new IMAGE_BRUSH("Automation/ExcludedTestsFilter", Icon16x16));
		Set("Untest.Success", new IMAGE_BRUSH("Automation/Success", Icon16x16));
		Set("Untest.Warning", new IMAGE_BRUSH("Automation/Warning", Icon16x16));
		Set("Untest.Fail", new IMAGE_BRUSH("Automation/Fail", Icon16x16));
		Set("Untest.InProcess", new IMAGE_BRUSH("Automation/InProcess", Icon16x16));
		Set("Untest.NotRun", new IMAGE_BRUSH("Automation/NotRun", Icon16x16));
		Set("Untest.Skipped", new IMAGE_BRUSH("Automation/NoSessionWarning", Icon16x16));
		Set("Untest.ParticipantsWarning", new IMAGE_BRUSH("Automation/ParticipantsWarning", Icon16x16));
		Set("Untest.Participant", new IMAGE_BRUSH("Automation/Participant", Icon16x16));

		// status as a regression test or not
		Set("Untest.SmokeTest", new IMAGE_BRUSH("Automation/SmokeTest", Icon16x16));
		Set("Untest.SmokeTestParent", new IMAGE_BRUSH("Automation/SmokeTestParent", Icon16x16));

		// run icons
		Set("Untest.RunTests", new IMAGE_BRUSH("Automation/RunTests", Icon40x40));
		Set("Untest.RefreshTests", new IMAGE_BRUSH("Automation/RefreshTests", Icon40x40));
		Set("Untest.FindWorkers", new IMAGE_BRUSH("Automation/RefreshWorkers", Icon40x40));
		Set("Untest.StopTests", new IMAGE_BRUSH("Automation/StopTests", Icon40x40));
		Set("Untest.RunTests.Small", new IMAGE_BRUSH("Automation/RunTests", Icon20x20));
		Set("Untest.RefreshTests.Small", new IMAGE_BRUSH("Automation/RefreshTests", Icon20x20));
		Set("Untest.FindWorkers.Small", new IMAGE_BRUSH("Automation/RefreshWorkers", Icon20x20));
		Set("Untest.StopTests.Small", new IMAGE_BRUSH("Automation/StopTests", Icon20x20));

		// filter icons
		Set("Untest.ErrorFilter", new IMAGE_BRUSH("Automation/ErrorFilter", Icon40x40));
		Set("Untest.WarningFilter", new IMAGE_BRUSH("Automation/WarningFilter", Icon40x40));
		Set("Untest.SmokeTestFilter", new IMAGE_BRUSH("Automation/SmokeTestFilter", Icon40x40));
		Set("Untest.DeveloperDirectoryContent", new IMAGE_BRUSH("Automation/DeveloperDirectoryContent", Icon40x40));
		Set("Untest.ExcludedTestsFilter", new IMAGE_BRUSH("Automation/ExcludedTestsFilter", Icon40x40));
		Set("Untest.ErrorFilter.Small", new IMAGE_BRUSH("Automation/ErrorFilter", Icon20x20));
		Set("Untest.WarningFilter.Small", new IMAGE_BRUSH("Automation/WarningFilter", Icon20x20));
		Set("Untest.SmokeTestFilter.Small", new IMAGE_BRUSH("Automation/SmokeTestFilter", Icon20x20));
		Set("Untest.DeveloperDirectoryContent.Small", new IMAGE_BRUSH("Automation/DeveloperDirectoryContent", Icon20x20));
		Set("Untest.TrackHistory", new IMAGE_BRUSH("Automation/TrackTestHistory", Icon40x40));

		// device group settings
		Set("Untest.GroupSettings", new IMAGE_BRUSH("Automation/Groups", Icon40x40));
		Set("Untest.GroupSettings.Small", new IMAGE_BRUSH("Automation/Groups", Icon20x20));

		// test backgrounds
		Set("Untest.GameGroupBorder", new BOX_BRUSH("Automation/GameGroupBorder", FMargin(4.0f / 16.0f)));
		Set("Untest.EditorGroupBorder", new BOX_BRUSH("Automation/EditorGroupBorder", FMargin(4.0f / 16.0f)));
	}

	FSlateStyleRegistry::RegisterSlateStyle(*this);
}

FUntestStyle::~FUntestStyle()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*this);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Local Helpers

static FSlateIcon GetIcon()
{
	return FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.UserDefinedStruct");
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// SUntestRunner

namespace UIConstants
{
	const FName Checked(TEXT("Checked"));
	const FName Disabled(TEXT("Disabled"));
	const FName Title(TEXT("Title"));
	const FName Duration(TEXT("Duration"));
	const FName StatusIcon(TEXT("StatusIcon"));
} // namespace UIConstants

enum EUntestStatus
{
	NotRun,
	Success,
	Skipped,
	Fail,
};

class FUntestRunnerTest : public TSharedFromThis<FUntestRunnerTest>
{
public:
	bool IsParent() const { return Children.Num() > 0; }

	void SetQueued(bool bQueued)
	{
		bIsQueued = bQueued;
		for (TSharedPtr<FUntestRunnerTest>& Test : FilteredChildren)
		{
			Test->SetQueued(bQueued);
		}
	}

	EUntestStatus GetStatus() const
	{
		if (Children.Num() > 0)
		{
			EUntestStatus Status = EUntestStatus::NotRun;
			for (const TSharedPtr<FUntestRunnerTest>& Child : FilteredChildren)
			{
				EUntestStatus ChildStatus = Child->GetStatus();
				switch (ChildStatus)
				{
					case EUntestStatus::NotRun:
						break;
					case EUntestStatus::Success:
						Status = EUntestStatus::Success;
						break;
					case EUntestStatus::Fail:
						return EUntestStatus::Fail;
					case EUntestStatus::Skipped:
						// Prefer retaining Success state
						if (Status == EUntestStatus::NotRun)
						{
							Status = EUntestStatus::Skipped;
						}
						break;
				}
			}
			return Status;
		}

		if (Results.IsSet())
		{
			switch (Results->Result)
			{
				case EUntestResult::Fail:
					return EUntestStatus::Fail;
				case EUntestResult::Success:
					return EUntestStatus::Success;
				case EUntestResult::Skipped:
					return EUntestStatus::Skipped;
			}
		}

		return EUntestStatus::NotRun;
	}

	bool IsDisabled() const
	{
		if (FilteredChildren.Num() > 0)
		{
			bool bAllDisabled = true;
			for (const TSharedPtr<FUntestRunnerTest>& Child : FilteredChildren)
			{
				if (Child->bIsDisabled == false)
				{
					bAllDisabled = false;
					break;
				}
			}
			return bAllDisabled;
		}

		return bIsDisabled;
	}

	FString Name;
	FText DisplayName;
	EUntestTypeFlags TestType = EUntestTypeFlags::None;
	FUntestRunnerTest* Parent = nullptr;
	bool bIsQueued = false;
	bool bIsDisabled = false; // corresponds to EUntestFlags::Disabled

	// Only filled out for modules or categories
	TArray<TSharedPtr<FUntestRunnerTest>> Children;
	TArray<TSharedPtr<FUntestRunnerTest>> FilteredChildren;

	// Only ever valid for actual test instances
	TOptional<FUntestResults> Results;
};

DECLARE_DELEGATE_OneParam(FOnItemCheckedStateChanged, TSharedPtr<FUntestRunnerTest> /*Test*/);

class SUntestRow : public SMultiColumnTableRow<TSharedPtr<FString>>
{
public:
	// clang-format off
	SLATE_BEGIN_ARGS(SUntestRow)
		{}
		SLATE_ARGUMENT(float, ColumnWidth)
		SLATE_ARGUMENT(TSharedPtr<FUntestRunnerTest>, Test)
		SLATE_ATTRIBUTE(FText, HighlightText)
		SLATE_EVENT(FOnItemCheckedStateChanged, OnCheckedStateChanged) // Delegate called when a report has it's checkbox clicked
	SLATE_END_ARGS()
	// clang-format on

	float ColumnWidth;
	TSharedPtr<FUntestRunnerTest> Test;
	TAttribute<FText> HighlightText;
	FOnItemCheckedStateChanged OnCheckedStateChanged;

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
	{
		Test = InArgs._Test;
		ColumnWidth = InArgs._ColumnWidth;
		HighlightText = InArgs._HighlightText;
		OnCheckedStateChanged = InArgs._OnCheckedStateChanged;

		SMultiColumnTableRow<TSharedPtr<FString>>::Construct(SMultiColumnTableRow<TSharedPtr<FString>>::FArguments(), InOwnerTableView);
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
	{
		// clang-format off
		if (ColumnName == UIConstants::Checked)
		{
			return SNew(SCheckBox)
				.IsChecked(this, &SUntestRow::IsTestQueued)
				.OnCheckStateChanged(this, &SUntestRow::HandleOnCheckedStateChanged)
				.ToolTipText(LOCTEXT("Untest.TestRow.QueueDequeue", "Queue / Dequeue Test"));
		}
		else if (ColumnName == UIConstants::Disabled)
		{
			return SNew(SImage)
				.Image(this, &SUntestRow::GetTestDisabledIcon)
				.ToolTipText(this, &SUntestRow::GetTestDisabledTooltipText);
		}
		else if (ColumnName == UIConstants::Title)
		{
			return SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					//this is where the tree is marked as expandable or not.
					SNew( SExpanderArrow, SharedThis( this ) )
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::Format(LOCTEXT("Untest.TestRow.Name", "{0}"), Test->DisplayName))
					.HighlightText(HighlightText)
				];
		}
		else if (ColumnName == UIConstants::Duration)
		{
			return SNew(STextBlock)
				.Text(this, &SUntestRow::GetTestResultDuration);
		}
		else if (ColumnName == UIConstants::StatusIcon)
		{
			return SNew(SImage)
				.Image(this, &SUntestRow::GetTestResultStatusIcon)
				.ToolTipText(this, &SUntestRow::GetTestResultStatusTooltipText);
		}

		return SNew(STextBlock).Text(LOCTEXT("Untest.TestRow.UnknownWidget", "?"));
		// clang-format on
	}

	ECheckBoxState IsTestQueued() const
	{
		return (Test.IsValid() && Test->bIsQueued) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	void HandleOnCheckedStateChanged(ECheckBoxState NewState)
	{
		OnCheckedStateChanged.ExecuteIfBound(Test);
	}

	FText GetTestResultDuration() const
	{
		FString DurationStr;
		if (Test->Results.IsSet())
		{
			DurationStr = FString::Printf(TEXT("%.4f"), Test->Results->DurationMs);
		}
		return FText::FromString(DurationStr);
	}

	const FSlateBrush* GetTestDisabledIcon() const
	{
		const FName BrushName = Test->IsDisabled() ? FName(TEXT("Untest.Disabled")) : NAME_None;
		return FUntestStyle::Get().GetBrush(BrushName);
	}

	FText GetTestDisabledTooltipText() const
	{
		bool bIsDisabled = Test->IsDisabled();
		return bIsDisabled ? LOCTEXT("Untest.DisabledText", "Disabled") : LOCTEXT("Untest.EnabledText", "Enabled");
	}

	const FSlateBrush* GetTestResultStatusIcon() const
	{
		const EUntestStatus Status = Test->GetStatus();
		FName BrushName;
		switch (Status)
		{
			case EUntestStatus::NotRun:
				BrushName = FName(TEXT("Untest.NotRun"));
				break;
			case EUntestStatus::Success:
				BrushName = FName(TEXT("Untest.Success"));
				break;
			case EUntestStatus::Fail:
				BrushName = FName(TEXT("Untest.Fail"));
				break;
			case EUntestStatus::Skipped:
				BrushName = FName(TEXT("Untest.Skipped"));
				break;
		}

		return FUntestStyle::Get().GetBrush(BrushName);
	}

	FText GetTestResultStatusTooltipText() const
	{
		const EUntestStatus Status = Test->GetStatus();
		switch (Status)
		{
			case EUntestStatus::NotRun:
				return LOCTEXT("Untest.Status.NotRun", "Not Run");
			case EUntestStatus::Success:
				return LOCTEXT("Untest.Status.Success", "Success");
			case EUntestStatus::Skipped:
				return LOCTEXT("Untest.Status.Skipped", "Skipped");
			case EUntestStatus::Fail:
				return LOCTEXT("Untest.Status.Fail", "Fail");
		}
		return LOCTEXT("Untest.Status.Unknown", "Unknown");
	}
};

struct FUntestRunnerOptions
{
	bool bIsTimeoutEnabled = true;
	bool bIncludeDisabled = false;
};

class SUntestRunner : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUntestRunner) {}
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs);

private:
	// Slate callbacks
	FReply RunOrAbortTests();
	const FSlateBrush* GetRunOrAbortTestsIcon() const;
	FText GetRunOrAbortTestsLabel() const;
	EVisibility GetLargeToolBarVisibility() const;
	FText OnGetNumQueuedTestsString() const;
	void OnFilterTextChanged(const FText& InFilterText);
	bool AreTestsRunning() const;
	bool AreNoTestsRunning() const;
	ECheckBoxState IsTimeoutEnabled() const;
	void OnTimeoutCheckStateChanged(ECheckBoxState CheckBoxState);
	ECheckBoxState IncludeDisabled() const;
	void OnIncludeDisabledCheckStateChanged(ECheckBoxState CheckBoxState);
	FText GetTestResultsText() const;
	FText GetStatusText() const;
	EVisibility StatusProgressVisibility() const;
	TOptional<float> StatusProgress() const;
	const FSlateBrush* GetAllTestResultsStatusIcon() const;

	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FUntestRunnerTest> InItem, const TSharedRef<STableViewBase>& OwnerTable);
	void OnGetChildren(TSharedPtr<FUntestRunnerTest> InItem, TArray<TSharedPtr<FUntestRunnerTest>>& OutItems);
	void OnTestExpansionRecursive(TSharedPtr<FUntestRunnerTest> InAutomationReport, bool bInIsItemExpanded);
	void OnGlobalCheckboxStateChange(ECheckBoxState InCheckboxState);
	FText GetHighlightText() const;
	void OnItemCheckboxStateChanged(TSharedPtr<FUntestRunnerTest> Test);

	// UntestModule Callbacks
	void OnTestComplete(const FUntestResults& Results);
	void OnAllTestsComplete(TArrayView<const FUntestResults> AllResults);

	// Helpers
	int32 GetNumTestsQueued() const;
	void RefreshAvailableTests();

	// UI Data
	TSharedPtr<FUICommandList> CommandList;
	TSharedPtr<SSearchBox> TestNameSearchBox;
	TSharedPtr<STreeView<TSharedPtr<FUntestRunnerTest>>> TestTable;
	TSharedPtr<SMultiLineEditableText> ResultsText;
	int32 NumCompletedTests = 0;
	int32 NumFailedTests = 0;

	float ColumnWidth = 50.0f;

	// Test data
	TArray<TSharedPtr<FUntestRunnerTest>> AllTests;
	TArray<TSharedPtr<FUntestRunnerTest>> RootTests;
	TMap<FString, TSharedPtr<FUntestRunnerTest>> NameToTests;

	FUntestRunnerOptions Options;
	FUntestSearchFilter TestFilter;
	TArray<TSharedPtr<FUntestRunnerTest>> FilteredRootTests;
};

void SUntestRunner::Construct(const FArguments& InArgs)
{
	CommandList = MakeShared<FUICommandList>();

	RefreshAvailableTests();

	// clang-format off
	TestTable = SNew(STreeView<TSharedPtr<FUntestRunnerTest>>)
		.SelectionMode(ESelectionMode::Multi)
		.TreeItemsSource(&FilteredRootTests)
		.OnGenerateRow(this, &SUntestRunner::OnGenerateRow)
		.OnGetChildren(this, &SUntestRunner::OnGetChildren)
		.OnSetExpansionRecursive(this, &SUntestRunner::OnTestExpansionRecursive)
		.HeaderRow
		(
			SNew(SHeaderRow)

			+ SHeaderRow::Column( UIConstants::Checked )
			.FixedWidth(30.0f)
			.HAlignHeader(HAlign_Center)
			.VAlignHeader(VAlign_Center)
			.HAlignCell(HAlign_Center)
			.VAlignCell(VAlign_Center)
			[
				// global queue/dequeue check box
				SNew(SCheckBox)
				.OnCheckStateChanged(this, &SUntestRunner::OnGlobalCheckboxStateChange)
				.ToolTipText(LOCTEXT("Untest.Header.TestQueued", "Queue/dequeue test for next run"))
			]

			+ SHeaderRow::Column( UIConstants::Disabled )
			.FixedWidth(24.0f)
			.HAlignHeader(HAlign_Center)
			.VAlignHeader(VAlign_Center)
			.HAlignCell(HAlign_Center)
			.VAlignCell(VAlign_Center)
			[
				SNew(SImage)
				.Image(FUntestStyle::Get().GetBrush(TEXT("Untest.Disabled")))
				.ToolTipText(LOCTEXT("Untest.Header.Disabled.Tooltip", "Is Test Disabled"))
			]

			+ SHeaderRow::Column( UIConstants::Title )
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew( STextBlock )
					.Text( LOCTEXT("Untest.Header.Title", "Test") )
				]
			]

			+ SHeaderRow::Column( UIConstants::Duration )
			.FixedWidth(100.0f)
			.HAlignHeader(HAlign_Center)
			.VAlignHeader(VAlign_Center)
			.HAlignCell(HAlign_Center)
			.VAlignCell(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Untest.Header.Duration", "Duration (ms)"))
			]

			+ SHeaderRow::Column( UIConstants::StatusIcon )
			.FixedWidth(60.0f)
			.HAlignHeader(HAlign_Center)
			.VAlignHeader(VAlign_Center)
			.HAlignCell(HAlign_Center)
			.VAlignCell(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Untest.Header.Status", "Status"))
			]

		);

	this->ChildSlot
	[

		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		[
			SNew(SSplitter)
			.Orientation(Orient_Vertical)
			+ SSplitter::Slot()
			.Value(0.66f)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.FillHeight(1.0f)
				.Padding(0.0f, 4.0f, 0.0f, 0.0f)
				[
					SNew(SOverlay)
					+SOverlay::Slot()
					[
						SNew(SBorder)
						.Padding(3)
						[
							SNew(SBox)
							.Padding(4)
							[
								SNew(SVerticalBox)

								+SVerticalBox::Slot()
								.AutoHeight()
								.Padding(0.0f, 0.0f, 0.0f, 4.0f)
								[
									SNew(SHorizontalBox)

									+SHorizontalBox::Slot()
									.AutoWidth()
									.VAlign(VAlign_Center)
									[
										SNew(SBox)
										.WidthOverride(105.0f) // enough space for the string "Run 99999"
										[
											SNew( SButton )
											.ToolTipText( LOCTEXT( "Untest.RunAbortTestsButton", "Run / Abort tests" ) )
											.OnClicked( this, &SUntestRunner::RunOrAbortTests )
											.ContentPadding(0)
											[
												SNew(SHorizontalBox)
												+ SHorizontalBox::Slot()
												.AutoWidth()
												[
													SNew( SImage )
													.Image( this, &SUntestRunner::GetRunOrAbortTestsIcon )
												]
												+ SHorizontalBox::Slot()
												.VAlign(VAlign_Center)
												[
													SNew( STextBlock )
													.Text( this, &SUntestRunner::GetRunOrAbortTestsLabel )
													.ColorAndOpacity(FSlateColor::UseForeground())
												]
											]
										]
									]

									+SHorizontalBox::Slot()
									.MaxWidth(80.0f)
									.Padding(4.0f, 0.0f, 0.0f, 0.0f)
									[
										SNew(SComboButton)
										.ComboButtonStyle(FAppStyle::Get(), "SimpleComboButton")
										.ToolTipText(LOCTEXT("Untest.Options.Description", "Set test run options"))
										.IsEnabled( this, &SUntestRunner::AreNoTestsRunning )
										.ButtonContent()
										[
											SNew(STextBlock)
											.Text(LOCTEXT("Untest.Options.Label", "Options"))
										]
										.MenuContent()
										[
											SNew(SVerticalBox)
											+SVerticalBox::Slot()
											.Padding(FMargin(4.0f, 4.0f))
											.AutoHeight()
											[
												SNew(SCheckBox)
												.IsChecked(this, &SUntestRunner::IsTimeoutEnabled)
												.OnCheckStateChanged(this, &SUntestRunner::OnTimeoutCheckStateChanged)
												.Padding(FMargin(4.0f, 0.0f))
												.ToolTipText(LOCTEXT("Untest.Options.CanTimeout.Tooltip", "Controls if tests can timeout or if they run to completion."))
												.IsEnabled( this, &SUntestRunner::AreNoTestsRunning )
												.Content()
												[
													SNew(STextBlock)
													.Text(LOCTEXT("Untest.Options.CanTimeout.Label", "Can Timeout"))
												]
											]

											+SVerticalBox::Slot()
											.Padding(FMargin(4.0f, 4.0f))
											.AutoHeight()
											[
												SNew(SCheckBox)
												.IsChecked(this, &SUntestRunner::IncludeDisabled)
												.OnCheckStateChanged(this, &SUntestRunner::OnIncludeDisabledCheckStateChanged)
												.Padding(FMargin(4.0f, 0.0f))
												.ToolTipText(LOCTEXT("Untest.Options.IncludeDisabled.Tooltip", "Controls if disabled tests are run."))
												.IsEnabled( this, &SUntestRunner::AreNoTestsRunning )
												.Content()
												[
													SNew(STextBlock)
													.Text(LOCTEXT("Untest.Options.IncludeDisabled.Label", "Include Disabled Tests"))
												]
											]
										]
									]

									+SHorizontalBox::Slot()
									.FillWidth(1.0f)
									.VAlign(VAlign_Center)
									.Padding(2.0f, 0, 0, 0)
									[
										SAssignNew(TestNameSearchBox, SSearchBox)
										.ToolTipText(LOCTEXT("Untest.FilterTests", "Filter tests by name"))
										.HintText(LOCTEXT("Untest.FilterTestsHint", "Filter"))
										.OnTextChanged(this, &SUntestRunner::OnFilterTextChanged)
										.IsEnabled(this, &SUntestRunner::AreNoTestsRunning)
									]
								]

								+SVerticalBox::Slot()
								.FillHeight(1.0f)
								[
									TestTable.ToSharedRef()
								]
							]
						]
					]
				]
			]
			+SSplitter::Slot()
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Untest.TestResults", "Test Results:"))
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SSeparator)
				]
				+SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					SNew(SScrollBox)
					.Orientation(Orient_Vertical)
					+ SScrollBox::Slot()
					[
						SAssignNew(ResultsText, SMultiLineEditableText)
						.IsReadOnly(true)
						.Text(this, &SUntestRunner::GetTestResultsText)
					]
				]
			]
		]

		+SVerticalBox::Slot()
		.AutoHeight()
		.VAlign(VAlign_Bottom)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			// .Padding(10)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				// .Padding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
				.Padding(4,4,10,4)
				[
					SNew(STextBlock)
					.Text(this, &SUntestRunner::GetStatusText)
					.Font(FAppStyle::GetFontStyle("StandardDialog.SmallFont"))
				]

				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Right)
				.Padding(4, 4, 10, 4)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					[
						SNew(SHorizontalBox)
						.Visibility(this, &SUntestRunner::StatusProgressVisibility)
						+SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SSeparator)
							.Orientation(Orient_Vertical)
						]
						+SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(6, 0, 0, 0)
						[
							SNew(SBox)
							.WidthOverride(150.0f)
							[
								SNew(SProgressBar)
								.Percent(this, &SUntestRunner::StatusProgress)
							]
						]
					]

					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(6, 0, 0, 0)
						[
							SNew(SSeparator)
							.Orientation(Orient_Vertical)
						]
						+SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(6, 0, 0, 0)
						[
							SNew(SImage)
							.Image(this, &SUntestRunner::GetAllTestResultsStatusIcon)
						]
					]
				]
			]
		]
	];
	// clang-format on
}

FText SUntestRunner::GetStatusText() const
{
	if (AreTestsRunning())
	{
		int32 NumEnabledTests = GetNumTestsQueued();
		return FText::Format(LOCTEXT("Untest.StatusBarText", "Test Runner: Running {0} / {1}"), NumCompletedTests + 1, NumEnabledTests);
	}

	return LOCTEXT("Untest.StatusBarTextIdle", "Test Runner: Idle");
}

EVisibility SUntestRunner::StatusProgressVisibility() const
{
	if (AreTestsRunning())
	{
		return EVisibility::Visible;
	}
	else
	{
		return EVisibility::Hidden;
	}
}

TOptional<float> SUntestRunner::StatusProgress() const
{
	int32 NumEnabled = GetNumTestsQueued();
	if (NumEnabled == 0)
	{
		return 0.0f;
	}

	float Progress = static_cast<float>(NumCompletedTests) / NumEnabled;
	return Progress;
}

const FSlateBrush* SUntestRunner::GetAllTestResultsStatusIcon() const
{
	FName BrushName;
	if (NumCompletedTests > 0)
	{
		bool bAllSuccessful = true;
		for (const TSharedPtr<FUntestRunnerTest>& Test : AllTests)
		{
			if (Test->Results.IsSet() && Test->Results->Errors.Num() > 0)
			{
				bAllSuccessful = false;
				break;
			}
		}

		if (bAllSuccessful)
		{
			BrushName = FName(TEXT("Untest.Success"));
		}
		else
		{
			BrushName = FName(TEXT("Untest.Fail"));
		}
	}
	else
	{
		BrushName = FName(TEXT("Untest.NotRun"));
	}
	return FUntestStyle::Get().GetBrush(BrushName);
}

TSharedRef<ITableRow> SUntestRunner::OnGenerateRow(TSharedPtr<FUntestRunnerTest> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SUntestRow, OwnerTable)
		.Test(InItem)
		.ColumnWidth(50.0f)
		.HighlightText(this, &SUntestRunner::GetHighlightText)
		.OnCheckedStateChanged(this, &SUntestRunner::OnItemCheckboxStateChanged);
}

void SUntestRunner::OnGetChildren(TSharedPtr<FUntestRunnerTest> InItem, TArray<TSharedPtr<FUntestRunnerTest>>& OutItems)
{
	OutItems = InItem->FilteredChildren;
}

void SUntestRunner::OnTestExpansionRecursive(TSharedPtr<FUntestRunnerTest> Test, bool bInIsItemExpanded)
{
	if (Test.IsValid())
	{
		TestTable->SetItemExpansion(Test, bInIsItemExpanded);

		for (TSharedPtr<FUntestRunnerTest>& Child : Test->FilteredChildren)
		{
			OnTestExpansionRecursive(Child, bInIsItemExpanded);
		}
	}
}

void SUntestRunner::OnGlobalCheckboxStateChange(ECheckBoxState InCheckboxState)
{
	const bool bIsQueued = InCheckboxState == ECheckBoxState::Checked;
	for (TSharedPtr<FUntestRunnerTest>& Test : AllTests)
	{
		Test->SetQueued(bIsQueued);
	}
}

FText SUntestRunner::GetHighlightText() const
{
	if (TestNameSearchBox.IsValid())
	{
		return TestNameSearchBox->GetText();
	}
	return FText();
}

void SUntestRunner::OnItemCheckboxStateChanged(TSharedPtr<FUntestRunnerTest> Test)
{
	// If multiple rows selected then handle all the rows
	if (TestTable->GetSelectedItems().Num() > 1)
	{
		bool bIsRowSelected = false;
		for (const TSharedPtr<FUntestRunnerTest>& SelectedTest : TestTable->GetSelectedItems())
		{
			if (SelectedTest == Test)
			{
				if (Test.IsValid())
				{
					bIsRowSelected = true;
				}
				break;
			}
		}

		if (bIsRowSelected)
		{
			struct Local
			{
				static void AddAllFilteredTests(const TArray<TSharedPtr<FUntestRunnerTest>>& Tests, TArray<TSharedPtr<FUntestRunnerTest>>& Out)
				{
					for (const TSharedPtr<FUntestRunnerTest>& Test : Tests)
					{
						if (Out.Contains(Test) == false)
						{
							Out.Emplace(Test);
							AddAllFilteredTests(Test->FilteredChildren, Out);
						}
					}
				}
			};

			TArray<TSharedPtr<FUntestRunnerTest>> SelectedTests;
			SelectedTests.Reserve(AllTests.Num());
			Local::AddAllFilteredTests(TestTable->GetSelectedItems(), SelectedTests);

			bool bIsQueued = !Test->bIsQueued;
			for (TSharedPtr<FUntestRunnerTest>& SelectedTest : SelectedTests)
			{
				SelectedTest->bIsQueued = bIsQueued;
			}
		}
		else
		{
			TestTable->SetSelection(Test, ESelectInfo::Direct);
			Test->SetQueued(!Test->bIsQueued);
		}
	}
	else
	{
		Test->SetQueued(!Test->bIsQueued);
	}
}

FReply SUntestRunner::RunOrAbortTests()
{
	FUntestModule& Module = FUntestModule::Get();

	if (Module.HasRunningTests())
	{
		Module.StopTests();
	}
	else
	{
		TArray<FString> TestNames;
		for (TSharedPtr<FUntestRunnerTest>& Test : AllTests)
		{
			Test->Results.Reset();
			if (Test->bIsQueued && Test->Children.Num() == 0)
			{
				TestNames.Emplace(Test->Name);
			}
		}

		NumCompletedTests = 0;
		NumFailedTests = 0;
		ResultsText->SetText(FText());

		auto OnTestCompleteDelegate = FBVOnTestComplete::CreateRaw(this, &SUntestRunner::OnTestComplete);
		auto OnAllTestsCompleteDelegate = FBVOnAllTestsComplete::CreateRaw(this, &SUntestRunner::OnAllTestsComplete);

		FUntestRunOpts RunOpts;
		RunOpts.bNoTimeouts = Options.bIsTimeoutEnabled == false;
		RunOpts.bIncludeDisabled = Options.bIncludeDisabled;
		RunOpts.OnTestComplete = OnTestCompleteDelegate;
		RunOpts.OnAllTestsComplete = OnAllTestsCompleteDelegate;
		Module.QueueTests(TestNames, RunOpts);
	}

	return FReply::Handled();
}

const FSlateBrush* SUntestRunner::GetRunOrAbortTestsIcon() const
{
	FString Brush = TEXT("Untest");
	FUntestModule& Module = FUntestModule::Get();
	if (Module.HasRunningTests())
	{
		Brush += TEXT(".StopTests"); // Temporary brush type for stop tests
	}
	else
	{
		Brush += TEXT(".RunTests");
	}

	Brush += TEXT(".Small");

	return FUntestStyle::Get().GetBrush(*Brush);
}

FText SUntestRunner::GetRunOrAbortTestsLabel() const
{
	FUntestModule& Module = FUntestModule::Get();
	if (Module.HasRunningTests())
	{
		return LOCTEXT("Untest.StopTestsLabel", "Abort");
	}
	else
	{
		FText NumTestsQueued = OnGetNumQueuedTestsString();
		return FText(FText::Format(LOCTEXT("Untest.StartTestsLabel", "Run {0}"), NumTestsQueued));
	}
}

EVisibility SUntestRunner::GetLargeToolBarVisibility() const
{
	return FMultiBoxSettings::UseSmallToolBarIcons.Get() ? EVisibility::Collapsed : EVisibility::Visible;
}

FText SUntestRunner::OnGetNumQueuedTestsString() const
{
	return FText::AsNumber(GetNumTestsQueued());
}

void SUntestRunner::OnFilterTextChanged(const FText& InFilterText)
{
	TestFilter.SearchName = InFilterText.ToString();

	RefreshAvailableTests();

	struct Local
	{
		static bool ExpandMatchingFilter(const FString& Filter, const TSharedPtr<FUntestRunnerTest>& Test, TSharedPtr<STreeView<TSharedPtr<FUntestRunnerTest>>>& TestTable)
		{
			bool bExpand = Filter.IsEmpty() ? false : Test->Name.Contains(Filter);

			for (const TSharedPtr<FUntestRunnerTest>& Child : Test->FilteredChildren)
			{
				bExpand |= Local::ExpandMatchingFilter(Filter, Child, TestTable);
			}

			TestTable->SetItemExpansion(Test, bExpand);

			return bExpand;
		}
	};

	for (const TSharedPtr<FUntestRunnerTest>& Test : FilteredRootTests)
	{
		Local::ExpandMatchingFilter(TestFilter.SearchName, Test, TestTable);
	}
}

bool SUntestRunner::AreTestsRunning() const
{
	FUntestModule& Module = FUntestModule::Get();
	return Module.HasRunningTests();
}

bool SUntestRunner::AreNoTestsRunning() const
{
	return !AreTestsRunning();
}

ECheckBoxState SUntestRunner::IsTimeoutEnabled() const
{
	return Options.bIsTimeoutEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SUntestRunner::OnTimeoutCheckStateChanged(ECheckBoxState CheckBoxState)
{
	Options.bIsTimeoutEnabled = CheckBoxState != ECheckBoxState::Unchecked;
}

ECheckBoxState SUntestRunner::IncludeDisabled() const
{
	return Options.bIncludeDisabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SUntestRunner::OnIncludeDisabledCheckStateChanged(ECheckBoxState CheckBoxState)
{
	Options.bIncludeDisabled = CheckBoxState != ECheckBoxState::Unchecked;
}

FText SUntestRunner::GetTestResultsText() const
{
	int32 NumTestsWithResults = 0;

	TStringBuilder<1024> TextBuilder;
	for (const TSharedPtr<FUntestRunnerTest>& Test : AllTests)
	{
		if ((Test->IsParent() == false) && Test->Results.IsSet())
		{
			++NumTestsWithResults;

			const TCHAR* ResultStr = UntestResultStr(Test->Results->Result);
			TextBuilder.Appendf(TEXT("%s: %s\n"), *Test->Name, ResultStr);

			for (const FString& Error : Test->Results->Errors)
			{
				TextBuilder.Appendf(TEXT("\t%s\n"), *Error);
			}
		}
	}

	if (NumTestsWithResults == 0)
	{
		return FText(LOCTEXT("Untest.EmptyResultsText", "Run tests to see results here."));
	}

	return FText::FromString(TextBuilder.ToString());
}

void SUntestRunner::OnTestComplete(const FUntestResults& Results)
{
	if (TSharedPtr<FUntestRunnerTest>* TestPtr = NameToTests.Find(Results.TestName.ToFull()))
	{
		TSharedPtr<FUntestRunnerTest> Test = *TestPtr;
		Test->Results = Results;

		ResultsText->SetText(GetTestResultsText());
		++NumCompletedTests;

		if (Results.Errors.Num() > 0)
		{
			++NumFailedTests;
		}

		FUntestRunnerTest* Category = Test->Parent;
		FUntestRunnerTest* Module = Category->Parent;
		FUntestRunnerTest* Parents[] = { Category, Module };

		for (FUntestRunnerTest* Parent : Parents)
		{
			if (Parent->Results.IsSet() == false)
			{
				Parent->Results = FUntestResults();
			}
			Parent->Results->DurationMs += Test->Results->DurationMs;
			Parent->Results->Errors.Append(Test->Results->Errors);
		}
	}
}

void SUntestRunner::OnAllTestsComplete(TArrayView<const FUntestResults> AllResults)
{
	// TODO update UI?
}

int32 SUntestRunner::GetNumTestsQueued() const
{
	int32 NumTestsQueued = 0;
	for (const TSharedPtr<FUntestRunnerTest>& Test : AllTests)
	{
		NumTestsQueued += (!Test->IsParent() && Test->bIsQueued) ? 1 : 0;
	}
	return NumTestsQueued;
}

void SUntestRunner::RefreshAvailableTests()
{
	if (AllTests.IsEmpty())
	{
		if (FUntestModule* Module = FUntestModule::GetSafe())
		{
			TArray<FUntestInfo> TestInfos = Module->FindTests(FUntestSearchFilter());

			const int32 ReservationSize = static_cast<int32>(float(TestInfos.Num()) * 1.2f); // extra .2 tries to account for parent rows
			AllTests.Reserve(ReservationSize);
			NameToTests.Reserve(ReservationSize);

			for (const FUntestInfo& Info : TestInfos)
			{
				TSharedPtr<FUntestRunnerTest> Test = MakeShared<FUntestRunnerTest>();
				Test->Name = Info.Name.ToFull();
				Test->DisplayName = FText::FromString(Info.Name.Test);
				Test->TestType = Info.TestType;
				Test->bIsDisabled = Info.Opts.IsSet(EUntestFlags::Disabled);

				const FString QualifiedCategoryName = FString::Printf(TEXT("%s.%s"), *Info.Name.Module, *Info.Name.Category);
				TSharedPtr<FUntestRunnerTest>* CategoryParent = NameToTests.Find(QualifiedCategoryName);
				if (CategoryParent == nullptr)
				{
					CategoryParent = &NameToTests.Emplace(QualifiedCategoryName, MakeShared<FUntestRunnerTest>());
					(*CategoryParent)->Name = QualifiedCategoryName;
					(*CategoryParent)->DisplayName = FText::FromString(Info.Name.Category);
					AllTests.Emplace(*CategoryParent);
				}
				(*CategoryParent)->Children.Emplace(Test);
				Test->Parent = (*CategoryParent).Get();

				TSharedPtr<FUntestRunnerTest>* ModuleParent = NameToTests.Find(Info.Name.Module);
				if (ModuleParent == nullptr)
				{
					ModuleParent = &NameToTests.Emplace(Info.Name.Module, MakeShared<FUntestRunnerTest>());
					(*ModuleParent)->Name = Info.Name.Module;
					(*ModuleParent)->DisplayName = FText::FromString(Info.Name.Module);
					AllTests.Emplace(*ModuleParent);
					RootTests.Emplace(*ModuleParent);
				}

				(*ModuleParent)->Children.AddUnique(*CategoryParent);
				(*CategoryParent)->Parent = (*ModuleParent).Get();

				AllTests.Emplace(Test);
				NameToTests.Emplace(Test->Name, Test);
			}
		}
	}

	FilteredRootTests.Reset();

	struct Local
	{
		static void ApplyTestFilter(const FUntestSearchFilter& Filter, const TArray<TSharedPtr<FUntestRunnerTest>>& Tests, TArray<TSharedPtr<FUntestRunnerTest>>& FilteredTests)
		{
			FilteredTests.Reset();
			FilteredTests.Reserve(Tests.Num());

			for (const TSharedPtr<FUntestRunnerTest>& Test : Tests)
			{
				if (Test->Children.Num() > 0)
				{
					ApplyTestFilter(Filter, Test->Children, Test->FilteredChildren);
					if (Test->FilteredChildren.Num() == 0)
					{
						continue;
					}
				}
				else if ((Filter.SearchName.IsEmpty() == false) && (Test->Name.Contains(Filter.SearchName) == false))
				{
					continue;
				}

				FilteredTests.Emplace(Test);
			}
		}
	};

	Local::ApplyTestFilter(TestFilter, RootTests, FilteredRootTests);

	if (TestTable)
	{
		TestTable->RequestTreeRefresh();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// FUntestUI

FUntestUI::FUntestUI()
{
	if (FSlateApplication::IsInitialized())
	{
		FTabSpawnerEntry& Entry = FGlobalTabmanager::Get()->RegisterNomadTabSpawner(NAME_UntestTab, FOnSpawnTab::CreateRaw(this, &FUntestUI::OnSpawnTab));
		Entry.SetDisplayName(LOCTEXT("Untest.UITabTitle", "Untest Runner"));
		Entry.SetMenuType(ETabSpawnerMenuType::Enabled);
		Entry.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory());
		Entry.SetIcon(GetIcon());
	}
}

FUntestUI::~FUntestUI()
{
	if (FSlateApplication::IsInitialized())
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(NAME_UntestTab);
	}
}

// void FUntestUI::OpenTab() const
// {
// 	FGlobalTabmanager::Get()->TryInvokeTab(NAME_UntestTab);
// }

// bool FUntestUI::IsTabOpen() const
// {
// 	return FGlobalTabmanager::Get()->FindExistingLiveTab(NAME_UntestTab) != nullptr;
// }

TSharedRef<SDockTab> FUntestUI::OnSpawnTab(const FSpawnTabArgs& SpawnTabArgs)
{
	// clang-format off
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SUntestRunner)
		];
	// clang-format on
}

#undef LOCTEXT_NAMESPACE

#else // WITH_EDITOR

FUntestUI::FUntestUI() {}
FUntestUI::~FUntestUI() {}

#endif // WITH_EDITOR
