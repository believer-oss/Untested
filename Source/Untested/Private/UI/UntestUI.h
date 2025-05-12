#pragma once

class SDockTab;
class FSpawnTabArgs;
class FUICommandList;
class SWidget;
class FReply;

struct FUntestInfo;
struct FUntestResults;

struct FUntestUI //: public TSharedFromThis<FUntestUI>
{
	FUntestUI();
	~FUntestUI();

private:
	TSharedRef<SDockTab> OnSpawnTab(const FSpawnTabArgs& SpawnTabArgs);
};
