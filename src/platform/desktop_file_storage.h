#pragma once

#include <filesystem>

#include "core/byte_storage.h"

class DesktopFileStorage : public ByteStorage
{
public:
    explicit DesktopFileStorage(
        std::filesystem::path path
    );

    ByteReadResult read() const override;

    ByteStorageError write(
        const std::vector<std::uint8_t>& bytes
    ) const override;

private:
    std::filesystem::path path_;
};