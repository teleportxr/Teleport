#pragma once
#include "Platform/Core/FileLoader.h"
#include <android/asset_manager.h>

namespace teleport
{
	namespace android
	{
		//! The default derived file loader
		class FileLoader:public platform::core::FileLoader
		{
		public:
			FileLoader();
			bool FileExists(const char *filename_utf8) const override;
			void AcquireFileContents(void*& pointer, unsigned int& bytes, const char* filename_utf8,bool open_as_text) override;
			double GetFileDate(const char* filename_utf8) const override;
			void ReleaseFileContents(void* pointer) override;
			bool Save(void* pointer, unsigned int bytes, const char* filename_utf8,bool save_as_text) override;

			static void SetAndroid_AAssetManager(AAssetManager* assetManager) { s_AssetManager = assetManager; };
			static AAssetManager* s_AssetManager;
		};
	}
}