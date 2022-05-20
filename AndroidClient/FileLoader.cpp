#include "FileLoader.h"
#include "Platform/Core/StringToWString.h"
#include "Platform/Core/RuntimeError.h"
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <unistd.h>
#include <stdio.h> // for fopen, seek, fclose
#include <stdlib.h> // for malloc, free
#include <time.h>

typedef struct stat Stat;
using namespace teleport;
using namespace android;

AAssetManager* android::FileLoader::s_AssetManager = nullptr;

#if PLATFORM_STD_FILESYSTEM==0
#define SIMUL_FILESYSTEM 0
#elif PLATFORM_STD_FILESYSTEM==1
#define SIMUL_FILESYSTEM 1
#include <filesystem>
namespace fs = std::filesystem;
#else
#define SIMUL_FILESYSTEM 1
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif

static int do_mkdir(const char *path_utf8)
{
	ALWAYS_ERRNO_CHECK
    int             status = 0;
    Stat            st;
    if (stat(path_utf8, &st)!=0)
    {
        /* Directory does not exist. EEXIST for race condition */
        if (mkdir(path_utf8,S_IRWXU) != 0 && errno != EEXIST)
            status = -1;
    }
    else if (!(st.st_mode & S_IFDIR))
    {
        //errno = ENOTDIR;
        status = -1;
    }

	errno=0;
    return(status);
}

static int nextslash(const std::string &str,int pos)
{
	int slash=(int)str.find('/',pos);
	int back=(int)str.find('\\',pos);
	if(slash<0||(back>=0&&back<slash))
		slash=back;
	return slash;
}

static int mkpath(const std::string &filename_utf8)
{
    int status = 0;
	int pos=0;
    while (status == 0 && (pos = nextslash(filename_utf8,pos))>=0)
    {
		status = do_mkdir(filename_utf8.substr(0,pos).c_str());
		pos++;
    }
    return (status);
}

FileLoader::FileLoader()
{
}

bool FileLoader::FileExists(const char *filename_utf8) const
{
	enum access_mode
	{
		NO_FILE=-1,EXIST=0,WRITE=2,READ=4
	};
	AAsset* asset = AAssetManager_open(s_AssetManager, filename_utf8, AASSET_MODE_UNKNOWN);
	bool bExists = (asset != nullptr);
	if (asset)
		AAsset_close(asset);
	return bExists;
}


bool FileLoader::Save(void* pointer, unsigned int bytes, const char* filename_utf8,bool save_as_text)
{
	SIMUL_CERR << "FileLoader::Save not implemented."<< std::endl;
	return true;
}

void FileLoader::AcquireFileContents(void*& pointer, unsigned int& bytes, const char* filename_utf8,bool open_as_text)
{
	AAsset* asset = AAssetManager_open(s_AssetManager, filename_utf8, AASSET_MODE_BUFFER);
	if(!asset)
	{
		SIMUL_CERR << "Failed to load file " << filename_utf8 << std::endl;
		pointer = NULL;
		return;
	}
	int64_t b= AAsset_getLength(asset);
	bytes=  b;
	if(bytes!=b)
	{
		SIMUL_CERR << "Failed to load file - too large for FileLoader: " << filename_utf8 << std::endl;
		pointer = NULL;
		return;
	}

	pointer = malloc(bytes+1);
	const void *source=AAsset_getBuffer(asset);
	memcpy(pointer, source,bytes);
	if(open_as_text)
		((char*)pointer)[bytes]=0;
	
	AAsset_close(asset);
}

double FileLoader::GetFileDate(const char* filename_utf8) const
{
	if(!FileExists(filename_utf8))
		return 0.0;

	std::wstring wstr=platform::core::Utf8ToWString(filename_utf8);
	FILE *fp = NULL;
	fp = fopen(filename_utf8,"rb");//open_as_text?L"r, ccs=UTF-8":
	if(!fp)
	{
		//std::cerr<<"Failed to find file "<<filename_utf8<<std::endl;
		return 0.0;
	}
	fclose(fp);
#if SIMUL_FILESYSTEM
    return (double)(fs::last_write_time(filename_utf8).time_since_epoch().count())/(3600.0*24.0*1000000.0);
#else
	return 0;
#endif
}

void FileLoader::ReleaseFileContents(void* pointer)
{
	free(pointer);
}
