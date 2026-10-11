#include "NBNumberBaseballLibrary.h"

FString UNBNumberBaseballLibrary::GenerateRandomNumbers() // 1~9에서 중복 없는 세 자리 정답을 만듦
{
	TCHAR Digits[] = TEXT("123456789"); // 뽑을 수 있는 숫자 후보
	FString Answer;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		Swap(Digits[Index], Digits[FMath::RandRange(Index, 8)]); // 아직 뽑지 않은 구간에서 무작위 숫자를 앞으로 옮김
		Answer.AppendChar(Digits[Index]); // 선택한 숫자를 정답 뒤에 붙임
	}
	return Answer;
}

bool UNBNumberBaseballLibrary::IsValidGuess(const FString& Input) // 길이·숫자 범위·중복 여부를 검사함
{
	if (Input.Len() != 3) // 정확히 세 글자가 아니면 거절함
	{
		return false;
	}

	for (int32 Index = 0; Index < 3; ++Index)
	{
		if (Input[Index] < TEXT('1') || Input[Index] > TEXT('9')) // 1~9 밖의 문자를 거절함
		{
			return false;
		}
		for (int32 Previous = 0; Previous < Index; ++Previous) // 앞서 검사한 숫자들과 비교함
		{
			if (Input[Index] == Input[Previous]) // 같은 숫자가 있으면 중복 입력임
			{
				return false;
			}
		}
	}
	return true;
}

FNBGuessResult UNBNumberBaseballLibrary::CheckAnswer(const FString& Answer, const FString& Input) // 정답과 입력을 비교해 스트라이크와 볼을 계산함
{
	FNBGuessResult Result;
	if (!IsValidGuess(Answer) || !IsValidGuess(Input))
	{
		return Result;
	}

	for (int32 AnswerIndex = 0; AnswerIndex < 3; ++AnswerIndex)
	{
		for (int32 InputIndex = 0; InputIndex < 3; ++InputIndex)
		{
			if (Answer[AnswerIndex] == Input[InputIndex]) // 숫자가 같은 경우만 위치를 검사함
			{
				if (AnswerIndex == InputIndex) // 자리까지 같으면 스트라이크, 다르면 볼
				{
					++Result.Strikes;
				}
				else
				{
					++Result.Balls;
				}
			}
		}
	}
	return Result;
}

FString UNBNumberBaseballLibrary::FormatResult(const FNBGuessResult& Result) // 판정 결과를 OUT 또는 S/B 문자열로 바꿈
{
	if (Result.Strikes == 0 && Result.Balls == 0) // 맞는 숫자가 하나도 없으면 OUT
	{
		return TEXT("OUT");
	}
	return FString::Printf(TEXT("%dS%dB"), Result.Strikes, Result.Balls);
}
