#include "Render/YJJTextureLoader.h"
#include "Engine/Texture2D.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "TextureResource.h"
#include "YJJWave.h"

FString FYJJTextureLoader::ArtDir()
{
	return FPaths::ProjectContentDir() / ArtFolder;
}

void FYJJTextureLoader::Downsample(const TArray64<uint8>& Source, int32 Width, int32 Height, TArray64<uint8>& Out)
{
	const int32 OutW = FMath::Max(Width / 2, 1);
	const int32 OutH = FMath::Max(Height / 2, 1);
	Out.SetNumUninitialized(static_cast<int64>(OutW) * OutH * BytesPerPixel);
	for (int32 Y = 0; Y < OutH; ++Y)
	{
		for (int32 X = 0; X < OutW; ++X)
		{
			float Sum[BytesPerPixel] = { 0.f, 0.f, 0.f, 0.f };
			for (int32 Sample = 0; Sample < SamplesPerTexel; ++Sample)
			{
				const int32 SX = FMath::Min(X * 2 + (Sample & 1), Width - 1);
				const int32 SY = FMath::Min(Y * 2 + (Sample >> 1), Height - 1);
				const uint8* Texel = &Source[(static_cast<int64>(SY) * Width + SX) * BytesPerPixel];
				const float Alpha = Texel[AlphaChannel] / MaxByte;
				for (int32 Channel = 0; Channel < ColorChannels; ++Channel)
				{
					Sum[Channel] += Texel[Channel] * Alpha;
				}
				Sum[AlphaChannel] += Alpha;
			}
			uint8* Dest = &Out[(static_cast<int64>(Y) * OutW + X) * BytesPerPixel];
			for (int32 Channel = 0; Channel < ColorChannels; ++Channel)
			{
				Dest[Channel] = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(Sum[AlphaChannel] > 0.f ? Sum[Channel] / Sum[AlphaChannel] : 0.f), 0, MAX_uint8));
			}
			Dest[AlphaChannel] = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(Sum[AlphaChannel] / SamplesPerTexel * MaxByte), 0, MAX_uint8));
		}
	}
}

UTexture2D* FYJJTextureLoader::Load(const FString& FileName, bool bMips)
{
	const FString Path = ArtDir() / FileName;
	FImage Image;
	if (!FImageUtils::LoadImage(*Path, Image))
	{
		UE_LOG(LogYJJWave, Error, TEXT("[Art] %s 를 읽지 못했다 (Tools/Art/BuildArt.ps1 로 만든다)"), *Path);
		return nullptr;
	}
	Image.ChangeFormat(ERawImageFormat::BGRA8, EGammaSpace::sRGB);

	UTexture2D* Texture = UTexture2D::CreateTransient(Image.SizeX, Image.SizeY, PF_B8G8R8A8, FName(*FPaths::GetBaseFilename(FileName)), Image.RawData);
	if (!Texture)
	{
		return nullptr;
	}

	if (bMips)
	{
		FTexturePlatformData* Platform = Texture->GetPlatformData();
		TArray64<uint8> Level = MoveTemp(Image.RawData);
		int32 Width = Image.SizeX;
		int32 Height = Image.SizeY;
		while (Width > 1 || Height > 1)
		{
			TArray64<uint8> Smaller;
			Downsample(Level, Width, Height, Smaller);
			Width = FMath::Max(Width / 2, 1);
			Height = FMath::Max(Height / 2, 1);

			FTexture2DMipMap* Mip = new FTexture2DMipMap(Width, Height, 1);
			Platform->Mips.Add(Mip);
			Mip->BulkData.Lock(LOCK_READ_WRITE);
			void* Dest = Mip->BulkData.Realloc(Smaller.Num());
			FMemory::Memcpy(Dest, Smaller.GetData(), Smaller.Num());
			Mip->BulkData.Unlock();
			Level = MoveTemp(Smaller);
		}
	}

	Texture->SRGB = true;
	Texture->Filter = bMips ? TF_Trilinear : TF_Bilinear;
	Texture->LODGroup = TEXTUREGROUP_UI;
	Texture->NeverStream = true;
	Texture->UpdateResource();
	return Texture;
}
