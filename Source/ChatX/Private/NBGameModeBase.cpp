#include "NBGameModeBase.h"
#include "NBGameState.h"
#include "NBNumberBaseballLibrary.h"
#include "NBPlayerController.h"
#include "NBPlayerState.h"

ANBGameModeBase::ANBGameModeBase()
{
	PlayerControllerClass = ANBPlayerController::StaticClass();
	PlayerStateClass = ANBPlayerState::StaticClass();
	GameStateClass = ANBGameState::StaticClass();
}

void ANBGameModeBase::BeginPlay() // 플레이 시작 시 클래스에 필요한 초기 설정을 실행함
{
	Super::BeginPlay();
	bReadyToStart = true;
	UE_LOG(LogTemp, Log, TEXT("Number Baseball BeginPlay. Net Mode: %s"), *GetNetModeString());
	StartRound();
}

void ANBGameModeBase::PostLogin(APlayerController* NewPlayer) // 서버에서 새 참가자를 등록하고 라운드 시작을 시도함
{
	Super::PostLogin(NewPlayer);
	ANBPlayerController* PC = Cast<ANBPlayerController>(NewPlayer);
	if (!HasAuthority() || !IsValid(PC) || Players.Contains(PC))
	{
		return;
	}

	Players.Add(PC);
	if (ANBPlayerState* PS = PC->GetPlayerState<ANBPlayerState>(); IsValid(PS))
	{
		PS->ResetAttempts();
	}

	if (!GetWorldTimerManager().TimerExists(RestartTimerHandle))
	{
		StartRound();
	}
}

void ANBGameModeBase::Logout(AController* Exiting) // 서버에서 퇴장한 참가자를 제거하고 턴을 조정함
{
	const int32 RemovedIndex = Players.IndexOfByKey(Cast<ANBPlayerController>(Exiting));
	if (HasAuthority() && RemovedIndex != INDEX_NONE)
	{
		const bool bActivePlayerLeft = RemovedIndex == CurrentPlayerIndex;
		Players.RemoveAt(RemovedIndex);
		if (RemovedIndex <= CurrentPlayerIndex)
		{
			--CurrentPlayerIndex;
		}

		if (Players.IsEmpty())
		{
			WaitForPlayers();
		}
		else if (bActivePlayerLeft)
		{
			bTurnResolved = true;
			GetWorldTimerManager().ClearTimer(TurnTimerHandle);
			ANBGameState* GS = GetGameState<ANBGameState>();
			if (IsValid(GS) && GS->GetPublicState().bRoundActive)
			{
				StartNextTurn();
			}
		}
	}
	Super::Logout(Exiting);
}

void ANBGameModeBase::EndPlay(const EEndPlayReason::Type EndPlayReason) // 플레이 종료 시 타이머 또는 UI 자원을 정리함
{
	bReadyToStart = false;
	bTurnResolved = true;
	GetWorldTimerManager().ClearTimer(TurnTimerHandle);
	GetWorldTimerManager().ClearTimer(RestartTimerHandle);
	Super::EndPlay(EndPlayReason);
}

void ANBGameModeBase::StartRound() // 정답과 시도 횟수를 준비하고 새 라운드를 시작함
{
	ANBGameState* GS = GetGameState<ANBGameState>();
	if (!HasAuthority() || !bReadyToStart || !IsValid(GS) || GS->GetPublicState().bRoundActive)
	{
		return;
	}

	bool bHasPlayer = false;
	for (ANBPlayerController* PC : Players)
	{
		ANBPlayerState* PS = IsValid(PC) ? PC->GetPlayerState<ANBPlayerState>() : nullptr;
		if (IsValid(PS))
		{
			PS->ResetAttempts();
			bHasPlayer = true;
		}
	}
	if (!bHasPlayer)
	{
		WaitForPlayers();
		return;
	}

	GetWorldTimerManager().ClearTimer(RestartTimerHandle);
	GetWorldTimerManager().ClearTimer(TurnTimerHandle);
	Answer = UNBNumberBaseballLibrary::GenerateRandomNumbers(); // 1~9에서 중복 없는 세 자리 정답을 만듦
	++RoundId;
	CurrentPlayerIndex = INDEX_NONE;
	UE_LOG(LogTemp, Log, TEXT("Number Baseball round %d. Server answer: %s"), RoundId, *Answer);
	StartNextTurn();
}

