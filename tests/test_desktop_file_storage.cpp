#include <cassert>
#include <filesystem>
#include <iostream>
#include <cstdint>
#include <vector>

#include "platform/desktop_file_storage.h"

void test_missing_file_reports_not_found()
{
    const std::filesystem::path path =
        std::filesystem::path("build") /
        "desktop_storage_tests" /
        "missing.bin";

    std::error_code error;
    std::filesystem::remove_all(
        path.parent_path(),
        error
    );

    DesktopFileStorage storage(path);

    const ByteReadResult result =
        storage.read();

    assert(
        result.error ==
        ByteStorageError::NotFound
    );

    assert(result.bytes.empty());
}

void test_writes_and_reads_binary_bytes()
{
    const std::filesystem::path path =
        std::filesystem::path("build") /
        "desktop_storage_tests" /
        "nested" /
        "agenda.bin";

    std::error_code error;

    std::filesystem::remove_all(
        std::filesystem::path("build") /
        "desktop_storage_tests",
        error
    );

    DesktopFileStorage storage(path);

    const std::vector<std::uint8_t> expected{
        0x00,
        0x12,
        0xFF
    };

    assert(
        storage.write(expected) ==
        ByteStorageError::None
    );

    const ByteReadResult result =
        storage.read();

    assert(
        result.error ==
        ByteStorageError::None
    );

    assert(result.bytes == expected);
    assert(std::filesystem::exists(path));
}

void test_directory_cannot_be_read_as_file()
{
    const std::filesystem::path path =
        std::filesystem::path("build") /
        "desktop_storage_tests" /
        "directory";

    std::error_code error;

    std::filesystem::remove_all(
        std::filesystem::path("build") /
        "desktop_storage_tests",
        error
    );

    std::filesystem::create_directories(path);

    DesktopFileStorage storage(path);

    const ByteReadResult result =
        storage.read();

    assert(
        result.error ==
        ByteStorageError::ReadFailed
    );

    assert(result.bytes.empty());
}

void test_directory_cannot_be_written_as_file()
{
    const std::filesystem::path path =
        std::filesystem::path("build") /
        "desktop_storage_tests" /
        "directory";

    std::error_code error;

    std::filesystem::remove_all(
        std::filesystem::path("build") /
        "desktop_storage_tests",
        error
    );

    std::filesystem::create_directories(path);

    DesktopFileStorage storage(path);

    assert(
        storage.write({1}) ==
        ByteStorageError::WriteFailed
    );
}

void test_new_write_replaces_old_contents()
{
    const std::filesystem::path path =
        std::filesystem::path("build") /
        "desktop_storage_tests" /
        "agenda.bin";

    std::error_code error;

    std::filesystem::remove_all(
        std::filesystem::path("build") /
        "desktop_storage_tests",
        error
    );

    DesktopFileStorage storage(path);

    assert(
        storage.write({1, 2, 3, 4}) ==
        ByteStorageError::None
    );

    assert(
        storage.write({9}) ==
        ByteStorageError::None
    );

    const ByteReadResult result =
        storage.read();

    assert(
        result.error ==
        ByteStorageError::None
    );

    assert(
        result.bytes ==
        std::vector<std::uint8_t>{9}
    );
}

void test_empty_content_creates_empty_file()
{
    const std::filesystem::path path =
        std::filesystem::path("build") /
        "desktop_storage_tests" /
        "empty.bin";

    std::error_code error;

    std::filesystem::remove_all(
        std::filesystem::path("build") /
        "desktop_storage_tests",
        error
    );

    DesktopFileStorage storage(path);

    assert(
        storage.write({}) ==
        ByteStorageError::None
    );

    const ByteReadResult result =
        storage.read();

    assert(
        result.error ==
        ByteStorageError::None
    );

    assert(result.bytes.empty());
    assert(std::filesystem::exists(path));
}

int main()
{
    test_missing_file_reports_not_found();
    test_writes_and_reads_binary_bytes();
    test_directory_cannot_be_read_as_file();
    test_directory_cannot_be_written_as_file();
    test_new_write_replaces_old_contents();
    test_empty_content_creates_empty_file();

    std::cout
        << "All desktop file storage tests passed.\n";

    return 0;
}