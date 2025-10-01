// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "MemoryFile.h"


MemoryReadFile::MemoryReadFile(const void* memory, long len, 
	const std::wstring& fileName, bool d)
:	mBuffer(memory), mLength(len), mPosition(0),
	mFileName(fileName), mDeleteMemoryWhenDropped(d)
{

}


MemoryReadFile::~MemoryReadFile()
{
	if (mDeleteMemoryWhenDropped)
		delete mBuffer;
}


//! returns how much was read
int MemoryReadFile::Read(void* buffer, unsigned int sizeToRead)
{
	int amount = static_cast<int>(sizeToRead);
	if (mPosition + amount > mLength)
		amount -= mPosition + amount - mLength;

	if (amount <= 0)
		return 0;

	char* p = (char*)mBuffer;
	memcpy(buffer, p + mPosition, amount);
    mPosition += amount;

	return amount;
}

//! changes position in file, returns true if successful
//! if relativeMovement==true, the pos is changed relative to current pos,
//! otherwise from begin of file
bool MemoryReadFile::Seek(long finalPos, bool relativeMovement)
{
	if (relativeMovement)
	{
		if (mPosition + finalPos > mLength)
			return false;

        mPosition += finalPos;
	}
	else
	{
		if (finalPos > mLength)
			return false;

        mPosition = finalPos;
	}

	return true;
}


//! returns size of file
long MemoryReadFile::GetSize() const
{
	return mLength;
}


//! returns where in the file we are.
long MemoryReadFile::GetPosition() const
{
	return mPosition;
}


//! returns name of file
const std::wstring& MemoryReadFile::GetFileName() const
{
	return mFileName;
}

BaseReadFile* CreateMemoryReadFile(const void* memory, long size, 
	const std::wstring& fileName, bool deleteMemoryWhenDropped)
{
	MemoryReadFile* file = new MemoryReadFile(
		memory, size, fileName, deleteMemoryWhenDropped);
	return file;
}

