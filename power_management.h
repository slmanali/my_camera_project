#ifndef POWER_MANAGEMENT_H
#define POWER_MANAGEMENT_H

#include <iostream>
#include <fstream>
#include <string>
#include <jsoncpp/json/json.h>
#include <chrono>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>  // For system() calls
#include <ctime>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cstdio>
#include <csignal>
#include <cmath>
#include <atomic>
#include <functional>
#include "Logger.h"
#include "Configuration.h"
#include "Timer.h"

class PowerManagement {
public:
    // Enum for Battery Status
    enum class BatteryStatus {
        RED,
        YELLOW,
        GREEN,
        CRITICAL
    };

    struct BatteryData {
        float level;
        float percentage;
    };
    // Constructor
    PowerManagement(Configuration& _config) 
        : config(_config), stop_thread(false), REPEAT_INIT(2), current_counter(1),
        last_log_time(std::chrono::steady_clock::now() - std::chrono::minutes(1)) {
            if (config.testbench == 0){
                LOG_INFO("PowerManagement Constructor");
                std::ifstream log_file(config.LOG_BOOK_FILE_PATH);
                if (log_file.is_open()) {
                    std::string line, last_line;
                    while (std::getline(log_file, line)) {
                        if (!line.empty()) last_line = line;
                    }
                    if (!last_line.empty()) {
                        std::istringstream iss(last_line);
                        std::string token;
                        if (std::getline(iss, token, ',')) {
                            try {
                                current_counter = std::stoi(token) + 1;
                            } catch (const std::exception& e) {
                                LOG_ERROR("Failed to parse counter: " + std::string(e.what()));
                            }
                        }
                    }
                    log_file.close();
                }
            }
    }

    // Destructor
    ~PowerManagement() {
        if (config.testbench == 0) {
            stop_updates();
        }
    }

    // Set a callback for battery status changes
    void set_battery_status_callback(const std::function<void(BatteryStatus)>& callback) {
        battery_status_callback = callback;
    }

    // Start periodic battery status updates
    void start_updates() {
        timer.start(config.READING_DELAY, 0, [this]() { check_battery_status(); });
    }

    // Stop periodic battery status updates
    void stop_updates() {
        timer.stop();
    }

private:
    // Configuration variables
    Configuration& config;
    std::atomic<bool> stop_thread;
    int REPEAT_INIT;
    int current_counter;
    std::chrono::steady_clock::time_point last_log_time;
    std::thread thread;
    Timer timer;
    std::function<void(BatteryStatus)> battery_status_callback;

    std::string battery_status_to_string(BatteryStatus status) const {
        switch (status) {
            case BatteryStatus::GREEN: return "GREEN";
            case BatteryStatus::YELLOW: return "YELLOW";
            case BatteryStatus::RED: return "RED";
            case BatteryStatus::CRITICAL: return "CRITICAL";
            default: return "CRITICAL";
        }
    }

    // Read ADC value
    int read_adc(bool smooth = true) {
        try {
            const char *i2cDevice = ("/dev/i2c-" + std::to_string(config.battery_i2c_channel)).c_str();
            int file = open(i2cDevice, O_RDWR);
            if (file < 0) {
                LOG_ERROR("Failed to open the I2C bus");
                return 1;
            }

            if (ioctl(file, I2C_SLAVE, config.battery_i2c_address) < 0) {
                LOG_ERROR("Failed to set I2C address");
                close(file);
                return 1;
            }

            uint8_t reg = 0x00;
            if (write(file, &reg, 1) != 1) {
                LOG_ERROR("Failed to set register address");
                close(file);
                return 1;
            }

            std::vector<uint8_t> buffer(32);
            if (read(file, buffer.data(), buffer.size()) != static_cast<int>(buffer.size())) {
                LOG_ERROR("Failed to read data from the device");
                return 1;
            }

            int reading = 0;
            if (!smooth) {
                reading = (static_cast<int>(buffer[0]) << 8) + static_cast<int>(buffer[1]);
            } else {
                for (int i = 0; i < 32; i += 2) {
                    reading += (static_cast<int>(buffer[i]) << 8) + static_cast<int>(buffer[i + 1]);
                }
                reading = static_cast<int>(2.0 * (reading / 32.0));
            }
            close(file);
            return reading;
        } catch (const std::exception& e) {
            LOG_ERROR("APPLICATION NOT RUNNING: " + std::string(e.what()));
            return 1;
        }

    }

