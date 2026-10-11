#include "NBPlayerState.h"
#include "Net/UnrealNetwork.h"

ANBPlayerState::ANBPlayerState()
{
	bReplicates = true; // 이 Actor의 서버 상태 복제를 활성화함
}

void ANBPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const // UE에 복제할 속성을 등록함
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ANBPlayerState, CurrentAttemptCount); // 등록한 속성을 서버에서 클라이언트로 복제함
	DOREPLIFETIME(ANBPlayerState, MaxAttemptCount);
}

int32 ANBPlayerState::GetCurrentAttemptCount() const // 이미 사용한 시도 횟수를 반환함
{
	return CurrentAttemptCount;
}

int32 ANBPlayerState::GetMaxAttemptCount() const // 최대 시도 횟수를 반환함
{
	return MaxAttemptCount;
}

bool ANBPlayerState::HasAttemptsRemaining() const // 사용 횟수가 최대 횟수보다 작은지 확인함
{
	return CurrentAttemptCount < MaxAttemptCount;
}

void ANBPlayerState::ResetAttempts() // 서버에서 새 라운드의 시도 횟수를 초기화함
{
	if (!HasAuthority()) // 서버 권한이 없는 환경에서는 상태를 수정하지 않음
	{
		return;
	}

	CurrentAttemptCount = 0;
	MaxAttemptCount = 3;
	ForceNetUpdate(); // 다음 복제 갱신을 앞당기도록 요청함; 즉시 도착 보장은 아님
}

bool ANBPlayerState::TryConsumeAttempt() // 서버에서 남은 시도 한 번을 소비하고 성공 여부를 반환함
{
	if (!HasAuthority() || !HasAttemptsRemaining()) // 서버가 아니거나 시도를 다 썼으면 소비 실패
	{
		return false;
	}

	++CurrentAttemptCount;
	ForceNetUpdate(); // 다음 복제 갱신을 앞당기도록 요청함; 즉시 도착 보장은 아님
	return true;
}
