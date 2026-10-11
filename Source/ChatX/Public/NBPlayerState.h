#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "NBPlayerState.generated.h"

UCLASS() // 플레이어별 상태를 서버에서 관리하고 클라이언트에 복제함
class CHATX_API ANBPlayerState : public APlayerState
{
	GENERATED_BODY()

private:
	UPROPERTY(Replicated)
	int32 CurrentAttemptCount = 0; // 서버가 관리하고 클라이언트로 복제하는 사용 횟수

	UPROPERTY(Replicated)
	int32 MaxAttemptCount = 3; // 클라이언트에도 복제되는 최대 시도 횟수

public:
	ANBPlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override; // UE에 복제할 속성을 등록함

	UFUNCTION(BlueprintPure, Category = "NumberBaseball")
	int32 GetCurrentAttemptCount() const; // 이미 사용한 시도 횟수를 반환함

	UFUNCTION(BlueprintPure, Category = "NumberBaseball")
	int32 GetMaxAttemptCount() const; // 최대 시도 횟수를 반환함

	UFUNCTION(BlueprintPure, Category = "NumberBaseball")
	bool HasAttemptsRemaining() const; // 사용 횟수가 최대 횟수보다 작은지 확인함

	void ResetAttempts(); // 서버에서 새 라운드의 시도 횟수를 초기화함
	bool TryConsumeAttempt(); // 서버에서 남은 시도 한 번을 소비하고 성공 여부를 반환함
};
