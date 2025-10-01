// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "FileSystem.h"
#include "FileList.h"

#include "ReadFile.h"
#include "MemoryFile.h"
#include "LimitReadFile.h"
#include "MountPointReader.h"

#include "Core/Logger/Logger.h"
#include "Core/Utility/StringUtil.h"


#if !defined (_WINDOWS_API_)
	#if (defined(_POSIX_API_) || defined(_OSX_PLATFORM_))
		#include <stdio.h>
		#include <stdlib.h>
		#include <string.h>
		#include <limits.h>
		#include <sys/types.h>
		#include <dirent.h>
		#include <sys/stat.h>
		#include <unistd.h>
	#endif
#endif

FileSystem* FileSystem::mFileSystem = NULL;

FileSystem* FileSystem::Get(void)
{
	LogAssert(FileSystem::mFileSystem, "Filesystem doesn't exist");
	return FileSystem::mFileSystem;
}

//! constructor
FileSystem::FileSystem()
{
	SetFileSystemType(FILESYSTEM_NATIVE);
	//! reset current working directory
	ChangeWorkingDirectoryTo(GetAbsolutePath(L""));

	if (FileSystem::mFileSystem)
	{
		LogError("Attempting to create two global filesystem! \
					The old one will be destroyed and overwritten with this one.");
		delete FileSystem::mFileSystem;
	}

	FileSystem::mFileSystem = this;
}

//! destructor
FileSystem::~FileSystem()
{
	if (FileSystem::mFileSystem == this)
		FileSystem::mFileSystem = nullptr;

	msDirectories.clear();
	//LogError("No directory list to deallocate.\n");
}

//! Creates an ReadFile interface for treating memory like a file.
BaseReadFile* FileSystem::CreateMemoryReadFile(const void* memory, int len, 
	const std::wstring& fileName, bool deleteMemoryWhenDropped)
{
	if (memory)
		return new MemoryReadFile(memory, len, fileName, deleteMemoryWhenDropped);
	
	return nullptr;
}

//! Creates an ReadFile interface for reading files inside files
BaseReadFile* FileSystem::CreateLimitReadFile(const std::wstring& fileName, 
	BaseReadFile* alreadyOpenedFile, long pos, long areaSize)
{
	if (alreadyOpenedFile)
		return new LimitReadFile(alreadyOpenedFile, pos, areaSize, fileName);

	return nullptr;
}

//! Creates an ReadFile interface for reading files
BaseReadFile* FileSystem::CreateReadFile(const std::wstring& fileName)
{
	return ReadFile::CreateReadFile(fileName);
}

//! Creates a list of files and directories in the current working directory
BaseFileList* FileSystem::CreateFileList()
{
	BaseFileList* r = 0;
	std::wstring filesPath = GetWorkingDirectory();
	filesPath += '/';

	//! Construct from native filesystem
	if (mFileSystemType == FILESYSTEM_NATIVE)
	{
		// --------------------------------------------
		//! Windows version
		#ifdef _WINDOWS_API_
		#if !defined ( _WIN32_WCE )

		r = new FileList(filesPath, true, false);

		// TODO: Should be unified once mingw adapts the proper types
#if defined(__GNUC__)
		long hFile; //mingw return type declaration
#else
		intptr_t hFile;
#endif

		struct _tfinddata_t c_file;
		if( (hFile = _tfindfirst( _T("*"), &c_file )) != -1L )
		{
			do
			{
				r->AddItem(
					filesPath + c_file.name, 0, c_file.size, (_A_SUBDIR & c_file.attrib) != 0, 0);
			}
			while( _tfindnext( hFile, &c_file ) == 0 );

			_findclose( hFile );
		}
		#endif

		#endif

		// --------------------------------------------
		//! Linux version
		#if (defined(_POSIX_API_) || defined(_OSX_PLATFORM_))

		r = new FileList(FilesPath, false, false);
		r->addItem(FilesPath + _GE_TEXT(".."), 0, 0, true, 0);

		//! We use the POSIX compliant methods instead of scandir
		DIR* dirHandle=opendir(FilesPath.c_str());
		if (dirHandle)
		{
			struct dirent *dirEntry;
			while ((dirEntry=readdir(dirHandle)))
			{
				unsigned int size = 0;
				bool isDirectory = false;

				if((strcmp(dirEntry->d_name, ".")==0) ||
				   (strcmp(dirEntry->d_name, "..")==0))
				{
					continue;
				}
				struct stat buf;
				if (stat(dirEntry->d_name, &buf)==0)
				{
					size = buf.st_size;
					isDirectory = S_ISDIR(buf.st_mode);
				}
				#if !defined(_SOLARIS_PLATFORM_) && !defined(__CYGWIN__)
				// only available on some systems
				else
				{
					isDirectory = dirEntry->d_type == DT_DIR;
				}
				#endif

				r->addItem(FilesPath + dirEntry->d_name, 0, size, isDirectory, 0);
			}
			closedir(dirHandle);
		}
		#endif
	}
	else
	{
		//! create file list for the virtual filesystem
		r = new FileList(filesPath, false, false);

		//! add relative navigation
		FileListEntry e2;
		FileListEntry e3;

		//! PWD
		r->AddItem(filesPath + L".", 0, 0, true, 0);

		//! parent
		r->AddItem(filesPath + L"..", 0, 0, true, 0);
	}

	if (r)
		r->Sort();
	return r;
}

