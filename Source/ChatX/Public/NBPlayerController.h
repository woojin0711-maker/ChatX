#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "NBPlayerController.generated.h"

class UNBChatInput;

UCLASS()
class CHATX_API ANBPlayerController : public APlayerController
{
	GENERATED_BODY()

private:
	TArray<FString> PendingChatMessages; // UI 생성 전에 받은 메시지를 임시 보관함

protected:
	UPROPERTY()
	TObjectPtr<UNBChatInput> ChatInputWidgetInstance; // 현재 화면에 생성한 채팅 위젯 참조

	virtual void BeginPlay() override; // 플레이 시작 시 클래스에 필요한 초기 설정을 실행함
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override; // 플레이 종료 시 타이머 또는 UI 자원을 정리함

public:
	UFUNCTION(Exec)
	void Chat(const FString& Message); // 로컬 입력과 현재 턴 번호를 서버 RPC로 제출함

	UFUNCTION(Server, Reliable) // 소유 클라이언트→서버 RPC; Reliable은 재전송을 통한 전달 보장 방식이며 입력 검증은 별도
	void ServerRPCSendChat(const FString& Message, int32 ExpectedTurnId); // 소유 클라이언트의 요청을 서버 GameMode에 전달함

	UFUNCTION(Client, Reliable) // 서버→이 Controller의 소유 클라이언트 RPC
	void ClientRPCPrintChat(const FString& Message); // 서버가 해당 Controller의 소유 클라이언트에 안내를 보냄

	void DisplayChatMessage(const FString& Message); // 로컬 UI에 표시하거나 UI 생성 전이면 임시 보관함
};
