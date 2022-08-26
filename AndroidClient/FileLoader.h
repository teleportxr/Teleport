#pragma once
#include "Platform/Core/FileLoader.h"
#include <android/asset_manager.h>

namespace teleport
{
	namespace android
	{
		//! The file loader for Android - uses either fstream or AAssetManager depending on the path.
		//! Paths beginning with "assets/" are obtained from the asset manager, all others from the file system.
		class FileLoader:public platform::core::FileLoader
		{
			bool IsAsset(const char *filename_utf8) const;
			std::string AssetFilename(const char *filename_utf8) const;
		public:
			FileLoader();
			bool FileExists(const char *filename_utf8) const override;
			void AcquireFileContents(void*& pointer, unsigned int& bytes, const char* filename_utf8,bool open_as_text) override;
			double GetFileDate(const char* filename_utf8) const override;
			void ReleaseFileContents(void* pointer) override;
			bool Save(const void* pointer, unsigned int bytes, const char* filename_utf8,bool save_as_text) override;

			static void SetAndroid_AAssetManager(AAssetManager* assetManager) { s_AssetManager = assetManager; };
			static AAssetManager* s_AssetManager;
		};
	}
}