// Returns a list of files in a given directory.
void FileSystem::GetFileList(
    std::vector<std::wstring>& result, const std::wstring& dir, bool makeFullPath)
{
	result.clear();
	std::wstring previousCWD = GetWorkingDirectory();

	if (!ChangeWorkingDirectoryTo(dir))
	{
		LogError("FileManager listFiles : Could not change CWD!");
		return;
	}

	BaseFileList* files = CreateFileList();
	for (int n = 0; n<(int)files->GetFileCount(); n++)
	{
		result.push_back(makeFullPath ? dir + L"/" +
			files->GetFileName(n) : files->GetFileName(n));
	}

	ChangeWorkingDirectoryTo(previousCWD);
	delete files;
}   // listFiles

//! Creates an empty filelist
BaseFileList* FileSystem::CreateEmptyFileList(const std::wstring& filesPath, bool ignoreCase, bool ignorePaths)
{
	return new FileList(filesPath, ignoreCase, ignorePaths);
}

//! Creates an archive file.
BaseFileArchive* FileSystem::CreateMountPointFileArchive(const std::wstring& filename, bool ignoreCase, bool ignorePaths)
{
	BaseFileArchive *archive = 0;
	BaseFileSystemType current = SetFileSystemType(FILESYSTEM_NATIVE);

	const std::wstring save = GetWorkingDirectory();
	std::wstring fullPath = GetAbsolutePath(filename);

	if (ChangeWorkingDirectoryTo(fullPath))
		archive = new MountPointReader(GetWorkingDirectory(), ignoreCase, ignorePaths);

	ChangeWorkingDirectoryTo(save);
	SetFileSystemType(current);

	return archive;
}

//! Returns the string of the current working directory
const std::wstring& FileSystem::GetWorkingDirectory()
{
	BaseFileSystemType type = mFileSystemType;

	if (type != FILESYSTEM_NATIVE)
	{
		type = FILESYSTEM_VIRTUAL;
	}
	else
	{
		#if defined(_WINDOWS_API_)
			wchar_t tmp[_MAX_PATH];
			_wgetcwd(tmp, _MAX_PATH);
			mWorkingDirectory[FILESYSTEM_NATIVE] = tmp;
			std::replace(
				mWorkingDirectory[FILESYSTEM_NATIVE].begin(),
				mWorkingDirectory[FILESYSTEM_NATIVE].end(), L'\\', L'/');
		#endif

		#if (defined(_POSIX_API_) || defined(_OSX_PLATFORM_))

			// getting the CWD is rather complex as we do not know the size
			// so try it until the call was successful
			// Note that neither the first nor the second parameter may be 0 according to POSIX
			unsigned int pathSize=256;
			wchar_t *tmpPath = new wchar_t[pathSize];
			while ((pathSize < (1<<16)) && !(wgetcwd(tmpPath,pathSize)))
			{
				SAFE_DELETE_ARRAY( tmpPath );
				pathSize *= 2;
				tmpPath = new char[pathSize];
			}
			if (tmpPath)
			{
				WorkingDirectory[FILESYSTEM_NATIVE] = tmpPath;
				delete[] tmpPath;
			}
		#endif
	}

	return mWorkingDirectory[type];
}

//! Changes the current Working Directory to the given string.
bool FileSystem::ChangeWorkingDirectoryTo(const std::wstring& newDirectory)
{
	bool success=false;

	if (mFileSystemType != FILESYSTEM_NATIVE)
	{
		mWorkingDirectory[FILESYSTEM_VIRTUAL] = newDirectory;
		success = true;
	}
	else
	{
		mWorkingDirectory[FILESYSTEM_NATIVE] = newDirectory;
		success = (_wchdir(newDirectory.c_str()) == 0);
	}

	return success;
}

