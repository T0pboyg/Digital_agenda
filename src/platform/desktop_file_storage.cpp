#include "desktop_file_storage.h"

#include <fstream>
#include <utility>

DesktopFileStorage::DesktopFileStorage(
    std::filesystem::path path
)
    : path_(std::move(path))
{
}

ByteReadResult DesktopFileStorage::read() const
{
    std::error_code error;

    const bool exists =
        std::filesystem::exists(
            path_,
            error
        );

    if (error)
    {
        return ByteReadResult{
            ByteStorageError::ReadFailed,
            {}
        };
    }

    if (!exists)
    {
        return ByteReadResult{
            ByteStorageError::NotFound,
            {}
        };
    }

    const bool isRegularFile =
        std::filesystem::is_regular_file(
            path_,
            error
        );

    if (error || !isRegularFile)
    {
        return ByteReadResult{
            ByteStorageError::ReadFailed,
            {}
        };
    }

    std::ifstream file(
        path_,
        std::ios::binary
    );

    if (!file)
    {
        return ByteReadResult{
            ByteStorageError::ReadFailed,
            {}
        };
    }

    std::vector<std::uint8_t> bytes;
    char character = 0;

    while (file.get(character))
    {
        bytes.push_back(
            static_cast<std::uint8_t>(
                static_cast<unsigned char>(
                    character
                )
            )
        );
    }

    if (!file.eof())
    {
        return ByteReadResult{
            ByteStorageError::ReadFailed,
            {}
        };
    }

    return ByteReadResult{
        ByteStorageError::None,
        bytes
    };
}

ByteStorageError DesktopFileStorage::write(
    const std::vector<std::uint8_t>& bytes
) const
{
    const std::filesystem::path parent =
        path_.parent_path();

    if (!parent.empty())
    {
        std::error_code error;

        std::filesystem::create_directories(
            parent,
            error
        );

        if (error)
        {
            return ByteStorageError::WriteFailed;
        }
    }

    std::ofstream file(
        path_,
        std::ios::binary | std::ios::trunc
    );

    if (!file)
    {
        return ByteStorageError::WriteFailed;
    }

    if (!bytes.empty())
    {
        file.write(
            reinterpret_cast<const char*>(
                bytes.data()
            ),
            static_cast<std::streamsize>(
                bytes.size()
            )
        );
    }

    file.flush();

    if (!file)
    {
        return ByteStorageError::WriteFailed;
    }

    return ByteStorageError::None;
}