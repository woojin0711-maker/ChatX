#include "NBChatInput.h"
#include "NBGameState.h"
#include "NBPlayerController.h"
#include "NBPlayerState.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/EditableTextBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"

namespace
{
	FString GetPlayerLabel(const ANBPlayerState* Player) // UI에 사용할 플레이어 이름 또는 대체 이름을 만듦
	{
		if (!IsValid(Player))
		{
			return TEXT("플레이어 대기 중");
		}
		const FString Name = Player->GetPlayerName();
		return Name.IsEmpty() ? FString::Printf(TEXT("플레이어 %d"), Player->GetPlayerId()) : Name;
	}

	void SetLabel(UTextBlock* Label, const FString& Text) // 텍스트가 달라졌을 때만 UE 텍스트 위젯을 갱신함
	{
		if (Label && Label->GetText().ToString() != Text)
		{
			Label->SetText(FText::FromString(Text));
		}
	}
}

TSharedRef<SWidget> UNBChatInput::RebuildWidget() // 코드로 생성한 루트를 재사용하여 채팅 화면을 구성함
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
		CodeRoot = nullptr;
	}

	if (!IsValid(CodeRoot)) // 이 클래스가 생성한 UI만 재사용함
	{
		UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
		Background->SetPadding(FMargin(20.0f));
		Background->SetBrushColor(FLinearColor(0.025f, 0.04f, 0.065f, 0.96f));
		CodeRoot = Background;

		UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>();
		Background->SetContent(Layout);

		auto AddLabel = [this, Layout](FName Name, const TCHAR* Text) // 같은 방식의 텍스트 생성·배치를 재사용하는 람다
			{
				UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
				Label->SetText(FText::FromString(Text));
				Label->SetAutoWrapText(true);
				Layout->AddChildToVerticalBox(Label)->SetPadding(FMargin(0.0f, 3.0f));
				return Label;
			};

		UTextBlock* Title = AddLabel(TEXT("TextBlock_Title"), TEXT("숫자 야구"));
		FSlateFontInfo TitleFont = Title->GetFont();
		TitleFont.Size = 26;
		Title->SetFont(TitleFont);

		UBorder* ResultBanner = WidgetTree->ConstructWidget<UBorder>();
		ResultBanner->SetBrushColor(FLinearColor(0.12f, 0.2f, 0.32f, 1.0f));
		ResultBanner->SetPadding(FMargin(12.0f));
		TextBlock_Result = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TextBlock_Result"));
		TextBlock_Result->SetAutoWrapText(true);
		TextBlock_Result->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.82f, 0.25f)));
		FSlateFontInfo ResultFont = TextBlock_Result->GetFont();
		ResultFont.Size = 22;
		TextBlock_Result->SetFont(ResultFont);
		ResultBanner->SetContent(TextBlock_Result);
		Layout->AddChildToVerticalBox(ResultBanner)->SetPadding(FMargin(0.0f, 8.0f));

		TextBlock_Turn = AddLabel(TEXT("TextBlock_Turn"), TEXT("게임 상태 대기 중"));
		TextBlock_Attempts = AddLabel(TEXT("TextBlock_Attempts"), TEXT("내 사용 횟수: [-- / --]"));
		TextBlock_InputStatus = AddLabel(TEXT("TextBlock_InputStatus"), TEXT("추측 불가: 서버 대기 중입니다."));

		USizeBox* PlayersHeight = WidgetTree->ConstructWidget<USizeBox>();
		PlayersHeight->SetMaxDesiredHeight(110.0f);
		UScrollBox* PlayersScroll = WidgetTree->ConstructWidget<UScrollBox>();
		TextBlock_Players = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TextBlock_Players"));
		TextBlock_Players->SetAutoWrapText(true);
		PlayersScroll->AddChild(TextBlock_Players);
		PlayersHeight->SetContent(PlayersScroll);
		Layout->AddChildToVerticalBox(PlayersHeight)->SetPadding(FMargin(0.0f, 8.0f));

		ScrollBox_ChatLog = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("ScrollBox_ChatLog"));
		TextBlock_ChatLog = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TextBlock_ChatLog"));
		TextBlock_ChatLog->SetAutoWrapText(true);
		ScrollBox_ChatLog->AddChild(TextBlock_ChatLog);
		UVerticalBoxSlot* ChatSlot = Layout->AddChildToVerticalBox(ScrollBox_ChatLog);
		ChatSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ChatSlot->SetPadding(FMargin(0.0f, 8.0f));

		EditableTextBox_ChatInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("EditableTextBox_ChatInput"));
		EditableTextBox_ChatInput->SetHintText(FText::FromString(TEXT("123 또는 /say 안녕하세요 입력 후 Enter")));
		Layout->AddChildToVerticalBox(EditableTextBox_ChatInput);
		AddLabel(TEXT("TextBlock_Help"), TEXT("추측: 1~9 중 서로 다른 숫자 3자리를 공백 없이 입력하세요. S = 스트라이크(숫자와 자리 일치), B = 볼(숫자만 일치), OUT = 아웃(일치 없음).\n/say 내용: 차례나 남은 횟수와 관계없이 채팅할 수 있습니다. 최대 256자입니다."));
	}

	WidgetTree->RootWidget = CodeRoot; // 디자이너 루트 대신 코드에서 구성한 루트를 사용함
	return Super::RebuildWidget();
}