std::wstring FileSystem::GetAbsolutePath(const std::wstring& filename) const
{
#if defined(_WINDOWS_API_)
	wchar_t tmp[_MAX_PATH];
	GetModuleFileName(NULL, tmp, _MAX_PATH);
	std::wstring wdir = tmp;
	std::replace(wdir.begin(), wdir.end(), L'\\', L'/');
	wdir = wdir.substr(0, wdir.find_last_of(L'/'));
	wdir += filename;

	return wdir;
#elif (defined(_POSIX_API_) || defined(_OSX_PLATFORM_))
	wchar_t* p=0;
	wchar_t fpath[4096];
	fpath[0]=0;
	p = realpath(filename.c_str(), fpath);
	if (!p)
	{
		// content in fpath is unclear at this point
		if (!fpath[0]) // seems like fpath wasn't altered, use our best guess
		{
			path tmp(filename);
			return tmp
		}
		else
			return std::wstring(fpath);
	}
	if (filename[filename.size()-1]=='/')
		return std::wstring(p)+ L"/";
	else
		return std::wstring(p);
#else
	return std::wstring(filename);
#endif
}

void FileSystem::GetRecursiveDirectories(std::vector<std::wstring>& directories, const std::wstring& dir)
{
    static const std::set<char> charsToIgnore = { '_', '.' };
    if (dir.empty() || !ExistDirectory(dir))
        return;

    GetRecursiveSubPaths(dir, directories, false, charsToIgnore);
}

std::vector<std::wstring> FileSystem::GetRecursiveDirectories(const std::wstring& dir)
{
    std::vector<std::wstring> result;
    GetRecursiveDirectories(result, dir);
    return result;
}

void FileSystem::GetRecursiveSubPaths(const std::wstring& path,
    std::vector<std::wstring>& dst, bool listPaths, const std::set<char>& ignore)
{
    std::vector<std::wstring> pathList;
    FileSystem::Get()->GetFileList(pathList, path, true);
    for (std::wstring const& path : pathList)
    {       
        std::string fileName = ToString(GetFileName(path));
        if (ignore.count(fileName[0]))
            continue;
        if (listPaths || ExistDirectory(path))
            dst.push_back(path);
        if (ExistDirectory(path))
            GetRecursiveSubPaths(path, dst, listPaths, ignore);
    }
}

//! returns the directory part of a filename, i.e. all until the first
//! slash or backslash, excluding it. If no directory path is prefixed, a '.'
//! is returned.
std::wstring FileSystem::GetFileDirectory(const std::wstring& filename) const
{
	// find last forward or backslash
	if (filename.rfind('/') != std::string::npos)
		return filename.substr(0, filename.rfind('/'));
	else 
		return L".";
}

//! returns the filename of a filepath. If no directory path is prefixed,
//! the file path is returned.
std::wstring FileSystem::GetFileName(const std::wstring& filepath) const
{
    // find last forward or backslash
    if (filepath.rfind('/') != std::string::npos)
        return filepath.substr(filepath.rfind('/') + 1, filepath.size());
    else
        return filepath;
}


