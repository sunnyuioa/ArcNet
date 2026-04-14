#ifndef _MOL_NET_MESSAGE_H_INCL
#define _MOL_NET_MESSAGE_H_INCL

#include "message.h"
#include <string>
#include <cstring>
#include <cstdint>

class CMessageOut
{
public:
    CMessageOut();
    CMessageOut(char *data, int length, int mpos = 0) {
        this->mPos = mpos + 1;
        this->mLength = length + msg.header.headerLen;
        this->msg.write_data(data, this->mPos, length);
    };
    
    CMessageOut(ProtoMessage ms) : msg(ms) {
        mLength = msg.header.bodyLen + msg.header.headerLen;
    }
    
    ~CMessageOut();
    void clear();
    void write16(int16_t value);
    void write32(int32_t value);
    MsgHeader& getHeader() { return msg.header; }
    void write8(uint8_t value);
    void writeBytes(uint8_t* data, uint32_t length);
    void write16(uint16_t value);
    void write32(uint32_t value);
    void writeFloat(float value);
    void write64(int64_t value);
    void writeString(const std::string &str, int length = -1);
    const ProtoMessage getData() const { return this->msg; }
    unsigned int getLength() const { return mLength; }
    
private:
    ProtoMessage msg;
    uint32_t mLength;
    uint32_t mPos;
};

class CMolMessageIn
{
public:
    CMolMessageIn(const char *data, int length) {
        if (data == nullptr || length < sizeof(MsgHeader)) {
            mLength = 0;
            mPos = 0;
            msg = nullptr;
            return;
        }
        memcpy(mBuffer, data, length);
        msg = (ProtoMessage*)mBuffer;
        mLength = length;
        mPos = sizeof(MsgHeader);
    }
    
    ~CMolMessageIn();
    int getLength() const { return mLength; }
    int16_t read16();
    int8_t  read8();
    int32_t read32();
    float readFloat();
    int64_t read64();
    std::string readString(int length = -1);
    int getUnreadLength() const { return mLength - mPos; }
    const char* getData() const { return msg->getdata_readonly(); }
    
private:
    char mBuffer[MSG_MAX];
    ProtoMessage *msg;
    uint32_t mLength;
    uint32_t mPos;
};

#endif