void UNBChatInput::NativeConstruct() // 입력 이벤트를 연결하고 초기 화면을 갱신함
{
	Super::NativeConstruct();
	if (EditableTextBox_ChatInput)
	{
		EditableTextBox_ChatInput->SetClearKeyboardFocusOnCommit(false);
		if (!EditableTextBox_ChatInput->OnTextCommitted.IsAlreadyBound(this, &UNBChatInput::OnChatInputCommitted))
		{
			EditableTextBox_ChatInput->OnTextCommitted.AddDynamic(this, &UNBChatInput::OnChatInputCommitted); // 텍스트 확정 이벤트를 제출 함수에 연결함
		}
		if (APlayerController* PC = GetOwningPlayer(); IsValid(PC) && PC->IsLocalController())
		{
			EditableTextBox_ChatInput->SetUserFocus(PC);
		}
	}
	RefreshChatLog();
	RefreshGameState();
}

void UNBChatInput::NativeDestruct() // 위젯 제거 시 입력 이벤트 연결을 해제함
{
	if (EditableTextBox_ChatInput)
	{
		EditableTextBox_ChatInput->OnTextCommitted.RemoveDynamic(this, &UNBChatInput::OnChatInputCommitted); // 제거되는 위젯의 이벤트 연결을 해제함
	}
	Super::NativeDestruct();
}

void UNBChatInput::NativeTick(const FGeometry& MyGeometry, float InDeltaTime) // 매 프레임 복제 상태와 남은 시간을 화면에 반영함
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshGameState();
}

void UNBChatInput::AppendChatMessage(const FString& Message) // 받은 메시지를 줄 단위로 기록하고 화면을 갱신함
{
	TArray<FString> NewLines;
	Message.ParseIntoArrayLines(NewLines, false);
	ChatLines.Append(NewLines);
	if (ChatLines.Num() > 100) // 최근 100줄만 남기도록 오래된 줄을 제거함
	{
		ChatLines.RemoveAt(0, ChatLines.Num() - 100);
	}
	RefreshChatLog();
}

void UNBChatInput::RefreshChatLog() // 기록을 합쳐 표시하고 마지막 메시지로 스크롤함
{
	SetLabel(TextBlock_ChatLog, FString::Join(ChatLines, TEXT("\n")));
	if (ScrollBox_ChatLog)
	{
		ScrollBox_ChatLog->ScrollToEnd();
	}
}

