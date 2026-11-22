#pragma once

#include "CoreMinimal.h"

class UTexture2D;

/**
 * Content/YJJWave/Art/*.png 를 실행 중에 읽어 텍스처로 만든다 (에셋 임포트 없음).
 * 유닛은 화면에서 20~40픽셀로 작게 그려지므로 반짝이지 않게 밉맵을 직접 만든다
 * (알파를 곱한 채로 2x2 평균 → 투명 가장자리가 검게 번지지 않는다).
 */
class YJJWAVE_API FYJJTextureLoader
{
public:
	/** 실패하면 nullptr (로그에 이유). 돌려준 텍스처는 쓰는 쪽이 UPROPERTY로 붙잡는다 */
	static UTexture2D* Load(const FString& FileName, bool bMips);

	static FString ArtDir();

private:
	/** BGRA8 한 단계를 반으로 줄인다 */
	static void Downsample(const TArray64<uint8>& Source, int32 Width, int32 Height, TArray64<uint8>& Out);

	static constexpr int32 BytesPerPixel = 4;
	static constexpr int32 AlphaChannel = 3;
	static constexpr int32 ColorChannels = 3;
	static constexpr int32 SamplesPerTexel = 4;
	static constexpr float MaxByte = 255.f;
	static constexpr const TCHAR* ArtFolder = TEXT("YJJWave/Art");
};
