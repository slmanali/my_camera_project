#ifndef GPIO_H
#define GPIO_H

#include <iostream>
#include <thread>
#include <chrono>
#include <gpiod.h> // Include the gpiod library for GPIO handling
#include <atomic>
#include "Logger.h"
#include <functional>

class GPIOThread {
public:
    GPIOThread(const std::string &chip_name, unsigned int line_number)
        : chip_name(chip_name), line_number(line_number), stop_thread(false) {
        try {
            chip = gpiod_chip_open_by_name(chip_name.c_str());
            if (!chip) {
                LOG_ERROR("Failed to open GPIO chip:  " + chip_name);
                throw std::runtime_error("Failed to open GPIO chip: " + chip_name);
            }

            line = gpiod_chip_get_line(chip, line_number);
            if (!line) {
                gpiod_chip_close(chip);
                LOG_ERROR("Failed to get GPIO chip line:  "  + std::to_string(line_number));
                throw std::runtime_error("Failed to get GPIO line: " + std::to_string(line_number));
            }

            int ret = gpiod_line_request_input(line, "GPIOThread");
            if (ret < 0) {
                gpiod_chip_close(chip);
                LOG_ERROR("Failed to request GPIO chip line as input");
                throw std::runtime_error("Failed to request GPIO line as input");
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong in GPIOThread Constructor: " + std::string(e.what()));      
        }
    };

    void start() {
        try {
            thread = std::thread(&GPIOThread::run, this);
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong in GPIOThread start: " + std::string(e.what()));      
        }
    };

    void stop() {
        try {
            stop_thread = true;
            if (thread.joinable()) {
                thread.join();
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong in GPIOThread stop: " + std::string(e.what()));      
        }
    };

    void set_button_pressed_callback(const std::function<void()> &callback) {
        button_pressed_callback = callback;
    }

    ~GPIOThread() {
        try {
            stop();
            if (line) {
                gpiod_line_release(line);
            }
            if (chip) {
                gpiod_chip_close(chip);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong in GPIOThread destructor: " + std::string(e.what()));      
        }
    };

private:
    std::string chip_name;
    unsigned int line_number;
    struct gpiod_chip *chip;
    struct gpiod_line *line;
    std::thread thread;
    std::atomic<bool> stop_thread;
    std::function<void()> button_pressed_callback;

    void run() {
        try {
            int last_value = gpiod_line_get_value(line);
            while (!stop_thread) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Polling interval
                int value = gpiod_line_get_value(line);
                if (value < 0) {
                    LOG_ERROR("Failed to read GPIO value");
                    continue;
                }
                if (last_value != value && value == 0) { // Assuming active low button
                    if (button_pressed_callback) {
                        button_pressed_callback();
                    }
                }
                last_value = value;
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong in GPIOThread runner: " + std::string(e.what()));      
        }
    };
};
#endif // GPIO_H