bool FileSystem::SafeWriteToFile(const std::string& path, const std::string& content)
{
    std::string tmpFile = path + ".~mt";

    // Write to a tmp file
    std::ofstream os(tmpFile, std::ios::binary);
    if (!os.good())
        return false;
    os << content;
    os.flush();
    os.close();
    if (os.fail()) 
    {
        // Remove the temporary file because writing it failed and it's useless.
        remove(tmpFile.c_str());
        return false;
    }

    bool renameSuccess = false;

    // Move the finished temporary file over the real file
#ifdef _WIN32
    // When creating the file, it can cause Windows Search indexer, virus scanners and other apps
    // to query the file. This can make the move file call below fail.
    // We retry up to 5 times, with a 1ms sleep between, before we consider the whole operation failed
    int numberAttempts = 0;
    while (numberAttempts < 5) {
        renameSuccess = MoveFileEx(
            ToWideString(tmpFile).c_str(), 
            ToWideString(path).c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
        if (renameSuccess)
            break;
        Sleep(1);
        ++numberAttempts;
    }
#else
    // On POSIX compliant systems rename() is specified to be able to swap the
    // file in place of the destination file, making this a truly error-proof
    // transaction.
    renameSuccess = rename(tmpFile.c_str(), path.c_str()) == 0;
#endif
    if (!renameSuccess) 
    {
        LogWarning("Failed to write to file: " + path);
        // Remove the temporary file because moving it over the target file
        // failed.
        remove(tmpFile.c_str());
        return false;
    }

    return true;
}

//----------------------------------------------------------------------------
bool FileSystem::InsertDirectory(const std::string& directory)
{
	std::vector<std::string>::iterator iter = msDirectories.begin();
	std::vector<std::string>::iterator end = msDirectories.end();
	for (/**/; iter != end; ++iter)
	{
		if (directory == *iter)
		{
			return false;
		}
	}
	msDirectories.push_back(directory);
	return true;
}

//----------------------------------------------------------------------------
bool FileSystem::DeleteDirectory(const std::string& directory)
{
	std::vector<std::string>::iterator iter = msDirectories.begin();
	std::vector<std::string>::iterator end = msDirectories.end();
	for (/**/; iter != end; ++iter)
	{
		if (directory == *iter)
		{
			msDirectories.erase(iter);
			return true;
		}
	}
	return false;
}

//----------------------------------------------------------------------------

bool IsDirDelimiter(char c)
{
    return c == '/' || c == '\\';
}

std::string FileSystem::RemoveLastPathComponent(
    const std::string& path, std::string* removed, int count)
{
    if (removed)
        *removed = "";

    size_t remaining = path.size();

    for (int i = 0; i < count; ++i) 
    {
        // strip a dir delimiter
        while (remaining != 0 && IsDirDelimiter(path[remaining - 1]))
            remaining--;
        // strip a path component
        size_t componentEnd = remaining;
        while (remaining != 0 && !IsDirDelimiter(path[remaining - 1]))
            remaining--;
        size_t componentStart = remaining;
        // strip a dir delimiter
        while (remaining != 0 && IsDirDelimiter(path[remaining - 1]))
            remaining--;
        if (removed) 
        {
            std::string component = path.substr(componentStart, componentEnd - componentStart);
            if (i)
                *removed = component + "\\" + *removed;
            else
                *removed = component;
        }
    }
    return path.substr(0, remaining);
}


//----------------------------------------------------------------------------


bool CreateDir(const std::string& path)
{
    bool r = CreateDirectoryA(path.c_str(), NULL);
    if (r == true)
        return true;
    if (GetLastError() == ERROR_ALREADY_EXISTS)
        return true;
    return false;
}

bool FileSystem::CreateAllDirectories(const std::string& path)
{
    std::vector<std::string> toCreate;
    std::string basePath = path;
    while (!ExistDirectory(ToWideString(basePath)))
    {
        toCreate.push_back(basePath);
        basePath = RemoveLastPathComponent(basePath);
        if (basePath.empty())
            break;
    }
    for (int i = (int)toCreate.size() - 1; i >= 0; i--)
    {
        if (!CreateDir(toCreate[i]))
            return false;

        InsertDirectory(toCreate[i]);
    }
    return true;
}

//----------------------------------------------------------------------------
void FileSystem::RemoveAllDirectories()
{
	msDirectories.clear();
}

//----------------------------------------------------------------------------
std::string FileSystem::GetPath(const std::string& fileName)
{
	std::vector<std::string>::iterator iter = msDirectories.begin();
	std::vector<std::string>::iterator end = msDirectories.end();
	for (/**/; iter != end; ++iter)
	{
		std::string decorated = *iter + fileName;
		if (access(decorated.c_str(), 0) != -1)
		{
			return decorated;
		}
	}

	LogError("File not found : " + fileName);
	return std::string();
}
//----------------------------------------------------------------------------

//! determines if a directory exists and would be able to be opened.
bool FileSystem::ExistDirectory(const std::wstring& dirname)
{
#ifdef _WIN32
	DWORD attr = GetFileAttributes(dirname.c_str());
	return (attr != INVALID_FILE_ATTRIBUTES &&
		(attr & FILE_ATTRIBUTE_DIRECTORY));
#else
	struct stat statbuf {};
	if (stat(path.c_str(), &statbuf))
		return false; // Actually error; but certainly not a directory
	return ((statbuf.st_mode & S_IFDIR) == S_IFDIR);
#endif
}

//! determines if a file exists and would be able to be opened.
bool FileSystem::ExistFile(const std::wstring& filename)
{
	if (filename.empty())
		return false;

	std::string fileName = ToString(filename);
	if (access(fileName.c_str(), 0) != -1)
		return true;

	std::vector<std::string>::iterator iter = msDirectories.begin();
	std::vector<std::string>::iterator end = msDirectories.end();
	for (/**/; iter != end; ++iter)
	{
		std::string decorated = *iter + fileName;
		if (access(decorated.c_str(), 0) != -1)
			return true;
	}

	return false;
}

//! Get the current file systen type
BaseFileSystemType FileSystem::GetFileSystemType( )
{
	return mFileSystemType;
}

//! Sets the current file systen type
BaseFileSystemType FileSystem::SetFileSystemType(BaseFileSystemType listType)
{
	BaseFileSystemType current = mFileSystemType;
	mFileSystemType = listType;
	return current;
}