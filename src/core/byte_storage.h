#pragma once

#include <cstdint>
#include <vector>

enum class ByteStorageError
{
    None,
    NotFound,
    ReadFailed,
    WriteFailed
};

struct ByteReadResult
{
    ByteStorageError error;
    std::vector<std::uint8_t> bytes;
};

class ByteStorage
{
public:
    virtual ~ByteStorage() = default;

    virtual ByteReadResult read() const = 0;

    virtual ByteStorageError write(
        const std::vector<std::uint8_t>& bytes
    ) const = 0;
};