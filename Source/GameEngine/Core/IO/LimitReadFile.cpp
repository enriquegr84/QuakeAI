// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "LimitReadFile.h"


LimitReadFile::LimitReadFile(BaseReadFile* alreadyOpenedFile, 
	long pos, long areaSize, const std::wstring& name)
:	mFileName(name), mAreaStart(0), mAreaEnd(0), mPosition(0), mFile(alreadyOpenedFile)
{
	if (mFile)
	{
		mAreaStart = pos;
		mAreaEnd = mAreaStart + areaSize;
	}
}


LimitReadFile::~LimitReadFile()
{
}


//! returns how much was read
int LimitReadFile::Read(void* buffer, unsigned int sizeToRead)
{
	if (0 == mFile)
		return 0;

	int r = mAreaStart + mPosition;
	int toRead = 
		std::min(mAreaEnd, (long)(r + sizeToRead)) - 
		std::max(mAreaStart, (long)r);
	if (toRead < 0)
		return 0;
	mFile->Seek(r);
	r = mFile->Read(buffer, toRead);
    mPosition += r;
	return r;
}


//! changes position in file, returns true if successful
bool LimitReadFile::Seek(long finalPos, bool relativeMovement)
{
    mPosition = std::clamp(finalPos + (relativeMovement ? mPosition : 0 ), (long)0, mAreaEnd - mAreaStart);
	return true;
}


//! returns size of file
long LimitReadFile::GetSize() const
{
	return mAreaEnd - mAreaStart;
}


//! returns where in the file we are.
long LimitReadFile::GetPosition() const
{
	return mPosition;
}


//! returns name of file
const std::wstring& LimitReadFile::GetFileName() const
{
	return mFileName;
}


BaseReadFile* CreateLimitReadFile(
	const std::wstring& fileName, BaseReadFile* alreadyOpenedFile, long pos, long areaSize)
{
	return new LimitReadFile(alreadyOpenedFile, pos, areaSize, fileName);
}

