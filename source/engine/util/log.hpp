
#pragma once

#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <mutex>
#include <string>

#ifdef __3DS__
#include <3ds.h>
#include <sys/stat.h>
#else
#include <filesystem>
#endif

namespace engine::util {

class Log {
public:
    enum class Level {
        Debug,
        Info,
        Warning,
        Error
    };

    static Log& get() {
        static Log instance;
        return instance;
    }

    Log(const Log&) = delete;
    Log& operator=(const Log&) = delete;

    bool init(const std::string& path = defaultPath()) {
        std::lock_guard<std::mutex> lock(mutex);

        closeUnlocked();

#ifdef __3DS__
        mkdir("sdmc:/Funkin3DS", 0777);
#else
        const std::filesystem::path parent =
            std::filesystem::path(path).parent_path();

        if (!parent.empty()) {
            std::error_code ec;
            std::filesystem::create_directories(parent, ec);

            if (ec)
                return false;
        }
#endif

        file = std::fopen(path.c_str(), "a");

        if (!file)
            return false;

        logUnlocked(Level::Info, "Logger initialized");
        return true;
    }

    void close() {
        std::lock_guard<std::mutex> lock(mutex);
        closeUnlocked();
    }

    void write(Level level, const char* format, ...) {
        std::lock_guard<std::mutex> lock(mutex);

        va_list args;
        va_start(args, format);
        writeUnlocked(level, format, args);
        va_end(args);
    }

    void debug(const char* format, ...) {
        log(Level::Debug, format);
    }

    void info(const char* format, ...) {
        log(Level::Info, format);
    }

    void warning(const char* format, ...) {
        log(Level::Warning, format);
    }

    void error(const char* format, ...) {
        log(Level::Error, format);
    }

    static std::string defaultPath() {
#ifdef __3DS__
        return "sdmc:/Funkin3DS/log.txt";
#else
        return "log.txt";
#endif
    }

private:
    FILE* file = nullptr;
    std::mutex mutex;

    Log() = default;

    static const char* levelName(Level level) {
        switch (level) {
            case Level::Debug:   return "DEBUG";
            case Level::Info:    return "INFO";
            case Level::Warning: return "WARNING";
            case Level::Error:   return "ERROR";
        }

        return "UNKNOWN";
    }

    void log(Level level, const char* format, ...) {
        std::lock_guard<std::mutex> lock(mutex);

        va_list args;
        va_start(args, format);
        writeUnlocked(level, format, args);
        va_end(args);
    }

    void writeUnlocked(Level level, const char* format, va_list args) {
        if (!file)
            return;

        std::time_t now = std::time(nullptr);
        std::tm* local = std::localtime(&now);

        if (local) {
            char timestamp[32];
            std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", local);

            std::fprintf(file, "[%s] [%s] ", timestamp, levelName(level));
        } else {
            std::fprintf(file, "[UNKNOWN TIME] [%s] ", levelName(level));
        }

        std::vfprintf(file, format, args);
        std::fputc('\n', file);
        std::fflush(file);
    }

    void logUnlocked(Level level, const char* message) {
        if (!file)
            return;

        std::fprintf(file, "[%s] %s\n", levelName(level), message);
        std::fflush(file);
    }

    void closeUnlocked() {
        if (file) {
            std::fflush(file);
            std::fclose(file);
            file = nullptr;
        }
    }
};

}

#define LOG_DEBUG(...) ::engine::util::Log::get().write(::engine::util::Log::Level::Debug, __VA_ARGS__)
#define LOG_INFO(...) ::engine::util::Log::get().write(::engine::util::Log::Level::Info, __VA_ARGS__)
#define LOG_WARNING(...) ::engine::util::Log::get().write(::engine::util::Log::Level::Warning, __VA_ARGS__)
#define LOG_ERROR(...) ::engine::util::Log::get().write(::engine::util::Log::Level::Error, __VA_ARGS__)
