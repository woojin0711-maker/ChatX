#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NBChatInput.generated.h"

class UBorder;
class UEditableTextBox;
class UScrollBox;
class UTextBlock;

UCLASS()
class CHATX_API UNBChatInput : public UUserWidget
{
	GENERATED_BODY()

public:
	void AppendChatMessage(const FString& Message); // 받은 메시지를 줄 단위로 기록하고 화면을 갱신함

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override; // 코드로 생성한 루트를 재사용하여 채팅 화면을 구성함
	virtual void NativeConstruct() override; // 입력 이벤트를 연결하고 초기 화면을 갱신함
	virtual void NativeDestruct() override; // 위젯 제거 시 입력 이벤트 연결을 해제함
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override; // 매 프레임 복제 상태와 남은 시간을 화면에 반영함

	UFUNCTION()
	void OnChatInputCommitted(const FText& Text, ETextCommit::Type CommitMethod); // Enter로 확정한 텍스트를 Controller에 제출함

private:
	void RefreshChatLog(); // 기록을 합쳐 표시하고 마지막 메시지로 스크롤함
	void RefreshGameState(); // 복제된 게임·플레이어 상태로 안내 문구를 갱신함

	UPROPERTY() // 코드로 생성한 입력창을 GC로부터 보관함
	TObjectPtr<UEditableTextBox> EditableTextBox_ChatInput; // Enter 입력을 받는 텍스트 입력창

	UPROPERTY()
	TObjectPtr<UScrollBox> ScrollBox_ChatLog; // 채팅 기록을 스크롤하는 컨테이너

	UPROPERTY()
	TObjectPtr<UTextBlock> TextBlock_ChatLog; // 합쳐진 채팅 기록 표시

	UPROPERTY()
	TObjectPtr<UTextBlock> TextBlock_Result; // 라운드 결과 표시

	UPROPERTY()
	TObjectPtr<UTextBlock> TextBlock_Turn; // 현재 차례와 남은 시간 표시

	UPROPERTY()
	TObjectPtr<UTextBlock> TextBlock_Attempts; // 내 시도 횟수 표시

	UPROPERTY()
	TObjectPtr<UTextBlock> TextBlock_Players; // 참가자별 사용 횟수 표시

	UPROPERTY()
	TObjectPtr<UTextBlock> TextBlock_InputStatus; // 현재 추측 가능한지 안내하는 표시

	UPROPERTY()
	TObjectPtr<UBorder> CodeRoot; // 코드로 만든 루트를 보관하여 재구성 시 중복 생성을 방지함

	TArray<FString> ChatLines; // 로컬 화면에 보관할 최근 채팅 줄 목록
};
