#include "NBPlayerController.h"
#include "NBChatInput.h"
#include "NBGameModeBase.h"
#include "NBGameState.h"
#include "Engine/World.h"

void ANBPlayerController::BeginPlay() // 플레이 시작 시 클래스에 필요한 초기 설정을 실행함
{
	Super::BeginPlay();

	if (!IsLocalController() || GetNetMode() == NM_DedicatedServer) // 로컬 소유 Controller에서만 UI·입력을 처리함
	{
		return;
	}

	if (IsValid(ChatInputWidgetInstance)) // 이미 생성된 UI의 중복 생성을 막음
	{
		return;
	}

	ChatInputWidgetInstance = CreateWidget<UNBChatInput>(this, UNBChatInput::StaticClass()); // 이 Controller를 소유자로 채팅 UI를 생성함
	if (!IsValid(ChatInputWidgetInstance))
	{
		return;
	}

	ChatInputWidgetInstance->AddToViewport(); // 생성한 UI를 화면에 표시함

	FInputModeUIOnly InputModeUIOnly;
	InputModeUIOnly.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputModeUIOnly); // 입력을 게임 조작 대신 UI로 보냄
	bShowMouseCursor = true;

	for (const FString& Message : PendingChatMessages)
	{
		ChatInputWidgetInstance->AppendChatMessage(Message);
	}
	PendingChatMessages.Reset();
}

void ANBPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason) // 플레이 종료 시 타이머 또는 UI 자원을 정리함
{
	if (IsValid(ChatInputWidgetInstance))
	{
		ChatInputWidgetInstance->RemoveFromParent();
		ChatInputWidgetInstance = nullptr;
	}
	PendingChatMessages.Reset();

	Super::EndPlay(EndPlayReason);
}

void ANBPlayerController::Chat(const FString& Message) // 로컬 입력과 현재 턴 번호를 서버 RPC로 제출함
{
	if (!IsLocalController() || GetNetMode() == NM_DedicatedServer) // 로컬 소유 Controller에서만 UI·입력을 처리함
	{
		return;
	}

	const UWorld* World = GetWorld();
	const ANBGameState* GS = World ? World->GetGameState<ANBGameState>() : nullptr;
	if (!IsValid(GS))
	{
		DisplayChatMessage(TEXT("게임 상태 대기 중입니다. 잠시 후 다시 입력하세요."));
		return;
	}

	const int32 ExpectedTurnId = GS->GetPublicState().TurnId;
	ServerRPCSendChat(Message, ExpectedTurnId); // 소유 Controller를 통해 서버에 메시지와 관측한 턴 번호 전달
}

void ANBPlayerController::ServerRPCSendChat_Implementation(const FString& Message, int32 ExpectedTurnId) // 소유 클라이언트의 요청을 서버 GameMode에 전달함
{
	const UWorld* World = GetWorld();
	ANBGameModeBase* GM = World ? World->GetAuthGameMode<ANBGameModeBase>() : nullptr; // 서버에만 존재하는 GameMode를 가져옴
	if (IsValid(GM))
	{
		GM->ReceiveChat(this, Message, ExpectedTurnId); // 서버 규칙으로 입력을 검증하도록 위임함
	}
}

void ANBPlayerController::ClientRPCPrintChat_Implementation(const FString& Message) // 서버가 해당 Controller의 소유 클라이언트에 안내를 보냄
{
	DisplayChatMessage(Message);
}

void ANBPlayerController::DisplayChatMessage(const FString& Message) // 로컬 UI에 표시하거나 UI 생성 전이면 임시 보관함
{
	if (!IsLocalController() || GetNetMode() == NM_DedicatedServer) // 로컬 소유 Controller에서만 UI·입력을 처리함
	{
		return;
	}

	if (IsValid(ChatInputWidgetInstance))
	{
		ChatInputWidgetInstance->AppendChatMessage(Message);
		return;
	}

	constexpr int32 MaxPendingMessages = 100;
	if (PendingChatMessages.Num() >= MaxPendingMessages) // 임시 기록이 한도에 도달하면 가장 오래된 메시지 제거
	{
		PendingChatMessages.RemoveAt(0);
	}
	PendingChatMessages.Add(Message);
}
