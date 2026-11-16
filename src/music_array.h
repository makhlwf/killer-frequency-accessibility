#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
inline std::vector<void *> musicArray(void *value) {
    if (!value)
        return {};
    auto n = *reinterpret_cast<uintptr_t *>(static_cast<char *>(value) + 24);
    if (n > 50000)
        return {};
    auto data = reinterpret_cast<void **>(static_cast<char *>(value) + 32);
    return std::vector<void *>(data, data + n);
}
