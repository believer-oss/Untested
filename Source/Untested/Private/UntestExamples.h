#pragma once

#include "GameFramework/PlayerController.h"
#include "GameFramework/GameModeBase.h"
#include "UntestExamples.generated.h"

UCLASS()
class AUntestExampleGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AUntestExampleGameMode();
};

UCLASS()
class AUntestExamplePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AUntestExamplePlayerController();

	UFUNCTION()
	void OnRep_ReplicatedInt();

	UFUNCTION(Client, Reliable)
	void ClientRPC();

	UFUNCTION(Server, Reliable)
	void ServerRPC();

	UFUNCTION(NetMulticast, Reliable)
	void NetMulticastRPC();

	UPROPERTY(ReplicatedUsing = OnRep_ReplicatedInt)
	int32 ReplicatedInt = 0;

	bool bWasOnRepCalled = 0;
	bool bWasServerRpcCalled = false;
	bool bWasClientRpcCalled = false;
	bool bWasMulticastCalled = false;
};
