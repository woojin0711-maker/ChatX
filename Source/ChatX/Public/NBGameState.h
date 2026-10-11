#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "NBGameState.generated.h"

class ANBPlayerState;

USTRUCT()
struct CHATX_API FNBPublicGameState
{
	GENERATED_BODY()

	UPROPERTY()
	int32 RoundId = 0; // 라운드 식별 번호

	UPROPERTY()
	int32 TurnId = 0; // 오래된 제출·타이머를 구분하는 턴 번호

	UPROPERTY()
	TObjectPtr<ANBPlayerState> ActivePlayer = nullptr; // 현재 차례인 PlayerState 참조

	UPROPERTY()
	double TurnEndServerTime = 0.0; // 서버 시간 기준으로 저장한 턴 종료 시각

	UPROPERTY()
	bool bRoundActive = false; // 추측을 받는 라운드가 진행 중인지 나타냄

	UPROPERTY()
	FString ResultMessage; // 라운드 종료 결과 표시 문구
};

UCLASS() // 전체 참가자가 볼 공개 게임 상태를 복제함
class CHATX_API ANBGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ANBGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override; // UE에 복제할 속성을 등록함

	const FNBPublicGameState& GetPublicState() const; // UI가 읽을 공개 게임 상태를 반환함
	double GetRemainingTurnSeconds() const; // 서버 기준 종료 시각으로 남은 초를 계산함
	void SetPublicState(const FNBPublicGameState& NewState); // 서버에서 공개 상태를 갱신해 복제되도록 함

	UFUNCTION(NetMulticast, Reliable) // 서버 호출을 서버와 관련 클라이언트에서 실행; 즉시 도착을 뜻하지 않음
	void MulticastRPCBroadcastMessage(const FString& Message); // 서버가 호출해 각 수신 환경의 로컬 UI에 메시지를 전달함

private:
	UPROPERTY(Replicated)
	FNBPublicGameState PublicState; // 서버에서 클라이언트로 복제하는 공개 상태 묶음
};
