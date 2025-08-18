#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>
#include <string>
#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <array>
#include <fstream>
#include <ctime>
#include <chrono>
#include <iomanip>
class Logger {
public:
    static void logInfoImpl(const std::string &message, const char* file, const char* function, int line) {
        log("INFO", message, file, function, line);
    }

    static void logErrorImpl(const std::string &message, const char* file, const char* function, int line) {
        log("ERROR", message, file, function, line);
    }

    static void logWarnImpl(const std::string &message, const char* file, const char* function, int line) {
        log("WARN", message, file, function, line);
    }

private:
    static void log(const std::string &level, const std::string &message, const char* file, const char* function, int line) {
        std::ofstream logFile("/home/x_user/my_camera_project/FOLOG.log", std::ios::app);
        if (logFile.is_open()) {
            auto now = std::chrono::system_clock::now();
            auto seconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - seconds).count();
            std::time_t now_t = std::chrono::system_clock::to_time_t(seconds);
            std::tm* local_tm = std::localtime(&now_t);
            char buf[100];
            std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", local_tm);
            std::ostringstream oss;
            oss << "[" << buf << "." << std::setfill('0') << std::setw(3) << ms << "] [" << level << "] "
                << "[" << file << ":" << function << ":" << line << "] "
                << message << std::endl;
            logFile << oss.str();
        }
    }
};

#define LOG_INFO(msg) Logger::logInfoImpl(msg, __FILE__, __FUNCTION__, __LINE__)
#define LOG_ERROR(msg) Logger::logErrorImpl(msg, __FILE__, __FUNCTION__, __LINE__)
#define LOG_WARN(msg) Logger::logWarnImpl(msg, __FILE__, __FUNCTION__, __LINE__)

#endif // LOGGER_H

