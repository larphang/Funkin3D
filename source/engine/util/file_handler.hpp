
#pragma once

#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <utility>
#include <algorithm>

#ifdef __3DS__
#include <3ds.h>
#include <sys/stat.h>
#include <dirent.h>
#include <cerrno>
#else
#include <filesystem>
#include <fstream>
#endif

#include <unistd.h>

namespace engine::io {

class FileHandler {
public:
    static bool initialize() {
#ifdef __3DS__
        return R_SUCCEEDED(romfsInit());
#else
        return true;
#endif
    }

    static void shutdown() {
#ifdef __3DS__
        romfsExit();
#endif
    }

    static std::string resolveReadPath(std::string_view path) {
        std::string p(path);

#ifdef __3DS__
        if (p.rfind("romfs:/", 0) == 0 || p.rfind("sdmc:/", 0) == 0)
            return p;

        return "romfs:/" + p;
#else
        return p;
#endif
    }

    static std::string resolveWritePath(std::string_view path) {
        std::string p(path);

#ifdef __3DS__
        if (p.rfind("sdmc:/", 0) == 0)
            return p;

        if (p.rfind("romfs:/", 0) == 0)
            return {};

        return "sdmc:/" + p;
#else
        return p;
#endif
    }

    static bool exists(std::string_view path) {
        const auto p = resolveReadPath(path);
#ifdef __3DS__
        struct stat info {};
        return stat(p.c_str(), &info) == 0;
#else
        std::error_code ec;
        return std::filesystem::exists(p, ec) && !ec;
#endif
    }

    static bool isFile(std::string_view path) {
        const auto p = resolveReadPath(path);
#ifdef __3DS__
        struct stat info {};
        return stat(p.c_str(), &info) == 0 && S_ISREG(info.st_mode);
#else
        std::error_code ec;
        return std::filesystem::is_regular_file(p, ec) && !ec;
#endif
    }