void ANBGameModeBase::StartNextTurn() // 시도가 남은 다음 플레이어를 찾아 턴과 타이머를 설정함
{
	ANBGameState* GS = GetGameState<ANBGameState>();
	if (!HasAuthority() || !IsValid(GS))
	{
		return;
	}

	bool bHasPlayer = false;
	for (int32 Offset = 1; Offset <= Players.Num(); ++Offset)
	{
		const int32 Index = (CurrentPlayerIndex + Offset) % Players.Num(); // 나머지 연산으로 배열 끝에서 처음으로 순환함
		ANBPlayerController* PC = Players[Index];
		ANBPlayerState* PS = IsValid(PC) ? PC->GetPlayerState<ANBPlayerState>() : nullptr;
		if (!IsValid(PS))
		{
			continue;
		}
		bHasPlayer = true;
		if (!PS->HasAttemptsRemaining()) // 시도를 모두 쓴 참가자는 다음 후보로 건너뜀
		{
			continue;
		}

		CurrentPlayerIndex = Index;
		++TurnId;
		bTurnResolved = false;
		bHasSubmittedThisTurn = false;
		const float Duration = FMath::IsFinite(TurnDuration) ? FMath::Max(0.1f, TurnDuration) : 30.0f;
		FNBPublicGameState State;
		State.RoundId = RoundId;
		State.TurnId = TurnId;
		State.ActivePlayer = PS;
		State.TurnEndServerTime = GS->GetServerWorldTimeSeconds() + Duration; // 서버 현재 시간에 제한 시간을 더해 마감 시각 저장
		State.bRoundActive = true;
		GS->SetPublicState(State);
		GetWorldTimerManager().SetTimer(TurnTimerHandle,
			FTimerDelegate::CreateUObject(this, &ANBGameModeBase::HandleTurnTimeout, TurnId), Duration, false); // 현재 턴 번호를 담아 일회성 만료 콜백을 예약함
		return;
	}

	if (bHasPlayer)
	{
		EndRound(TEXT("무승부! 모든 플레이어가 기회를 소진했습니다."));
	}
	else
	{
		WaitForPlayers();
	}
}

