#pragma once

#include <mutex>
#include <string>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cstdlib>
#include <string>
#include "Env.h"

namespace radiyx {

class Log
{
    public:
        static Log& Instance()
        {
            static Log instance;
            return instance;
        }

        void Push(const std::string& line)
        {
            std::lock_guard<std::mutex> lock(mutex);

            auto now = std::chrono::system_clock::now();
            auto timeT = std::chrono::system_clock::to_time_t(now);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        now.time_since_epoch()) % 1000;

            std::ostringstream timestamp;
            timestamp << std::put_time(std::localtime(&timeT), "[%Y-%m-%d %H:%M:%S")
                    << "." << std::setw(3) << std::setfill('0') << ms.count() << "] ";

            std::string path = GetLogPath();
            if (!path.empty())
            {
                std::ofstream log(path, std::ios::app);
                if (log.is_open())
                    log << timestamp.str() << line << std::endl;
            }
        }

        void Reset()
        {
            std::lock_guard<std::mutex> lock(mutex);
            std::ofstream logFile(GetLogPath(), std::ios::trunc);
        }

    private:
        Log() = default;
        Log(const Log&) = delete;
        Log& operator=(const Log&) = delete;

        std::mutex mutex;

        std::string GetLogPath()
        {
            static bool dotenvLoaded = false;
            if (!dotenvLoaded) { Env::LoadDotEnv(); dotenvLoaded = true; }

            const char* p = std::getenv("LOG_PATH");
            if (p && *p) return std::string(p);

            // Fallback to a temporary log to retain diagnostic output.
            return "/tmp/vsthost.log";
        }
};

} // namespace radiyx