    static bool isDirectory(std::string_view path) {
        const auto p = resolveReadPath(path);
#ifdef __3DS__
        struct stat info {};
        return stat(p.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
#else
        std::error_code ec;
        return std::filesystem::is_directory(p, ec) && !ec;
#endif
    }

    static std::string extension(std::string_view path) {
        const auto slash = path.find_last_of("/\\");
        const auto dot = path.find_last_of('.');

        if (dot == std::string_view::npos || (slash != std::string_view::npos && dot < slash))
            return {};

        return std::string(path.substr(dot));
    }

    static std::string filename(std::string_view path) {
        const auto slash = path.find_last_of("/\\");
        return std::string(slash == std::string_view::npos ? path : path.substr(slash + 1));
    }

    static std::string stem(std::string_view path) {
        auto name = filename(path);
        const auto dot = name.find_last_of('.');

        if (dot != std::string::npos && dot != 0)
            name.resize(dot);

        return name;
    }

    static std::string parentPath(std::string_view path) {
        const auto slash = path.find_last_of("/\\");

        if (slash == std::string_view::npos)
            return {};

        return std::string(path.substr(0, slash));
    }

    static bool createDirectories(std::string_view path) {
        const auto p = resolveWritePath(path);
        if (p.empty())
            return false;

#ifdef __3DS__
        std::string current;
        std::size_t start = 0;

        const auto colon = p.find(':');
        if (colon != std::string::npos) {
            current = p.substr(0, colon + 1);
            start = colon + 1;

            if (start < p.size() && p[start] == '/') {
                current += '/';
                ++start;
            }
        }

        while (start < p.size()) {
            const auto end = p.find('/', start);
            const auto part = p.substr(start, end == std::string::npos ? std::string::npos : end - start);

            if (!part.empty()) {
                if (!current.empty() && current.back() != '/')
                    current += '/';

                current += part;

                struct stat info {};
                if (stat(current.c_str(), &info) != 0) {
                    if (mkdir(current.c_str(), 0777) != 0 && errno != EEXIST)
                        return false;
                } else if (!S_ISDIR(info.st_mode)) {
                    return false;
                }
            }

            if (end == std::string::npos)
                break;

            start = end + 1;
        }

        return true;
#else
        std::error_code ec;
        return std::filesystem::create_directories(p, ec) || (!ec && std::filesystem::is_directory(p, ec));
#endif
    }

    static bool removeFile(std::string_view path) {
        const auto p = resolveWritePath(path);
        if (p.empty())
            return false;

#ifdef __3DS__
        struct stat info {};
        if (stat(p.c_str(), &info) != 0 || !S_ISREG(info.st_mode))
            return false;

        return std::remove(p.c_str()) == 0;
#else
        std::error_code ec;
        if (!std::filesystem::is_regular_file(p, ec) || ec)
            return false;

        return std::filesystem::remove(p, ec) && !ec;
#endif
    }

    static bool remove(std::string_view path) {
        const auto p = resolveWritePath(path);
        if (p.empty())
            return false;

#ifdef __3DS__
        struct stat info {};
        if (stat(p.c_str(), &info) != 0)
            return false;

        return S_ISDIR(info.st_mode) ? ::rmdir(p.c_str()) == 0 : std::remove(p.c_str()) == 0;
#else
        std::error_code ec;
        return std::filesystem::remove(p, ec) && !ec;
#endif
    }

    static bool rename(std::string_view from, std::string_view to) {
        const auto src = resolveWritePath(from);
        const auto dst = resolveWritePath(to);

        if (src.empty() || dst.empty())
            return false;

        return std::rename(src.c_str(), dst.c_str()) == 0;
    }

    static bool copyFile(std::string_view from, std::string_view to, bool overwrite = false) {
        std::vector<std::uint8_t> data;

        if (!readBytes(from, data))
            return false;

        const auto dst = resolveWritePath(to);
        if (dst.empty())
            return false;

        if (!overwrite && exists(to))
            return false;

        return writeBytes(to, data);
    }

    static bool readText(std::string_view path, std::string& output) {
        const auto p = resolveReadPath(path);
        FILE* file = std::fopen(p.c_str(), "rb");

        if (!file)
            return false;

        if (std::fseek(file, 0, SEEK_END) != 0) {
            std::fclose(file);
            return false;
        }

        const long length = std::ftell(file);
        if (length < 0 ||
            std::fseek(file, 0, SEEK_SET) != 0) {
            std::fclose(file);
            return false;
        }

        std::string result(static_cast<std::size_t>(length), '\0');

        const auto read = result.empty() ? std::size_t(0) : std::fread(result.data(), 1, result.size(), file);

        const bool ok = read == result.size() && std::ferror(file) == 0;

        std::fclose(file);

        if (!ok)
            return false;

        output = std::move(result);
        return true;
    }

    static bool writeText(std::string_view path, std::string_view contents, bool append = false) {
        const auto p = resolveWritePath(path);
        if (p.empty())
            return false;

        FILE* file = std::fopen(p.c_str(), append ? "ab" : "wb");

        if (!file)
            return false;

        const auto written = contents.empty() ? std::size_t(0) : std::fwrite(contents.data(), 1, contents.size(), file);

        const bool ok = written == contents.size() && std::fclose(file) == 0;

        return ok;
    }

    static bool readBytes(std::string_view path, std::vector<std::uint8_t>& output) {
        std::string data;

        if (!readText(path, data))
            return false;

        std::vector<std::uint8_t> result(data.begin(), data.end());

        output = std::move(result);
        return true;
    }

    static bool writeBytes(std::string_view path, const std::vector<std::uint8_t>& data) {
        const auto p = resolveWritePath(path);
        if (p.empty())
            return false;

        FILE* file = std::fopen(p.c_str(), "wb");
        if (!file)
            return false;

        const auto written = data.empty() ? std::size_t(0) : std::fwrite(data.data(), 1, data.size(), file);

        const bool ok = written == data.size() && std::fclose(file) == 0;

        return ok;
    }

    static std::vector<std::string> listFiles(std::string_view path, bool recursive = false) {
        std::vector<std::string> result; 
        const auto p = resolveReadPath(path);

#ifdef __3DS__
        DIR* dir = opendir(p.c_str());
        if (!dir)
            return result;

        while (auto* entry = readdir(dir)) {
            const std::string name = entry->d_name;

            if (name == "." || name == "..")
                continue;

            const std::string full = p + (p.back() == '/' ? "" : "/") + name;

            struct stat info {};
            if (stat(full.c_str(), &info) != 0)
                continue;

            if (S_ISREG(info.st_mode))
                result.push_back(full);
            else if (recursive && S_ISDIR(info.st_mode)) {
                auto nested = listFiles(full, true);
                result.insert(result.end(), nested.begin(), nested.end());
            }
        }

        closedir(dir);
#else
        std::error_code ec;

        if (recursive) {
            std::filesystem::recursive_directory_iterator it(p, std::filesystem::directory_options::skip_permission_denied, ec);
            std::filesystem::recursive_directory_iterator end;

            for (; !ec && it != end; it.increment(ec)) {
                std::error_code itemEc;
                if (it->is_regular_file(itemEc) && !itemEc)
                    result.push_back(it->path().string());
            }
        } else {
            std::filesystem::directory_iterator it(p, ec);
            std::filesystem::directory_iterator end;

            for (; !ec && it != end; it.increment(ec)) {
                std::error_code itemEc;
                if (it->is_regular_file(itemEc) && !itemEc)
                    result.push_back(it->path().string());
            }
        }
#endif

        return result;
    }

    static std::vector<std::string> listDirectories(
        std::string_view path) {
        std::vector<std::string> result;
        const auto p = resolveReadPath(path);

#ifdef __3DS__
        DIR* dir = opendir(p.c_str());
        if (!dir)
            return result;

        while (auto* entry = readdir(dir)) {
            const std::string name = entry->d_name;

            if (name == "." || name == "..")
                continue;

            const std::string full = p + (p.back() == '/' ? "" : "/") + name;

            struct stat info {};
            if (stat(full.c_str(), &info) == 0 && S_ISDIR(info.st_mode))
                result.push_back(full);
        }

        closedir(dir);
#else
        std::error_code ec;
        std::filesystem::directory_iterator it(p, ec);
        std::filesystem::directory_iterator end;

        for (; !ec && it != end; it.increment(ec)) {
            std::error_code itemEc;
            if (it->is_directory(itemEc) && !itemEc)
                result.push_back(it->path().string());
        }
#endif

        return result;
    }
};

} // namespace engine::io
