#ifndef MESSAGE_H
#define MESSAGE_H

#pragma once

#include <cstdint>
#include <cstring>

constexpr uint32_t PROTO_MAGIC = 0xABCD1234;
constexpr uint16_t MSG_Min = 16384;
constexpr uint16_t MSG_MAX = 32767;

#pragma pack(push, 1)
struct MsgHeader {
    uint32_t magic      = PROTO_MAGIC;
    uint16_t headerLen  = sizeof(MsgHeader);
    uint16_t bodyLen    = 0;
};
#pragma pack(pop)

struct ProtoMessage
{
    MsgHeader header;
    char body[MSG_MAX];

    const char* getdata_readonly() const {
        return body;
    }

    void write_data(const char* data, int pos, int len) {
        if (pos + len > MSG_MAX) return;
        ::memcpy(body + pos, data, len);
    }

    void clean() {
        ::memset(body, 0, sizeof(body));
    }
};

#endif