void UNBChatInput::RefreshGameState() // 복제된 게임·플레이어 상태로 안내 문구를 갱신함
{
	ANBPlayerController* PC = Cast<ANBPlayerController>(GetOwningPlayer());
	ANBPlayerState* LocalPlayer = IsValid(PC) && PC->IsLocalController() ? PC->GetPlayerState<ANBPlayerState>() : nullptr;
	ANBGameState* GS = GetWorld() ? GetWorld()->GetGameState<ANBGameState>() : nullptr;

	SetLabel(TextBlock_Attempts, IsValid(LocalPlayer)
		? FString::Printf(TEXT("내 사용 횟수: [%d / %d]"), LocalPlayer->GetCurrentAttemptCount(), LocalPlayer->GetMaxAttemptCount())
		: TEXT("내 사용 횟수: [-- / --] (플레이어 상태 대기 중)"));

	if (!IsValid(GS))
	{
		SetLabel(TextBlock_Result, TEXT("서버 대기 중"));
		SetLabel(TextBlock_Turn, TEXT("현재 차례: -- | 남은 시간: --"));
		SetLabel(TextBlock_Players, TEXT("플레이어 대기 중"));
		SetLabel(TextBlock_InputStatus, TEXT("추측 불가: 게임 상태 대기 중입니다."));
		return;
	}

	const FNBPublicGameState& State = GS->GetPublicState();
	const double RemainingSeconds = GS->GetRemainingTurnSeconds();
	const bool bHasActivePlayer = IsValid(State.ActivePlayer.Get());
	SetLabel(TextBlock_Result, !State.ResultMessage.IsEmpty() ? State.ResultMessage
		: (State.bRoundActive ? TEXT("라운드 진행 중") : TEXT("라운드 대기 중")));
	SetLabel(TextBlock_Turn, State.bRoundActive && bHasActivePlayer
		? FString::Printf(TEXT("라운드 %d | 현재 차례: %s | 남은 시간: %.0f초"), State.RoundId, *GetPlayerLabel(State.ActivePlayer.Get()), FMath::CeilToDouble(RemainingSeconds))
		: TEXT("현재 차례: -- | 남은 시간: --"));

	FString Players = TEXT("플레이어별 사용 횟수");
	for (APlayerState* Player : GS->PlayerArray) // GameState의 참가자 목록에서 복제된 횟수를 읽음
	{
		const ANBPlayerState* NBPlayer = Cast<ANBPlayerState>(Player);
		if (IsValid(NBPlayer))
		{
			Players += FString::Printf(TEXT("\n%s%s [%d / %d]%s"),
				State.bRoundActive && NBPlayer == State.ActivePlayer.Get() ? TEXT("> ") : TEXT(""),
				*GetPlayerLabel(NBPlayer), NBPlayer->GetCurrentAttemptCount(), NBPlayer->GetMaxAttemptCount(),
				NBPlayer == LocalPlayer ? TEXT(" (나)") : TEXT(""));
		}
	}
	SetLabel(TextBlock_Players, Players);

	FString InputStatus;
	if (!IsValid(LocalPlayer))
	{
		InputStatus = TEXT("추측 불가: 내 플레이어 상태 대기 중입니다.");
	}
	else if (!State.ResultMessage.IsEmpty())
	{
		InputStatus = TEXT("추측 불가: 결과 표시 중입니다. 다음 라운드를 기다려 주세요.");
	}
	else if (!State.bRoundActive || !bHasActivePlayer)
	{
		InputStatus = TEXT("추측 불가: 차례 대기 중입니다.");
	}
	else if (!LocalPlayer->HasAttemptsRemaining())
	{
		InputStatus = TEXT("추측 불가: 기회를 모두 사용했습니다.");
	}
	else if (State.ActivePlayer.Get() != LocalPlayer) // 다른 사람 차례이면 추측 불가 안내
	{
		InputStatus = TEXT("추측 불가: 다른 플레이어의 차례입니다.");
	}
	else if (RemainingSeconds <= 0.0) // UI 시간이 끝났어도 최종 판정은 서버가 담당함
	{
		InputStatus = TEXT("추측 불가: 제한 시간이 끝났습니다. 서버 확인 대기 중입니다.");
	}
	else
	{
		InputStatus = TEXT("내 차례입니다. 숫자를 입력하세요. 제출한 입력은 서버에서 확인합니다.");
	}
	SetLabel(TextBlock_InputStatus, InputStatus);
}

void UNBChatInput::OnChatInputCommitted(const FText& Text, ETextCommit::Type CommitMethod) // Enter로 확정한 텍스트를 Controller에 제출함
{
	if (CommitMethod != ETextCommit::OnEnter) // Enter 입력이 아니면 처리하지 않음
	{
		return;
	}
	ANBPlayerController* PC = Cast<ANBPlayerController>(GetOwningPlayer());
	if (IsValid(PC) && PC->IsLocalController()) // 유효한 로컬 Controller에만 화면 메시지를 전달함
	{
		PC->Chat(Text.ToString()); // 확정된 FText를 FString으로 바꿔 제출함
		if (EditableTextBox_ChatInput)
		{
			EditableTextBox_ChatInput->SetText(FText::GetEmpty()); // 제출 후 입력창을 비움
		}
	}
}