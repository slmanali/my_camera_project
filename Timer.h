#ifndef Timer_H
#define Timer_H

#include <iostream>
#include <thread>
#include <functional>
#include <atomic>
#include <chrono>

class Timer {
public:
    Timer() : running(false) {}

    void start(int interval_ms, int _type, std::function<void()> callback) {
        try {
            stop();
            running = true;

            timer_thread = std::thread([=]() {
                while (running) {
                    if (_type == 0) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
                    }
                    else if (_type == 2){
                        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
                    }
                    else if (_type == 1){
                        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms/4));
                    }
                    if (running) {
                        callback();
                    }
                }
            });
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong in start Timer: " + std::string(e.what()));
        }
    }

    void stop() {
        try {
            running = false;
            if (timer_thread.joinable()) {
                timer_thread.join();
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong in stop Timer: " + std::string(e.what()));
        }
    }

    ~Timer() {
        stop();
    }

private:
    std::thread timer_thread;
    std::atomic<bool> running;
};

#endif // Timer_H