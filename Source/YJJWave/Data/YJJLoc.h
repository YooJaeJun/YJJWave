#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"

/**
 * 화면에 나오는 모든 글자는 Content/YJJWave/Data/Localization.csv 에 있다.
 *   열: Key, ko, en, Note  (언어를 늘리려면 열을 하나 더 붙이면 된다. 예: ja)
 * 언어: 명령줄 -yjjlang=en > 콘솔 yjj.Lang en > 시스템 언어 > ko.
 * 그 언어 칸이 비어 있으면 ko, 그것도 없으면 [Key] 를 보여 준다 (빠진 글자가 눈에 띄게).
 * 서식 자리는 {0} {1} ... (FString::Format). 글자 안의 \n 은 줄바꿈.
 * 서식 자리 뒤 조사는 (이)가 (을)를 (은)는 (과)와 (으)로 로 적으면 앞 글자 받침을 보고 고른다. 예: {0}(이)가
 */

/** 받침에 따라 갈리는 조사 한 쌍 */
struct FYJJJosa
{
	const TCHAR* Marker;
	const TCHAR* WithFinal;      // 받침 있을 때 (졸병이)
	const TCHAR* WithoutFinal;   // 받침 없을 때 (궁수가)
	bool bRieulAsVowel;          // (으)로: ㄹ 받침은 받침 없는 쪽 (망치로, 해골로)
};

struct FYJJJosaRules
{
	static constexpr FYJJJosa Pairs[] = {
		{ TEXT("(이)가"), TEXT("이"), TEXT("가"), false },
		{ TEXT("(을)를"), TEXT("을"), TEXT("를"), false },
		{ TEXT("(은)는"), TEXT("은"), TEXT("는"), false },
		{ TEXT("(과)와"), TEXT("과"), TEXT("와"), false },
		{ TEXT("(으)로"), TEXT("으로"), TEXT("로"), true },
	};
	static constexpr TCHAR HangulFirst = 0xAC00;    // 가
	static constexpr TCHAR HangulLast = 0xD7A3;     // 힣
	static constexpr int32 FinalCount = 28;         // 받침 경우의 수 (없음 포함)
	static constexpr int32 FinalRieul = 8;          // ㄹ 받침
	/** 숫자를 읽었을 때 받침이 있는 것: 영 일 삼 육 칠 팔 */
	static constexpr const TCHAR* DigitsWithFinal = TEXT("013678");
	static constexpr const TCHAR* DigitsWithRieul = TEXT("178");
};
namespace YJJLoc
{
	YJJWAVE_API const FString& Get(const TCHAR* Key);
	YJJWAVE_API const FString& Get(const FString& Key);
	YJJWAVE_API FString Format(const TCHAR* Key, const FStringFormatOrderedArguments& Args);
	YJJWAVE_API bool Has(const FString& Key);
	/** (이)가 같은 조사 표시를 앞 글자 받침에 맞춰 바꾼다 */
	YJJWAVE_API FString ResolveJosa(const FString& Text);

	/** 지금 언어 (ko, en ...) */
	YJJWAVE_API const FString& Language();
	YJJWAVE_API void SetLanguage(const FString& Language);

	/** CSV를 다시 읽는다 (yjj.ReloadData 도 부른다) */
	YJJWAVE_API void Reload();
}

/** YJJLoc 뒤에서 글자를 들고 있는 것 */
class YJJWAVE_API FYJJTextStore
{
public:
	static FYJJTextStore& Get();

	const FString& Find(const FString& Key);
	bool Has(const FString& Key) const { return Texts.Contains(Key); }
	const FString& Language() const { return CurrentLanguage; }
	void SetLanguage(const FString& Language);
	void Load();

private:
	FYJJTextStore() = default;
	void PickLanguage();

	/** 키 -> (언어 -> 글자) */
	TMap<FString, TMap<FString, FString>> Texts;
	TArray<FString> Languages;
	FString CurrentLanguage;
	/** 없는 키는 [Key]로 한 번 만들어 둔다 (참조를 돌려주려고. 표가 커져도 주소가 바뀌지 않게 따로 할당) */
	TMap<FString, TSharedRef<FString>> Missing;
	bool bLoaded = false;

	static const TCHAR* const FileName;
	static const TCHAR* const DefaultLanguage;
	static const TCHAR* const NoteColumn;
	static FAutoConsoleCommand LanguageCommand;
};
