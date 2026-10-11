#include "NBGameState.h"
#include "NBPlayerController.h"
#include "NBPlayerState.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

ANBGameState::ANBGameState()
{
	bReplicates = true; // 이 Actor의 서버 상태 복제를 활성화함
	bAlwaysRelevant = true; // 거리와 관계없이 네트워크 관련 대상으로 유지함
}

void ANBGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const // UE에 복제할 속성을 등록함
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ANBGameState, PublicState); // 등록한 속성을 서버에서 클라이언트로 복제함
}

const FNBPublicGameState& ANBGameState::GetPublicState() const // UI가 읽을 공개 게임 상태를 반환함
{
	return PublicState;
}

double ANBGameState::GetRemainingTurnSeconds() const // 서버 기준 종료 시각으로 남은 초를 계산함
{
	if (!PublicState.bRoundActive || !IsValid(PublicState.ActivePlayer.Get()))
	{
		return 0.0;
	}

	return FMath::Max(0.0, PublicState.TurnEndServerTime - GetServerWorldTimeSeconds()); // 동기화된 서버 시간을 빼고 음수는 0으로 제한함
}

void ANBGameState::SetPublicState(const FNBPublicGameState& NewState) // 서버에서 공개 상태를 갱신해 복제되도록 함
{
	if (!HasAuthority()) // 서버 권한이 없는 환경에서는 상태를 수정하지 않음
	{
		return;
	}

	PublicState = NewState;
	ForceNetUpdate(); // 다음 복제 갱신을 앞당기도록 요청함; 즉시 도착 보장은 아님
}

void ANBGameState::MulticastRPCBroadcastMessage_Implementation(const FString& Message) // 서버가 호출해 각 수신 환경의 로컬 UI에 메시지를 전달함
{
	if (!GetWorld() || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	for (TActorIterator<ANBPlayerController> It(GetWorld()); It; ++It)
	{
		ANBPlayerController* PC = *It;
		if (IsValid(PC) && PC->IsLocalController()) // 유효한 로컬 Controller에만 화면 메시지를 전달함
		{
			PC->DisplayChatMessage(Message);
		}
	}
}
