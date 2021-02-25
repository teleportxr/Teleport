/************************************************************************************

Filename    :   SoundAssetMapping.cpp
Content     :   Sound asset manager via json definitions
Created     :   October 22, 2013
Authors     :   Warsam Osman

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.


*************************************************************************************/
#include "SoundAssetMapping.h"

#include "OVR_JSON.h"
#include "OVR_LogUtils.h"

//#include "PathUtils.h"
#include "PackageFiles.h"
#include "OVR_FileSys.h"

namespace OVRFW {

static const char * DEV_SOUNDS_RELATIVE = "Oculus/sound_assets.json";
static const char * VRLIB_SOUNDS = "res/raw/sound_assets.json";
static const char * APP_SOUNDS = "assets/sound_assets.json";

void ovrSoundAssetMapping::LoadSoundAssets( ovrFileSys * fileSys )
{
#if defined( OVR_OS_ANDROID )
	std::vector<std::string> searchPaths;
	searchPaths.push_back( "/storage/extSdCard/" );	// FIXME: This does not work for Android-M
	searchPaths.push_back( "/sdcard/" );

	// First look for sound definition using SearchPaths for dev
	std::string foundPath;
	if ( GetFullPath( searchPaths, DEV_SOUNDS_RELATIVE, foundPath ) )
	{
		std::shared_ptr<OVR::JSON> dataFile = OVR::JSON::Load( foundPath.c_str() );
		if ( dataFile == NULL )
		{
			OVR_FAIL( "ovrSoundAssetMapping::LoadSoundAssets failed to load JSON meta file: %s", foundPath.c_str() );
		}
		foundPath = foundPath.substr( 0, foundPath.length() - std::string("sound_assets.json").length() );
		LoadSoundAssetsFromJsonObject( foundPath, dataFile );
	}
	else // if that fails, we are in release - load sounds from vrappframework/res/raw and the assets folder
	{
		if ( ovr_PackageFileExists( VRLIB_SOUNDS ) )
		{
			LoadSoundAssetsFromPackage( "res/raw/", VRLIB_SOUNDS );
		}
		if ( ovr_PackageFileExists( APP_SOUNDS ) )
		{
			LoadSoundAssetsFromPackage( "", APP_SOUNDS );
		}
	}
#else
	const char  * soundAssets[]   = { VRLIB_SOUNDS, APP_SOUNDS };
	const size_t soundAssetCount = sizeof( soundAssets ) / sizeof( soundAssets[0] );
	for ( size_t soundAssetIndex = 0; soundAssetIndex < soundAssetCount; soundAssetIndex++ )
	{
		const std::string filename = std::string( "apk:///" ) + soundAssets[soundAssetIndex];
		std::vector< uint8_t > buffer;
		if ( fileSys != nullptr && fileSys->ReadFile( filename.c_str(), buffer ) )
		{
			std::string foundPath = filename;
			foundPath = foundPath.substr( 0, foundPath.length() - std::string( "sound_assets.json" ).length() );
			const char * perror = nullptr;
			std::shared_ptr<JSON> dataFile = JSON::Parse( reinterpret_cast< char const * >( static_cast< uint8_t const * >( buffer.data() ) ), &perror );
			LoadSoundAssetsFromJsonObject( foundPath, dataFile );
		}
	}
#endif

	if ( SoundMap.empty() )
	{
		OVR_LOG( "SoundManger - failed to load any sound definition files!" );
	}
}

bool ovrSoundAssetMapping::HasSound( const char * soundName ) const
{
	auto soundMapping = SoundMap.find( soundName );
	return ( soundMapping != SoundMap.end() );
}

bool ovrSoundAssetMapping::GetSound( const char * soundName, std::string & outSound ) const
{
	auto soundMapping = SoundMap.find( soundName );
	if ( soundMapping != SoundMap.end() )
	{
		outSound = soundMapping->second;
		return true;
	}
	else
	{
		OVR_WARN( "ovrSoundAssetMapping::GetSound failed to find %s", soundName );
	}

	return false;
}

void ovrSoundAssetMapping::LoadSoundAssetsFromPackage( const std::string & url, const char * jsonFile )
{
	int bufferLength = 0;
	void * 	buffer = NULL;
	ovr_ReadFileFromApplicationPackage( jsonFile, bufferLength, buffer );
	if ( !buffer )
	{
		OVR_FAIL( "ovrSoundAssetMapping::LoadSoundAssetsFromPackage failed to read %s", jsonFile );
	}

	auto dataFile = OVR::JSON::Parse( reinterpret_cast< char * >( buffer ) );
	if ( !dataFile )
	{
		OVR_FAIL( "ovrSoundAssetMapping::LoadSoundAssetsFromPackage failed json parse on %s", jsonFile );
	}
	free( buffer );

	LoadSoundAssetsFromJsonObject( url, dataFile );
}

void ovrSoundAssetMapping::LoadSoundAssetsFromJsonObject( const std::string & url, std::shared_ptr<OVR::JSON> dataFile )
{
	OVR_ASSERT( dataFile );

	// Read in sounds - add to map
	auto sounds = dataFile->GetItemByName( "Sounds" );
	OVR_ASSERT( sounds );
	
	const unsigned numSounds = sounds->GetItemCount();

	for ( unsigned i = 0; i < numSounds; ++i )
	{
		auto sound = sounds->GetItemByIndex( i );
		OVR_ASSERT( sound );

		std::string fullPath( url );
		fullPath += sound->GetStringValue().c_str();

		// Do we already have this sound?
		auto soundMapping = SoundMap.find( sound->Name );
		if ( soundMapping != SoundMap.end() )
		{
			OVR_LOG( "SoundManger - adding Duplicate sound %s with asset %s", sound->Name.c_str(), fullPath.c_str() );
		}
		else // add new sound
		{
			OVR_LOG( "SoundManger read in: %s -> %s", sound->Name.c_str(), fullPath.c_str() );
		}
		SoundMap[ sound->Name ] = fullPath;
	}
}

}
