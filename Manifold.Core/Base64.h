#pragma once
#include <string>
#include <vector>

namespace Manifold::Core
{
    inline std::string Base64Encode(const std::vector<uint8_t>& data)
    {
        static constexpr char kTable[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        std::string result;
        result.reserve(((data.size() + 2) / 3) * 4);

        size_t i = 0;
        for (; i + 2 < data.size(); i += 3)
        {
            uint32_t n = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
            result += kTable[(n >> 18) & 0x3F];
            result += kTable[(n >> 12) & 0x3F];
            result += kTable[(n >> 6) & 0x3F];
            result += kTable[n & 0x3F];
        }
        if (i + 1 == data.size())
        {
            uint32_t n = data[i] << 16;
            result += kTable[(n >> 18) & 0x3F];
            result += kTable[(n >> 12) & 0x3F];
            result += '=';
            result += '=';
        }
        else if (i + 2 == data.size())
        {
            uint32_t n = (data[i] << 16) | (data[i + 1] << 8);
            result += kTable[(n >> 18) & 0x3F];
            result += kTable[(n >> 12) & 0x3F];
            result += kTable[(n >> 6) & 0x3F];
            result += '=';
        }
        return result;
    }
}
