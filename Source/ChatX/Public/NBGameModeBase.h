#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TimerManager.h"
#include "NBGameModeBase.generated.h"

class ANBPlayerController;
class ANBPlayerState;

UCLASS() // GameMode는 서버에만 존재하며 게임 규칙과 정답을 관리함
class CHATX_API ANBGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	ANBGameModeBase();

	virtual void BeginPlay() override; // 플레이 시작 시 클래스에 필요한 초기 설정을 실행함
	virtual void PostLogin(APlayerController* NewPlayer) override; // 서버에서 새 참가자를 등록하고 라운드 시작을 시도함
	virtual void Logout(AController* Exiting) override; // 서버에서 퇴장한 참가자를 제거하고 턴을 조정함
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override; // 플레이 종료 시 타이머 또는 UI 자원을 정리함
	void ReceiveChat(APlayerController* Sender, const FString& Message, int32 ExpectedTurnId); // 서버에서 채팅 명령 또는 숫자 추측을 검증하고 처리함

private:
	void StartRound(); // 정답과 시도 횟수를 준비하고 새 라운드를 시작함
	void StartNextTurn(); // 시도가 남은 다음 플레이어를 찾아 턴과 타이머를 설정함
	void HandleTurnTimeout(int32 ExpiredTurnId); // 현재 턴의 만료를 확인하고 시도를 소비한 뒤 넘김
	void EndRound(const FString& ResultMessage); // 결과를 공개하고 일정 시간 뒤 다음 라운드를 예약함
	void WaitForPlayers(); // 타이머와 정답을 지우고 참가자를 기다리는 상태로 바꿈
	FString GetPlayerDisplayName(const ANBPlayerState* PlayerState) const; // 이름이 비어 있으면 플레이어 ID로 표시 이름을 만듦
	FString GetNetModeString() const; // 실행 중인 네트워크 모드를 로그용 문자열로 바꿈

	FString Answer; // 서버 전용 GameMode에 보관하는 정답

	UPROPERTY()
	TArray<TObjectPtr<ANBPlayerController>> Players; // 서버가 관리하는 참가 Controller 목록

	int32 CurrentPlayerIndex = INDEX_NONE; // 현재 참가자 배열 위치; INDEX_NONE은 선택 전 상태
	int32 TurnId = 0; // 오래된 제출·타이머를 구분하는 턴 번호
	int32 RoundId = 0; // 라운드 식별 번호
	bool bReadyToStart = false; // BeginPlay 이후 라운드 시작이 가능한지 나타냄
	bool bTurnResolved = true; // 제출과 시간 초과의 중복 처리를 막는 완료 표시
	bool bHasSubmittedThisTurn = false; // 현재 턴의 제출이 이미 처리됐는지 나타냄
	FTimerHandle TurnTimerHandle; // 턴 시간 초과 타이머를 취소·재설정할 때 쓰는 핸들
	FTimerHandle RestartTimerHandle; // 결과 표시 후 라운드 재시작 예약을 관리하는 핸들

	UPROPERTY(EditDefaultsOnly, Category = "NumberBaseball", meta = (ClampMin = "0.1"))
	float TurnDuration = 30.0f; // 한 턴의 제한 시간(초)

	UPROPERTY(EditDefaultsOnly, Category = "NumberBaseball", meta = (ClampMin = "0.1"))
	float ResultDisplayDuration = 3.0f; // 결과를 보여준 뒤 재시작까지 기다리는 시간(초)
};
