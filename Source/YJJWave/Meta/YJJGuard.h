#pragma once

#include "CoreMinimal.h"
#include "Misc/SecureHash.h"

/**
 * 변조 방지 (오프라인 싱글 게임 기준):
 *   FYJJGuardedNumber  금화·영혼처럼 중요한 수를 메모리에 그대로 두지 않는다 (치트 엔진으로 값을 찾아 바꾸기 어렵게).
 *                      비트를 무작위 열쇠로 뒤섞어 두 벌 들고 다니고, 두 벌이 안 맞으면 변조로 보고 마지막 정상 값으로 되돌린다.
 *   YJJGuard::Sign     저장 파일 서명 (내용 + 비밀 소금의 SHA1). 파일을 고치면 서명이 안 맞아 불러오지 않는다.
 *   FYJJSpeedWatch     스피드핵 감지: 게임 시계와 벽시계(UTC)를 견줘 게임 시계만 빨리 가면 표시한다.
 * 랭킹·유료 재화가 생기면 서버 검증(계정 재화·영수증·상위 기록 재계산)을 따로 둔다.
 */
namespace YJJGuard
{
	/** 열쇠: 실행마다 다르게 (주소·시간으로) */
	inline uint64 NewKey()
	{
		static uint64 Counter = 0x9E3779B97F4A7C15ull;
		Counter = Counter * 6364136223846793005ull + static_cast<uint64>(FPlatformTime::Cycles64()) + reinterpret_cast<UPTRINT>(&Counter);
		return Counter | 1ull;
	}

	/** 변조가 한 번이라도 보였는가 (저장·보상을 막는 데 쓴다) */
	inline bool& Tampered()
	{
		static bool bTampered = false;
		return bTampered;
	}

	/** 저장 서명용 비밀 소금. 빌드에 박혀 있으니 완벽하진 않지만 메모장 수정은 막는다 */
	inline constexpr const TCHAR* SaveSalt = TEXT("YJJWave-Gukgeori-Defense-7742-salt");

	inline FString Sign(const FString& Content)
	{
		const FString Salted = FString(SaveSalt) + Content + FString(SaveSalt);
		const FTCHARToUTF8 Utf8(*Salted);
		FSHAHash Hash;
		FSHA1::HashBuffer(Utf8.Get(), Utf8.Length(), Hash.Hash);
		return Hash.ToString();
	}
}

/** 메모리에 숨긴 수 (double). 보통 double 처럼 쓴다: 읽기, =, +=, -=, *= */
struct FYJJGuardedNumber
{
	FYJJGuardedNumber() { Set(0.0); }
	FYJJGuardedNumber(double Value) { Set(Value); }
	FYJJGuardedNumber(const FYJJGuardedNumber& Other) { Set(Other.Get()); }
	FYJJGuardedNumber& operator=(const FYJJGuardedNumber& Other) { Set(Other.Get()); return *this; }
	FYJJGuardedNumber& operator=(double Value) { Set(Value); return *this; }
	FYJJGuardedNumber& operator+=(double Value) { Set(Get() + Value); return *this; }
	FYJJGuardedNumber& operator-=(double Value) { Set(Get() - Value); return *this; }
	FYJJGuardedNumber& operator*=(double Value) { Set(Get() * Value); return *this; }
	operator double() const { return Get(); }

	double Get() const
	{
		const uint64 A = Masked ^ Key;
		const uint64 B = Rotr(Check) ^ ~Key;
		if (A != B)
		{
			// 둘 중 하나만 바뀌었다 = 메모리를 직접 고쳤다
			YJJGuard::Tampered() = true;
			return Last;
		}
		double Value;
		FMemory::Memcpy(&Value, &A, sizeof(Value));
		return Value;
	}

private:
	void Set(double Value)
	{
		uint64 Bits;
		FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
		Key = YJJGuard::NewKey();
		Masked = Bits ^ Key;
		Check = Rotl(Bits ^ ~Key);
		Last = Value;
	}

	uint64 Key = 0;
	uint64 Masked = 0;
	uint64 Check = 0;
	/** 변조가 보이면 돌려줄 마지막 정상 값 (이것만 고쳐도 다음 Set 에서 덮인다) */
	double Last = 0.0;
	static constexpr uint32 CheckRotate = 23;
	static uint64 Rotl(uint64 V) { return (V << CheckRotate) | (V >> (64 - CheckRotate)); }
	static uint64 Rotr(uint64 V) { return (V >> CheckRotate) | (V << (64 - CheckRotate)); }
};

/** 스피드핵 감지: 게임이 받은 프레임 시간 합과 벽시계(UTC) 흐름을 구간마다 견준다 */
struct FYJJSpeedWatch
{
	/** 매 프레임 (배속을 곱하기 전 실제 프레임 시간) */
	void Tick(float RealDeltaSeconds)
	{
		const FDateTime Now = FDateTime::UtcNow();
		if (Start == FDateTime())
		{
			Start = Now;
			Sum = 0.0;
			return;
		}
		Sum += RealDeltaSeconds;
		const double Wall = (Now - Start).GetTotalSeconds();
		if (Wall < Window)
		{
			return;
		}
		// 멈춤(창 끌기·로딩)으로 벽시계가 더 가는 건 괜찮다. 게임 시계만 확 빨라지면 변조
		if (Sum > Wall * Tolerance)
		{
			YJJGuard::Tampered() = true;
		}
		Start = Now;
		Sum = 0.0;
	}

private:
	FDateTime Start;
	double Sum = 0.0;
	static constexpr double Window = 5.0;
	static constexpr double Tolerance = 1.25;
};
