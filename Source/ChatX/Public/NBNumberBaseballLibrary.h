#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NBNumberBaseballLibrary.generated.h"

USTRUCT(BlueprintType)
struct CHATX_API FNBGuessResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 Strikes = 0; // 숫자와 위치가 모두 맞는 개수

	UPROPERTY(BlueprintReadOnly)
	int32 Balls = 0; // 숫자는 맞지만 위치가 다른 개수
};

UCLASS()
class CHATX_API UNBNumberBaseballLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "NumberBaseball")
	static FString GenerateRandomNumbers(); // 1~9에서 중복 없는 세 자리 정답을 만듦

	UFUNCTION(BlueprintPure, Category = "NumberBaseball")
	static bool IsValidGuess(const FString& Input); // 길이·숫자 범위·중복 여부를 검사함

	UFUNCTION(BlueprintPure, Category = "NumberBaseball")
	static FNBGuessResult CheckAnswer(const FString& Answer, const FString& Input); // 정답과 입력을 비교해 스트라이크와 볼을 계산함

	UFUNCTION(BlueprintPure, Category = "NumberBaseball")
	static FString FormatResult(const FNBGuessResult& Result); // 판정 결과를 OUT 또는 S/B 문자열로 바꿈
};
