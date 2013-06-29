/*******************************************************************************
* This file is part of AvsVCEh264.
* Contains declaration & functions for circularBuffer
*
* Copyright (C) 2013 David González García <davidgg666@gmail.com>
*******************************************************************************/
#include <malloc.h>

typedef void* BufferType;

typedef struct
{
    BYTE write;
    BYTE read;
    BufferType keys[0];
} Buffer;

Buffer* newBuffer()
{
    int sz = 256 * sizeof(BufferType) + sizeof(Buffer);
    Buffer *que = (Buffer*) malloc(sz);
	que->write = que->read = 0;
    return que;
}

inline bool BufferIsFull(Buffer *que)
{
	return que->read == (BYTE)(que->write + 1);
}

inline bool BufferIsEmpty(Buffer *que)
{
    return que->read == que->write;
}

inline bool BufferWrite(Buffer *que, BufferType k)
{
	if (BufferIsFull(que))
		return false;

	que->keys[que->write] = k;
	que->write++;
    return true;
}

inline int BufferRead(Buffer *que, BufferType *pK)
{
    if (BufferIsEmpty(que))
		return 0;

	*pK = que->keys[que->read];
	que->read++;
    return true;
}
