#ifndef _NETLIB_CIRCULARBUFFER_H
#define _NETLIB_CIRCULARBUFFER_H

#include "../common/common.h"

class CircularBuffer
{
    uint8_t* m_buffer;
    uint8_t* m_bufferEnd;
    uint8_t* m_regionAPointer;
    size_t m_regionASize;
    uint8_t* m_regionBPointer;
    size_t m_regionBSize;
    
    inline size_t GetAFreeSpace() { return (m_bufferEnd - m_regionAPointer - m_regionASize); }
    inline size_t GetSpaceBeforeA() { return (m_regionAPointer - m_buffer); }
    inline size_t GetSpaceAfterA() { return (m_bufferEnd - m_regionAPointer - m_regionASize); }
    inline size_t GetBFreeSpace() { if(m_regionBPointer == NULL) { return 0; } return (m_regionAPointer - m_regionBPointer - m_regionBSize); }

public:
    CircularBuffer();
    ~CircularBuffer();
    bool Read(void* destination, size_t bytes);
    void AllocateB();
    bool Write(const void* data, size_t bytes);
    size_t GetSpace();
    uint32_t Peek(uint8_t* dst, uint32_t len);
    size_t GetSize();
    size_t GetContiguiousBytes();
    void Remove(size_t len);
    void* GetBuffer();
    void Allocate(size_t size);
    void IncrementWritten(size_t len);
    void* GetBufferStart();
};

#endif