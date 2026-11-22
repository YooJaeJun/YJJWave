#include "Data/YJJLoc.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/Csv/CsvParser.h"
#include "YJJWave.h"

const TCHAR* const FYJJTextStore::FileName = TEXT("YJJWave/Data/Localization.csv");
const TCHAR* const FYJJTextStore::DefaultLanguage = TEXT("ko");
const TCHAR* const FYJJTextStore::NoteColumn = TEXT("Note");

FAutoConsoleCommand FYJJTextStore::LanguageCommand(
	TEXT("yjj.Lang"),
	TEXT("화면 언어 바꾸기. 예: yjj.Lang en"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() > 0)
		{
			YJJLoc::SetLanguage(Args[0]);
		}
		UE_LOG(LogYJJWave, Display, TEXT("[Loc] language: %s"), *YJJLoc::Language());
	}));

FYJJTextStore& FYJJTextStore::Get()
{
	static FYJJTextStore Instance;
	if (!Instance.bLoaded)
	{
		Instance.Load();
	}
	return Instance;
}

void FYJJTextStore::Load()
{
	bLoaded = true;
	Texts.Reset();
	Languages.Reset();
	Missing.Reset();

	const FString Path = FPaths::ProjectContentDir() / FileName;
	FString Csv;
	if (!FFileHelper::LoadFileToString(Csv, *Path))
	{
		UE_LOG(LogYJJWave, Error, TEXT("[Loc] %s 를 읽지 못했다"), *Path);
		PickLanguage();
		return;
	}

	const FCsvParser Parser(MoveTemp(Csv));
	const FCsvParser::FRows& Rows = Parser.GetRows();
	if (Rows.IsEmpty())
	{
		PickLanguage();
		return;
	}

	// 머리줄: Key, ko, en, ..., Note
	TArray<int32> LanguageColumns;
	for (int32 Column = 1; Column < Rows[0].Num(); ++Column)
	{
		const FString Header = FString(Rows[0][Column]).TrimStartAndEnd();
		if (!Header.IsEmpty() && !Header.Equals(NoteColumn, ESearchCase::IgnoreCase))
		{
			Languages.Add(Header.ToLower());
			LanguageColumns.Add(Column);
		}
	}

	for (int32 RowIndex = 1; RowIndex < Rows.Num(); ++RowIndex)
	{
		const TArray<const TCHAR*>& Row = Rows[RowIndex];
		const FString Key = Row.Num() > 0 ? FString(Row[0]).TrimStartAndEnd() : FString();
		if (Key.IsEmpty() || Key.StartsWith(TEXT("#")))
		{
			continue;
		}
		if (Texts.Contains(Key))
		{
			UE_LOG(LogYJJWave, Warning, TEXT("[Loc] 키 중복: %s"), *Key);
		}
		TMap<FString, FString>& Entry = Texts.FindOrAdd(Key);
		for (int32 Index = 0; Index < LanguageColumns.Num(); ++Index)
		{
			const int32 Column = LanguageColumns[Index];
			if (Column < Row.Num() && *Row[Column])
			{
				Entry.Add(Languages[Index], FString(Row[Column]).Replace(TEXT("\\n"), TEXT("\n")));
			}
		}
	}

	PickLanguage();
	UE_LOG(LogYJJWave, Display, TEXT("[Loc] %d keys, languages: %s, using %s"), Texts.Num(), *FString::Join(Languages, TEXT(", ")), *CurrentLanguage);
}

void FYJJTextStore::PickLanguage()
{
	FString Wanted;
	if (!FParse::Value(FCommandLine::Get(), TEXT("yjjlang="), Wanted))
	{
		// 엔진 컬처는 프로젝트에 UE 현지화 데이터가 없으면 en으로 떨어지므로 OS 표시 언어(ko-KR …)를 본다
		Wanted = FPlatformMisc::GetDefaultLanguage();
		int32 Separator = INDEX_NONE;
		if (Wanted.FindChar(TEXT('-'), Separator) || Wanted.FindChar(TEXT('_'), Separator))
		{
			Wanted.LeftInline(Separator);
		}
	}
	Wanted = Wanted.ToLower();
	CurrentLanguage = Languages.Contains(Wanted) ? Wanted : FString(DefaultLanguage);
}

void FYJJTextStore::SetLanguage(const FString& Language)
{
	const FString Wanted = Language.ToLower();
	if (Languages.Contains(Wanted))
	{
		CurrentLanguage = Wanted;
	}
	else
	{
		UE_LOG(LogYJJWave, Warning, TEXT("[Loc] 없는 언어: %s (있는 것: %s)"), *Language, *FString::Join(Languages, TEXT(", ")));
	}
}

const FString& FYJJTextStore::Find(const FString& Key)
{
	if (const TMap<FString, FString>* Entry = Texts.Find(Key))
	{
		if (const FString* Text = Entry->Find(CurrentLanguage))
		{
			return *Text;
		}
		if (const FString* Fallback = Entry->Find(DefaultLanguage))
		{
			return *Fallback;
		}
	}
	if (const TSharedRef<FString>* Shown = Missing.Find(Key))
	{
		return Shown->Get();
	}
	UE_LOG(LogYJJWave, Warning, TEXT("[Loc] 없는 키: %s"), *Key);
	return Missing.Add(Key, MakeShared<FString>(FString::Printf(TEXT("[%s]"), *Key))).Get();
}

// ---------------------------------------------------------------------------------------------

const FString& YJJLoc::Get(const TCHAR* Key)
{
	return FYJJTextStore::Get().Find(Key);
}

const FString& YJJLoc::Get(const FString& Key)
{
	return FYJJTextStore::Get().Find(Key);
}

FString YJJLoc::Format(const TCHAR* Key, const FStringFormatOrderedArguments& Args)
{
	return FString::Format(*Get(Key), Args);
}

bool YJJLoc::Has(const FString& Key)
{
	return FYJJTextStore::Get().Has(Key);
}

const FString& YJJLoc::Language()
{
	return FYJJTextStore::Get().Language();
}

void YJJLoc::SetLanguage(const FString& Language)
{
	FYJJTextStore::Get().SetLanguage(Language);
}

void YJJLoc::Reload()
{
	FYJJTextStore::Get().Load();
}