    // Calculate battery level
    float calculate_battery_level(int level, float vdd, float rdf = 1.0, float gef = 1.0) {
        float resolution = vdd / 4096.0;
        return level * resolution * rdf * gef;
    }

    // Calculate battery percentage
    float calculate_battery_percentage(int level, float vdd, float rdf, float gef, float vbat_max, float vbat_min) {
        float resolution = vdd / 4096.0;
        float battery_volt = level * resolution * rdf * gef;
        if (battery_volt < vbat_min) {
            battery_volt = vbat_min;
        }
        if (battery_volt > vbat_max) {
            battery_volt = vbat_max;
        }

        float range = vbat_max - vbat_min;
        float relative_level = battery_volt - vbat_min;
        return (relative_level / range) * 100.0;
    }

    // Read battery level and percentage
    BatteryData read_battery(int repetitions = 1) {
        float rd_factor = 2.0;
        float max_vbat = 4.0;
        float min_vbat = 3.0;
        float bl = 0.0;
        float bp = 0.0;

        for (int i = 0; i < repetitions; ++i) {
            int data = read_adc();
            float battery_level = calculate_battery_level(data, config.VDD_VOLTAGE, rd_factor, config.GAIN_FACTOR);
            float battery_percentage = calculate_battery_percentage(data, config.VDD_VOLTAGE, rd_factor, config.GAIN_FACTOR, max_vbat, min_vbat);
            bl += battery_level;
            bp += battery_percentage;
        }

        float average_battery_level = bl / repetitions;
        float average_battery_percentage = bp / repetitions;

        return {average_battery_level, average_battery_percentage};
    }

    // Check battery status and emit updates
    void check_battery_status() {
        try {
            BatteryData battery_stats = read_battery();
            BatteryStatus new_status;
            if (battery_stats.level > config.voltage_hi) {
                new_status = BatteryStatus::GREEN;
            } else if (battery_stats.level > config.low_threshold) {
                new_status = BatteryStatus::YELLOW;
            } else if (battery_stats.level > config.critical_threshold) {
                new_status = BatteryStatus::RED;
            } else {
                new_status = BatteryStatus::CRITICAL;
            }

            auto now = std::chrono::steady_clock::now();
            if (now - last_log_time >= std::chrono::minutes(1)) {
                auto system_now = std::chrono::system_clock::now();
                std::time_t now_time = std::chrono::system_clock::to_time_t(system_now);
                std::tm* now_tm = std::localtime(&now_time);
                std::ostringstream oss;
                oss << std::put_time(now_tm, "%d/%m/%y %H:%M:%S");
                std::string dts = oss.str();

                std::ostringstream line_stream;
                line_stream << current_counter << ", "
                            << dts << ", "
                            << std::fixed << std::setprecision(2) << battery_stats.level << ", "
                            << static_cast<int>(battery_stats.percentage) << ", "
                            << battery_status_to_string(new_status) << ", "
                            << config.SAMPLING_INTERVAL << "\n";

                std::ofstream log_file(config.LOG_BOOK_FILE_PATH, std::ios::app);
                if (log_file.is_open()) {
                    log_file << line_stream.str();
                    log_file.close();
                } else {
                    LOG_ERROR("Failed to open log file: " + config.LOG_BOOK_FILE_PATH);
                }

                current_counter++;
                last_log_time = now;
            }
            // Emit status update if it has changed
            if (battery_status_callback) {
                battery_status_callback(new_status);
            }

            // Log battery information
            // LOG_INFO("Battery Level: " + std::to_string(battery_level) + ", Percentage: " + std::to_string(battery_percentage) + ", Status: " + std::to_string(static_cast<int>(new_status)));
        } catch (const std::exception& e) {
            LOG_ERROR("Error checking battery status: " + std::string(e.what()));
        }
    }

    // Shutdown the board
    void shutdown_board() {
        LOG_INFO("shutdown_board.");
        BatteryData battery_stats = read_battery(REPEAT_INIT);
        if (battery_stats.level < config.voltage_hi) {
            system("fbi -T 1 --noverbose /root/low-battery-1280x720.png");
            system("aplay /root/beep.wav");

            for (int i = 0; i < 10; ++i) {
                system("echo 0 > /sys/class/gpio/gpio124/value");
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                system("echo 1 > /sys/class/gpio/gpio124/value");
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }

            system("/sbin/shutdown -h now");
        }
    }
};

#endif // POWER_MANAGEMENT_H