void ANBGameModeBase::ReceiveChat(APlayerController* Sender, const FString& Message, int32 ExpectedTurnId) // 서버에서 채팅 명령 또는 숫자 추측을 검증하고 처리함
{
	ANBPlayerController* PC = Cast<ANBPlayerController>(Sender);
	if (!HasAuthority() || !IsValid(PC))
	{
		return;
	}
	ANBGameState* GS = GetGameState<ANBGameState>();
	ANBPlayerState* PS = PC->GetPlayerState<ANBPlayerState>();
	if (!Players.Contains(PC) || !IsValid(GS) || !IsValid(PS))
	{
		PC->ClientRPCPrintChat(TEXT("게임 준비 중입니다. 잠시 후 다시 입력하세요."));
		return;
	}
	if (Message.Len() > 256) // 서버에서 메시지 최대 길이를 검사함
	{
		PC->ClientRPCPrintChat(TEXT("메시지는 256자 이하로 입력하세요."));
		return;
	}
	if (Message.StartsWith(TEXT("/say "), ESearchCase::CaseSensitive)) // 정확한 소문자 /say 명령이면 일반 채팅으로 처리함
	{
		const FString ChatText = Message.Mid(5).TrimStartAndEnd(); // 명령 접두사를 제거하고 양끝 공백을 정리함
		if (ChatText.IsEmpty())
		{
			PC->ClientRPCPrintChat(TEXT("/say 뒤에 채팅 내용을 입력하세요."));
			return;
		}
		GS->MulticastRPCBroadcastMessage(FString::Printf(TEXT("%s: %s"), *GetPlayerDisplayName(PS), *ChatText)); // 서버가 호출해 각 수신 환경의 로컬 UI에 메시지를 전달함
		return;
	}

	const FNBPublicGameState State = GS->GetPublicState();
	if (!State.bRoundActive)
	{
		PC->ClientRPCPrintChat(TEXT("다음 라운드를 기다려 주세요."));
		return;
	}
	if (ExpectedTurnId != TurnId || ExpectedTurnId != State.TurnId) // 클라이언트가 본 턴이 현재 서버 턴과 다르면 거절함
	{
		PC->ClientRPCPrintChat(TEXT("이미 지난 턴의 입력입니다. 현재 차례를 확인하세요."));
		return;
	}
	if (State.ActivePlayer != PS) // 현재 차례인 플레이어만 추측할 수 있음
	{
		PC->ClientRPCPrintChat(TEXT("지금은 다른 플레이어의 차례입니다."));
		return;
	}
	if (!PS->HasAttemptsRemaining() || bTurnResolved || bHasSubmittedThisTurn) // 잔여 시도와 중복 제출 여부를 서버에서 검사함
	{
		PC->ClientRPCPrintChat(TEXT("이번 턴에는 더 이상 추측할 수 없습니다."));
		return;
	}
	if (GS->GetServerWorldTimeSeconds() >= State.TurnEndServerTime) // 마감 이후 입력은 시간 초과로 처리함
	{
		PC->ClientRPCPrintChat(TEXT("제한 시간이 끝났습니다."));
		HandleTurnTimeout(ExpectedTurnId);
		return;
	}
	if (!UNBNumberBaseballLibrary::IsValidGuess(Message))
	{
		PC->ClientRPCPrintChat(TEXT("1~9의 중복 없는 숫자 3자리를 공백 없이 다시 입력하세요."));
		return;
	}

	// 입력과 타임아웃 중 먼저 처리된 쪽만 이 턴을 닫는다.
	bTurnResolved = true;
	bHasSubmittedThisTurn = true;
	GetWorldTimerManager().ClearTimer(TurnTimerHandle);
	if (!PS->TryConsumeAttempt())
	{
		StartNextTurn();
		return;
	}
	const FNBGuessResult Result = UNBNumberBaseballLibrary::CheckAnswer(Answer, Message); // 정답과 입력을 비교해 스트라이크와 볼을 계산함
	GS->MulticastRPCBroadcastMessage(FString::Printf(TEXT("%s: %s → %s [%d/%d]"),
		*GetPlayerDisplayName(PS), *Message, *UNBNumberBaseballLibrary::FormatResult(Result),
		PS->GetCurrentAttemptCount(), PS->GetMaxAttemptCount()));
	if (Result.Strikes == 3) // 세 자리 모두 맞으면 승리로 라운드 종료
	{
		EndRound(FString::Printf(TEXT("%s 승리!"), *GetPlayerDisplayName(PS))); // 결과를 공개하고 일정 시간 뒤 다음 라운드를 예약함
	}
	else
	{
		StartNextTurn();
	}
}

