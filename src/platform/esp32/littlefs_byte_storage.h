#pragma once

#include <string>
#include <vector>

#include "core/byte_storage.h"

struct LittleFsApi
{
    using MountFunction =
        bool (*)(bool formatOnFailure);

    using ExistsFunction =
        bool (*)(const char *path);

    using ReadFileFunction =
        bool (*)(
            const char *path,
            std::vector<std::uint8_t> &bytes);

    using WriteFileFunction =
        bool (*)(
            const char *path,
            const std::vector<std::uint8_t> &bytes);

    using RenameFileFunction =
        bool (*)(
            const char *sourcePath,
            const char *destinationPath);

    using RemoveFileFunction =
        bool (*)(const char *path);

    MountFunction mount = nullptr;
    ExistsFunction exists = nullptr;
    ReadFileFunction readFile = nullptr;
    WriteFileFunction writeFile = nullptr;
    RenameFileFunction renameFile = nullptr;
    RemoveFileFunction removeFile = nullptr;
};

class LittleFsByteStorage final : public ByteStorage
{
public:
    LittleFsByteStorage();

    LittleFsByteStorage(
        std::string path,
        LittleFsApi api);

    bool begin();

    ByteReadResult read() const override;

    ByteStorageError write(
        const std::vector<std::uint8_t> &bytes) const override;

private:
    void removeTemporaryFile() const;

    std::string path_;
    std::string temporaryPath_;
    LittleFsApi api_;
    bool mounted_ = false;
};