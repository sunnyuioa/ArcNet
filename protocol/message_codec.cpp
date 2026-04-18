#include"protocol/message_codec.h"
CMessageOut::CMessageOut() {
    clear();
}

void CMessageOut::clear() {
    mPos = 0;
    mLength = sizeof(MsgHeader);
    memset(msg.body, 0, MSG_MAX);
}

CMessageOut::~CMessageOut() {
}

void CMessageOut::write16(uint16_t value) {
    if (mPos + 2 > MSG_MAX) return;
    msg.body[mPos++] = (uint8_t)(value & 0xFF);
    msg.body[mPos++] = (uint8_t)((value >> 8) & 0xFF);
    msg.header.bodyLen = mPos;
    mLength = msg.header.headerLen + msg.header.bodyLen;
}

void CMessageOut::write16(int16_t value) {
    write16((uint16_t)value);
}

void CMessageOut::write32(uint32_t value) {
    if (mPos + 4 > MSG_MAX) return;
    msg.body[mPos++] = (uint8_t)(value & 0xFF);
    msg.body[mPos++] = (uint8_t)((value >> 8) & 0xFF);
    msg.body[mPos++] = (uint8_t)((value >> 16) & 0xFF);
    msg.body[mPos++] = (uint8_t)((value >> 24) & 0xFF);
    msg.header.bodyLen = mPos;
    mLength = msg.header.headerLen + msg.header.bodyLen;
}

void CMessageOut::write32(int32_t value) {
    write32((uint32_t)value);
}

void CMessageOut::write64(int64_t value) {
    uint64_t v = (uint64_t)value;
    for (int i = 0; i < 8; ++i) {
        if (mPos >= MSG_MAX) break;
        msg.body[mPos++] = (uint8_t)(v & 0xFF);
        v >>= 8;
    }
    msg.header.bodyLen = mPos;
    mLength = msg.header.headerLen + msg.header.bodyLen;
}

void CMessageOut::writeFloat(float value) {
    uint32_t* p = (uint32_t*)&value;
    write32(*p);
}

void CMessageOut::writeString(const std::string &str, int length) {
    std::cout<<"正在写入字符串: " << str << " 长度: " << length ;
    int len = length < 0 ? str.size() : length;
    if (mPos + len > MSG_MAX) return;
    std::cout<<"写入成功: " << str << " 长度: " << len ;
    memcpy(msg.body + mPos, str.c_str(), len);
    mPos += len;
    msg.header.bodyLen = mPos;
    mLength = msg.header.headerLen + msg.header.bodyLen;
}

void CMessageOut::writeBytes(uint8_t* data, uint32_t length) {
    if (!data || mPos + length > MSG_MAX) return;
    memcpy(msg.body + mPos, data, length);
    mPos += length;
    msg.header.bodyLen = mPos;
    mLength = msg.header.headerLen + msg.header.bodyLen;
}

void CMessageOut::write8(uint8_t value) {
    if (mPos + 1 > MSG_MAX) return;
    msg.body[mPos++] = value;
    msg.header.bodyLen = mPos;
    mLength = msg.header.headerLen + msg.header.bodyLen;
}

CMolMessageIn::~CMolMessageIn() {}

int8_t CMolMessageIn::read8() {
    if (mPos + 1 > mLength) return 0;
    return (int8_t)msg->body[mPos++];
}

int16_t CMolMessageIn::read16() {
    if (mPos + 2 > mLength) return 0;
    uint16_t val = 0;
    val |= (uint8_t)msg->body[mPos++];
    val |= (uint16_t)(uint8_t)msg->body[mPos++] << 8;
    return (int16_t)val;
}

int32_t CMolMessageIn::read32() {
    if (mPos + 4 > mLength) return 0;
    uint32_t val = 0;
    val |= (uint8_t)msg->body[mPos++];
    val |= (uint32_t)(uint8_t)msg->body[mPos++] << 8;
    val |= (uint32_t)(uint8_t)msg->body[mPos++] << 16;
    val |= (uint32_t)(uint8_t)msg->body[mPos++] << 24;
    return (int32_t)val;
}

int64_t CMolMessageIn::read64() {
    if (mPos + 8 > mLength) return 0;
    uint64_t val = 0;
    for (int i = 0; i < 8; ++i) {
        val |= (uint64_t)(uint8_t)msg->body[mPos++] << (i * 8);
    }
    return (int64_t)val;
}

float CMolMessageIn::readFloat() {
    uint32_t val = (uint32_t)read32();
    return *(float*)&val;
}

std::string CMolMessageIn::readString(int length) {
    if (length <= 0) {
        length = getUnreadLength();
    }
    if (mPos + length > mLength) {
        return "";
    }
    std::string str(msg->body + mPos, length);
    mPos += length;
    return str;
}