void ANBGameModeBase::HandleTurnTimeout(int32 ExpiredTurnId) // 현재 턴의 만료를 확인하고 시도를 소비한 뒤 넘김
{
	ANBGameState* GS = GetGameState<ANBGameState>();
	if (!HasAuthority() || !IsValid(GS) || ExpiredTurnId != TurnId || bTurnResolved || bHasSubmittedThisTurn)
	{
		return;
	}
	const FNBPublicGameState State = GS->GetPublicState();
	ANBPlayerState* PS = State.ActivePlayer;
	if (!State.bRoundActive || State.TurnId != ExpiredTurnId || !IsValid(PS))
	{
		return;
	}
	const double Remaining = State.TurnEndServerTime - GS->GetServerWorldTimeSeconds();
	if (Remaining > 0.0) // 콜백이 마감보다 일찍 실행됐으면 남은 시간만큼 다시 예약함
	{
		GetWorldTimerManager().SetTimer(TurnTimerHandle,
			FTimerDelegate::CreateUObject(this, &ANBGameModeBase::HandleTurnTimeout, ExpiredTurnId),
			FMath::Max(0.001f, static_cast<float>(Remaining)), false);
		return;
	}

	bTurnResolved = true;
	GetWorldTimerManager().ClearTimer(TurnTimerHandle);
	if (PS->TryConsumeAttempt())
	{
		GS->MulticastRPCBroadcastMessage(FString::Printf(TEXT("%s 시간 초과! [%d/%d]"), // 서버가 호출해 각 수신 환경의 로컬 UI에 메시지를 전달함
			*GetPlayerDisplayName(PS), PS->GetCurrentAttemptCount(), PS->GetMaxAttemptCount()));
	}
	StartNextTurn();
}

void ANBGameModeBase::EndRound(const FString& ResultMessage) // 결과를 공개하고 일정 시간 뒤 다음 라운드를 예약함
{
	ANBGameState* GS = GetGameState<ANBGameState>();
	if (!HasAuthority() || !IsValid(GS) || !GS->GetPublicState().bRoundActive)
	{
		return;
	}
	bTurnResolved = true;
	CurrentPlayerIndex = INDEX_NONE;
	GetWorldTimerManager().ClearTimer(TurnTimerHandle);
	GetWorldTimerManager().ClearTimer(RestartTimerHandle);
	FNBPublicGameState State;
	State.RoundId = RoundId;
	State.TurnId = TurnId;
	State.ResultMessage = ResultMessage;
	GS->SetPublicState(State);
	GS->MulticastRPCBroadcastMessage(ResultMessage);
	const float Delay = FMath::IsFinite(ResultDisplayDuration) ? FMath::Max(0.1f, ResultDisplayDuration) : 3.0f;
	GetWorldTimerManager().SetTimer(RestartTimerHandle, this, &ANBGameModeBase::StartRound, Delay, false); // 결과 표시 시간이 지나면 한 번 새 라운드를 시작함
}

void ANBGameModeBase::WaitForPlayers() // 타이머와 정답을 지우고 참가자를 기다리는 상태로 바꿈
{
	bTurnResolved = true;
	bHasSubmittedThisTurn = false;
	CurrentPlayerIndex = INDEX_NONE;
	Answer.Empty();
	GetWorldTimerManager().ClearTimer(TurnTimerHandle);
	GetWorldTimerManager().ClearTimer(RestartTimerHandle);
	if (ANBGameState* GS = GetGameState<ANBGameState>(); HasAuthority() && IsValid(GS))
	{
		FNBPublicGameState State;
		State.RoundId = RoundId;
		State.TurnId = TurnId;
		GS->SetPublicState(State);
	}
}

FString ANBGameModeBase::GetPlayerDisplayName(const ANBPlayerState* PlayerState) const // 이름이 비어 있으면 플레이어 ID로 표시 이름을 만듦
{
	const FString PlayerName = PlayerState->GetPlayerName();
	return PlayerName.IsEmpty() ? FString::Printf(TEXT("플레이어 %d"), PlayerState->GetPlayerId()) : PlayerName;
}

FString ANBGameModeBase::GetNetModeString() const // 실행 중인 네트워크 모드를 로그용 문자열로 바꿈
{
	switch (GetNetMode())
	{
	case NM_Standalone:
		return TEXT("Standalone");
	case NM_DedicatedServer:
		return TEXT("Dedicated Server");
	case NM_ListenServer:
		return TEXT("Listen Server");
	case NM_Client:
		return TEXT("Client");
	default:
		return TEXT("Unknown");
	}
}
