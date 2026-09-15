#include <cassert>
#include <cstdint>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <type_traits>

#include "platform/esp32/littlefs_byte_storage.h"

namespace
{
    bool mountSucceeds = true;
    bool readSucceeds = true;
    bool writeSucceeds = true;
    bool renameSucceeds = true;

    bool mountCalled = false;
    bool receivedFormatOnFailure = true;

    std::map<
        std::string,
        std::vector<std::uint8_t>>
        files;

    bool fakeMount(
        bool formatOnFailure)
    {
        mountCalled = true;

        receivedFormatOnFailure =
            formatOnFailure;

        return mountSucceeds;
    }

    bool fakeExists(
        const char *path)
    {
        return files.find(path) !=
               files.end();
    }

    bool fakeReadFile(
        const char *path,
        std::vector<std::uint8_t> &bytes)
    {
        if (!readSucceeds)
        {
            return false;
        }

        const auto file =
            files.find(path);

        if (file == files.end())
        {
            return false;
        }

        bytes = file->second;
        return true;
    }

    bool fakeWriteFile(
        const char *path,
        const std::vector<std::uint8_t> &bytes)
    {
        if (!writeSucceeds)
        {
            return false;
        }

        files[path] = bytes;
        return true;
    }

    bool fakeRenameFile(
        const char *sourcePath,
        const char *destinationPath)
    {
        if (!renameSucceeds)
        {
            return false;
        }

        const auto source =
            files.find(sourcePath);

        if (source == files.end())
        {
            return false;
        }

        const std::vector<std::uint8_t> bytes =
            source->second;

        files.erase(source);
        files[destinationPath] = bytes;

        return true;
    }

    bool fakeRemoveFile(
        const char *path)
    {
        files.erase(path);
        return true;
    }

    void resetFakeFileSystem()
    {
        mountSucceeds = true;
        readSucceeds = true;
        writeSucceeds = true;
        renameSucceeds = true;

        mountCalled = false;
        receivedFormatOnFailure = true;

        files.clear();
    }

    LittleFsApi makeFakeApi()
    {
        return LittleFsApi{
            fakeMount,
            fakeExists,
            fakeReadFile,
            fakeWriteFile,
            fakeRenameFile,
            fakeRemoveFile};
    }

    LittleFsByteStorage makeStorage()
    {
        return LittleFsByteStorage{
            "/agenda.bin",
            makeFakeApi()};
    }

    void test_mount_never_formats_and_handles_missing_file()
    {
        resetFakeFileSystem();

        LittleFsByteStorage storage =
            makeStorage();

        assert(storage.begin());
        assert(mountCalled);
        assert(!receivedFormatOnFailure);

        const ByteReadResult missing =
            storage.read();

        assert(
            missing.error ==
            ByteStorageError::NotFound);

        assert(missing.bytes.empty());

        resetFakeFileSystem();
        mountSucceeds = false;

        LittleFsByteStorage failedStorage =
            makeStorage();

        assert(!failedStorage.begin());

        assert(
            failedStorage.read().error ==
            ByteStorageError::ReadFailed);

        const std::vector<std::uint8_t> bytes{
            1};

        assert(
            failedStorage.write(bytes) ==
            ByteStorageError::WriteFailed);
    }

    void test_read_returns_exact_bytes_and_reports_failure()
    {
        resetFakeFileSystem();

        LittleFsByteStorage storage =
            makeStorage();

        assert(storage.begin());

        const std::vector<std::uint8_t>
            expectedBytes{
                0,
                127,
                255};

        files["/agenda.bin"] =
            expectedBytes;

        const ByteReadResult result =
            storage.read();

        assert(
            result.error ==
            ByteStorageError::None);

        assert(result.bytes == expectedBytes);

        readSucceeds = false;

        const ByteReadResult failed =
            storage.read();

        assert(
            failed.error ==
            ByteStorageError::ReadFailed);

        assert(failed.bytes.empty());
    }

    void test_write_replaces_file_without_losing_old_data_on_failure()
    {
        resetFakeFileSystem();

        LittleFsByteStorage storage =
            makeStorage();

        assert(storage.begin());

        const std::vector<std::uint8_t>
            originalBytes{
                1,
                2};

        const std::vector<std::uint8_t>
            replacementBytes{
                9,
                8,
                7};

        files["/agenda.bin"] =
            originalBytes;

        assert(
            storage.write(replacementBytes) ==
            ByteStorageError::None);

        assert(
            files.at("/agenda.bin") ==
            replacementBytes);

        assert(
            files.find("/agenda.bin.tmp") ==
            files.end());

        const std::vector<std::uint8_t>
            preservedBytes{
                4,
                5};

        files["/agenda.bin"] =
            preservedBytes;

        renameSucceeds = false;

        assert(
            storage.write(replacementBytes) ==
            ByteStorageError::WriteFailed);

        assert(
            files.at("/agenda.bin") ==
            preservedBytes);

        assert(
            files.find("/agenda.bin.tmp") ==
            files.end());

        renameSucceeds = true;
        writeSucceeds = false;

        assert(
            storage.write(replacementBytes) ==
            ByteStorageError::WriteFailed);

        assert(
            files.at("/agenda.bin") ==
            preservedBytes);
    }

    void test_production_constructor_accepts_an_explicit_path()
    {
        static_assert(
            std::is_constructible_v<
                LittleFsByteStorage,
                std::string>);

        LittleFsByteStorage primary{
            std::string{"/agenda.bin"}};

        LittleFsByteStorage backup{
            std::string{"/agenda.backup.bin"}};

        (void)primary;
        (void)backup;
    }

    void test_primary_and_backup_paths_store_independent_bytes()
    {
        resetFakeFileSystem();

        LittleFsByteStorage primary{
            "/agenda.bin",
            makeFakeApi()};

        LittleFsByteStorage backup{
            "/agenda.backup.bin",
            makeFakeApi()};

        assert(primary.begin());
        assert(backup.begin());

        assert(
            primary.write({1, 2, 3}) ==
            ByteStorageError::None);

        assert(
            backup.write({7, 8}) ==
            ByteStorageError::None);

        assert((files.at("/agenda.bin") ==
                std::vector<std::uint8_t>{1, 2, 3}));

        assert((files.at("/agenda.backup.bin") ==
                std::vector<std::uint8_t>{7, 8}));
    }
}

int main()
{

    test_production_constructor_accepts_an_explicit_path();
    test_primary_and_backup_paths_store_independent_bytes();
    test_mount_never_formats_and_handles_missing_file();
    test_read_returns_exact_bytes_and_reports_failure();
    test_write_replaces_file_without_losing_old_data_on_failure();

    std::cout
        << "All LittleFS byte storage tests passed.\n";

    return 0;
}