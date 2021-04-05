////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "NoesisResourceProvider.h"

// Core includes
#include "Misc/FileHelper.h"

// Engine includes
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"

// NoesisRuntime includes
#include "NoesisXaml.h"
#include "NoesisSupport.h"

void FNoesisXamlProvider::OnXamlChanged(UNoesisXaml* Xaml)
{
#if WITH_EDITOR
	if (FString* Name = NameMap.Find(Xaml))
	{
		RaiseXamlChanged(TCHAR_TO_UTF8(**Name));
	}
#endif
}

Noesis::Ptr<Noesis::Stream> FNoesisXamlProvider::LoadXaml(const char* Path)
{
    //FString XamlPath = NsProviderPathToAssetPath(Path);
    FString XamlPath = FString(Path);
    FPaths::RemoveDuplicateSlashes(XamlPath);
    UNoesisXaml* Xaml = LoadObject<UNoesisXaml>(nullptr, *XamlPath, nullptr, LOAD_NoWarn);
    UE_LOG(LogNoesis, Verbose, TEXT("FNoesisXamlProvider::LoadXaml(): %ld %s"), Xaml, *XamlPath);
	if (Xaml)
	{
        // Need to register the XAML's dependencies (fonts, styles, etc.) before returning.
		Xaml->RegisterDependencies();

#if WITH_EDITOR
		NameMap.Add(Xaml, Path);
#endif

		return Noesis::Ptr<Noesis::Stream>(*new Noesis::MemoryStream(Xaml->XamlText.GetData(), (uint32)Xaml->XamlText.Num()));
	}

	return Noesis::Ptr<Noesis::Stream>();
};

void FNoesisTextureProvider::OnTextureChanged(UTexture2D* Texture)
{
#if WITH_EDITOR
	if (FString* Name = NameMap.Find(Texture))
	{
		RaiseTextureChanged(TCHAR_TO_UTF8(**Name));
	}
#endif
}

Noesis::TextureInfo FNoesisTextureProvider::GetTextureInfo(const char* Path)
{
    FString TexturePath = FString(Path);
    UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *TexturePath, nullptr, LOAD_NoWarn);
    UE_LOG(LogNoesis, Verbose, TEXT("FNoesisTextureProvider::GetTextureInfo(): %ld %s"), Texture, *TexturePath);
	if (Texture)
	{
#if WITH_EDITOR
		NameMap.Add(Texture, Path);
#endif
		return Noesis::TextureInfo { (uint32)Texture->GetSizeX(), (uint32)Texture->GetSizeY() };
	}

	return Noesis::TextureInfo {};
}

Noesis::Ptr<Noesis::Texture> FNoesisTextureProvider::LoadTexture(const char* Path, Noesis::RenderDevice* RenderDevice)
{
    FString TextureObjectPath = FString(Path);
    //FString TextureObjectPath = TexturePath + TEXT(".") + FPackageName::GetShortName(TexturePath);
    UTexture2D* Texture = FindObject<UTexture2D>(nullptr, *TextureObjectPath);
    UE_LOG(LogNoesis, Verbose, TEXT("FNoesisTextureProvider::LoadTexture(): %ld %s"), Texture, *TextureObjectPath);
	if (Texture)
	{
		return NoesisCreateTexture(Texture);
	}

	return nullptr;
};

void FNoesisFontProvider::RegisterFont(const UFontFace* FontFace)
{
	if (FontFace != nullptr)
	{
        const FString& fontFacePath = FontFace->GetPathName();
        FString fontFullPackagePath = FPackageName::GetLongPackagePath(fontFacePath);

        // (snote) Remove the leading slash due to [Noesis::GUI::SetFontFallbacks()] also doing so.
        fontFullPackagePath.RemoveAt(0);
        Noesis::CachedFontProvider::RegisterFont(TCHARToNsString(*fontFullPackagePath).Str(), TCHARToNsString(*fontFacePath).Str());
        UE_LOG(LogNoesis, Verbose, TEXT("FNoesisFontProvider::RegisterFont() [%s]: [%s]"), *fontFullPackagePath, *fontFacePath);
#if NOESIS_DEBUG
        FString packageRoot, packagePath, packageName;
        FPackageName::SplitLongPackageName(FontFace->GetPackage()->GetPathName(), packageRoot, packagePath, packageName, false);
        UE_LOG(LogNoesis, Verbose, TEXT("Package: %s %s %s %s"), *fontFullPackagePath, *packageRoot, *packagePath, *packageName);
#endif
	}
}

Noesis::FontSource FNoesisFontProvider::MatchFont(const char* BaseUri, const char* FamilyName, Noesis::FontWeight& Weight,
	Noesis::FontStretch& Stretch, Noesis::FontStyle& Style)
{
    return Noesis::CachedFontProvider::MatchFont(TCHAR_TO_UTF8(*FString(BaseUri)), FamilyName, Weight, Stretch, Style);
}

bool FNoesisFontProvider::FamilyExists(const char* BaseUri, const char* FamilyName)
{
    FString fontPackagePath(BaseUri);
    // (snote) Remove the leading slash due to [Noesis::GUI::SetFontFallbacks()] also doing so.
    if (fontPackagePath.StartsWith(TEXT("/")))
    {
        fontPackagePath.RemoveAt(0);
    }
    bool bFontExists = Noesis::CachedFontProvider::FamilyExists(TCHAR_TO_UTF8(*fontPackagePath), FamilyName);
    UE_LOG(LogNoesis, Verbose, TEXT("%d FNoesisFontProvider::FamilyExists() in [%s]: [%s]"), bFontExists, *fontPackagePath, *FString(FamilyName));
    return bFontExists;
}

void FNoesisFontProvider::ScanFolder(const char* InFolder)
{
}

Noesis::Ptr<Noesis::Stream> FNoesisFontProvider::OpenFont(const char* InFolder, const char* InFilename) const
{
    const UFontFace* FontFace = LoadObject<UFontFace>(nullptr, *FString(InFilename), nullptr, LOAD_NoWarn);

    UE_LOG(LogNoesis, Verbose, TEXT("FNoesisFontProvider::OpenFont() %ld %s %s"), FontFace, *FString(InFilename), *FontFace->GetFontFilename());
	if (FontFace != nullptr)
	{
		class FontArrayMemoryStream : public Noesis::MemoryStream
		{
		public:
			FontArrayMemoryStream(TArray<uint8>&& InFileData)
				: Noesis::MemoryStream(InFileData.GetData(), (uint32)InFileData.Num()),
				FileData(MoveTemp(InFileData))
			{
			}

		private:
			TArray<uint8> FileData;
		};
#if !WITH_EDITORONLY_DATA
		if (FontFace->GetLoadingPolicy() != EFontLoadingPolicy::Inline)
		{
			TArray<uint8> FileData;
			FFileHelper::LoadFileToArray(FileData, *FontFace->GetFontFilename());
			return Noesis::Ptr<Noesis::Stream>(*new FontArrayMemoryStream(MoveTemp(FileData)));
		}
		else
#endif
		{
			const FFontFaceDataRef FontFaceDataRef = FontFace->FontFaceData;
			const FFontFaceData& FontFaceData = FontFaceDataRef.Get();
			const TArray<uint8>& FontFaceDataArray = FontFaceData.GetData();
			return Noesis::Ptr<Noesis::Stream>(*new FontArrayMemoryStream(CopyTemp(FontFaceDataArray)));
		}
	}
	return Noesis::Ptr<Noesis::Stream>();
}
