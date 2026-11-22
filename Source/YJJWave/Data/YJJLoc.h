#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"

/**
 * 화면에 나오는 모든 글자는 Content/YJJWave/Data/Localization.csv 에 있다.
 *   열: Key, ko, en, Note  (언어를 늘리려면 열을 하나 더 붙이면 된다. 예: ja)
 * 언어: 명령줄 -yjjlang=en > 콘솔 yjj.Lang en > 시스템 언어 > ko.
 * 그 언어 칸이 비어 있으면 ko, 그것도 없으면 [Key] 를 보여 준다 (빠진 글자가 눈에 띄게).
 * 서식 자리는 {0} {1} ... (FString::Format). 글자 안의 \n 은 줄바꿈.
 */
namespace YJJLoc
{
	YJJWAVE_API const FString& Get(const TCHAR* Key);
	YJJWAVE_API const FString& Get(const FString& Key);
	YJJWAVE_API FString Format(const TCHAR* Key, const FStringFormatOrderedArguments& Args);
	YJJWAVE_API bool Has(const FString& Key);

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
