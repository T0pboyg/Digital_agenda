#include "littlefs_byte_storage.h"

#include <utility>

#ifdef ARDUINO
#include <FS.h>
#include <LittleFS.h>
#endif

namespace
{
#ifdef ARDUINO
    bool mountLittleFs(
        bool formatOnFailure)
    {
        return LittleFS.begin(
            formatOnFailure);
    }

    bool littleFsFileExists(
        const char *path)
    {
        return LittleFS.exists(path);
    }

    bool readLittleFsFile(
        const char *path,
        std::vector<std::uint8_t> &bytes)
    {
        fs::File file =
            LittleFS.open(
                path,
                FILE_READ);

        if (!file || file.isDirectory())
        {
            return false;
        }

        const std::size_t fileSize =
            file.size();

        bytes.clear();
        bytes.resize(fileSize);

        std::size_t totalRead = 0;

        while (totalRead < fileSize)
        {
            const std::size_t readCount =
                file.read(
                    bytes.data() + totalRead,
                    fileSize - totalRead);

            if (readCount == 0)
            {
                bytes.clear();
                file.close();
                return false;
            }

            totalRead += readCount;
        }

        file.close();
        return true;
    }

    bool writeLittleFsFile(
        const char *path,
        const std::vector<std::uint8_t> &bytes)
    {
        fs::File file =
            LittleFS.open(
                path,
                FILE_WRITE);

        if (!file || file.isDirectory())
        {
            return false;
        }

        std::size_t totalWritten = 0;

        while (totalWritten < bytes.size())
        {
            const std::size_t writeCount =
                file.write(
                    bytes.data() + totalWritten,
                    bytes.size() - totalWritten);

            if (writeCount == 0)
            {
                file.close();
                return false;
            }

            totalWritten += writeCount;
        }

        file.flush();
        file.close();

        return totalWritten == bytes.size();
    }

    bool renameLittleFsFile(
        const char *sourcePath,
        const char *destinationPath)
    {
        return LittleFS.rename(
            sourcePath,
            destinationPath);
    }

    bool removeLittleFsFile(
        const char *path)
    {
        if (!LittleFS.exists(path))
        {
            return true;
        }

        return LittleFS.remove(path);
    }
#else
    bool mountLittleFs(
        bool)
    {
        return false;
    }

    bool littleFsFileExists(
        const char *)
    {
        return false;
    }

    bool readLittleFsFile(
        const char *,
        std::vector<std::uint8_t> &)
    {
        return false;
    }

    bool writeLittleFsFile(
        const char *,
        const std::vector<std::uint8_t> &)
    {
        return false;
    }

    bool renameLittleFsFile(
        const char *,
        const char *)
    {
        return false;
    }

    bool removeLittleFsFile(
        const char *)
    {
        return false;
    }
#endif

    LittleFsApi defaultLittleFsApi()
    {
        return LittleFsApi{
            mountLittleFs,
            littleFsFileExists,
            readLittleFsFile,
            writeLittleFsFile,
            renameLittleFsFile,
            removeLittleFsFile};
    }
}

LittleFsByteStorage::LittleFsByteStorage()
    : LittleFsByteStorage(
          std::string{"/agenda.bin"})
{
}

LittleFsByteStorage::LittleFsByteStorage(
    std::string path)
    : LittleFsByteStorage(
          std::move(path),
          defaultLittleFsApi())
{
}

LittleFsByteStorage::LittleFsByteStorage(
    std::string path,
    LittleFsApi api)
    : path_(std::move(path)),
      temporaryPath_(path_ + ".tmp"),
      api_(api)
{
}

bool LittleFsByteStorage::begin()
{
    mounted_ =
        api_.mount != nullptr &&
        api_.mount(false);

    return mounted_;
}

ByteReadResult LittleFsByteStorage::read() const
{
    if (
        !mounted_ ||
        api_.exists == nullptr ||
        api_.readFile == nullptr)
    {
        return ByteReadResult{
            ByteStorageError::ReadFailed,
            {}};
    }

    if (!api_.exists(path_.c_str()))
    {
        return ByteReadResult{
            ByteStorageError::NotFound,
            {}};
    }

    std::vector<std::uint8_t> bytes;

    if (
        !api_.readFile(
            path_.c_str(),
            bytes))
    {
        return ByteReadResult{
            ByteStorageError::ReadFailed,
            {}};
    }

    return ByteReadResult{
        ByteStorageError::None,
        std::move(bytes)};
}

ByteStorageError LittleFsByteStorage::write(
    const std::vector<std::uint8_t> &bytes) const
{
    if (
        !mounted_ ||
        api_.writeFile == nullptr ||
        api_.renameFile == nullptr)
    {
        return ByteStorageError::WriteFailed;
    }

    removeTemporaryFile();

    if (
        !api_.writeFile(
            temporaryPath_.c_str(),
            bytes))
    {
        removeTemporaryFile();
        return ByteStorageError::WriteFailed;
    }

    if (
        !api_.renameFile(
            temporaryPath_.c_str(),
            path_.c_str()))
    {
        removeTemporaryFile();
        return ByteStorageError::WriteFailed;
    }

    return ByteStorageError::None;
}

void LittleFsByteStorage::
    removeTemporaryFile() const
{
    if (api_.removeFile != nullptr)
    {
        api_.removeFile(
            temporaryPath_.c_str());
    }
}
