// imu_classifier_thread.h
#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>
#include <string>
#include <iostream>
#include <functional>
#include <onnxruntime_cxx_api.h>
#include <optional>
#include "Configuration.h"
#include "Timer.h"
// Linux I2C stuff
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

class IMUClassifierThread {

public:
    IMUClassifierThread(const IMUConfig& imu_config)
        :imu_config_(imu_config), env_(ORT_LOGGING_LEVEL_WARNING, "IMUClassifier"), session(nullptr)  {
            LOG_INFO("IMUClassifierThread Constructor");
        }
    
    int init() {
        Ort::SessionOptions session_options;
        try {
            session = Ort::Session(env_, imu_config_.imu_model_path.c_str(), session_options);
        } catch (const Ort::Exception& e) {
            LOG_ERROR("ONNX Runtime error: " + std::string(e.what()));
            return 1;
        }

        // Get input and output names from the model
        Ort::AllocatorWithDefaultOptions allocator;
        input_name = session.GetInputNameAllocated(0, allocator).get();
        output_name = session.GetOutputNameAllocated(0, allocator).get();
        // Check output type
        Ort::TypeInfo output_type_info = session.GetOutputTypeInfo(0);
        auto output_tensor_info = output_type_info.GetTensorTypeAndShapeInfo();
        output_type_ = output_tensor_info.GetElementType();

        LOG_INFO("Model input name: " + input_name);
        LOG_INFO("Model output name: " + output_name);
        LOG_INFO("Output type: " + std::to_string(output_type_));

        fd = open(imu_config_.i2c_device.c_str(), O_RDWR);
        if (fd < 0) {            
            LOG_ERROR("Can't open I2C device");
            return 1;
        }
        if (ioctl(fd, I2C_SLAVE, imu_config_.i2c_addr) < 0) {
            LOG_ERROR("I2C ioctl failed");
            close(fd);
            return 1;
        }
        return initialize_sensor(fd);
    }

    void start_IMU(int period_ms) {
        timer.start(period_ms, 0, [this]() { 
            std::thread([this]() { ContinuousReadOnce(); }).detach(); 
        });
    }

    void stop() {
        timer.stop();
        {
            std::lock_guard<std::mutex> lock(window_mutex_);
            window_.clear();
        }
    }

    void setResultCallback(std::function<void(const QString)> callback) {
        result_callback = callback;
    } 

private:
    IMUConfig imu_config_;
    Ort::Env env_; 
    Ort::Session session;
    std::vector<float> features_;
    std::mutex features_mutex_;
    Timer timer;
    std::function<void(const QString)> result_callback;
    std::string input_name;  
    std::string output_name; 
    std::deque<std::array<float, 3>> window_;
    ONNXTensorElementDataType output_type_;
    std::mutex window_mutex_;
    std::condition_variable window_cv_;
    static constexpr size_t WINDOW_SIZE = 180;
    bool ready = false;
    int fd = -1;

    int read_reg(int fd, int reg) {
        uint8_t buf[1] = {static_cast<uint8_t>(reg)};
        if (write(fd, buf, 1) != 1) return -1;
        if (read(fd, buf, 1) != 1) return -1;
        return buf[0];
    }

    void write_reg(int fd, int reg, int value) {
        uint8_t buf[2] = {static_cast<uint8_t>(reg), static_cast<uint8_t>(value)};
        write(fd, buf, 2);
    }

    int initialize_sensor(int fd) {
        if (read_reg(fd, imu_config_.WHO_AM_I) != 0x44) {
            LOG_ERROR("Device not LIS2DW12TR!");
            return 1;
        }
        write_reg(fd, imu_config_.CTRL1, imu_config_.ON_CTRL1);
        return 0;
    }

    void read_accel(int fd, int16_t &x, int16_t &y, int16_t &z) {
        auto read_axis = [&](int l, int h) {
            uint16_t raw = (static_cast<uint16_t>(read_reg(fd, h)) << 8) | static_cast<uint16_t>(read_reg(fd, l));
            return static_cast<int16_t>(raw);
        };
        x = read_axis(imu_config_.OUT_X_L, imu_config_.OUT_X_H);
        y = read_axis(imu_config_.OUT_Y_L, imu_config_.OUT_Y_H);
        z = read_axis(imu_config_.OUT_Z_L, imu_config_.OUT_Z_H);
    }

    void ContinuousRead() {
        while (!ready) {
            int16_t x, y, z;
            read_accel(fd, x, y, z);            
            {
                std::lock_guard<std::mutex> lock(window_mutex_);
                window_.push_back({static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)});
                if (window_.size() > WINDOW_SIZE) {  // Keep some buffer
                    window_.erase(window_.begin(), window_.begin() + (window_.size() - WINDOW_SIZE));
                    ready = true;
                }
            } 
            if (ready) {
                CaptureIMU();
            }           
            usleep(50);
        }
    }

    void ContinuousReadOnce() {
        std::deque<std::array<float, 3>> temp_window;
        for (size_t i = 0; i < WINDOW_SIZE; ++i) {
            int16_t x, y, z;
            read_accel(fd, x, y, z);
            temp_window.push_back({static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)});
            usleep(20000); // 20 ms between samples (~50 Hz)
        }
        // Copy to member window_ for processing (if needed by CaptureIMU)
        {
            std::lock_guard<std::mutex> lock(window_mutex_);
            window_ = temp_window;
        }
        ready = true;
        CaptureIMU();
    }

    void CaptureIMU() {
        try {
            if (ready) {
                // 1. Read new sample
                std::deque<std::array<float, 3>> current_window;        
                
                // Lock and copy the current window
                {
                    std::unique_lock<std::mutex> lock(window_mutex_);
                    if (window_.size() < WINDOW_SIZE) {
                        LOG_INFO("Not enough samples yet: " + std::to_string(window_.size()));
                        return;
                    }
                    current_window.assign(window_.end() - WINDOW_SIZE, window_.end());
                }                      
                // 2. Compute features on current window_
                std::vector<float> feature_vec = compute_features(current_window);
                if (feature_vec.size() != 18) {
                    LOG_ERROR("Unexpected feature vector size");
                    return;
                }

                Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
                std::array<int64_t, 2> input_shape{1, (int64_t)feature_vec.size()};
                
                // Create input tensor
                Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
                    memory_info, feature_vec.data(), feature_vec.size(),
                    input_shape.data(), input_shape.size()
                );
                
                // Prepare input/output names
                std::vector<const char*> input_names{input_name.c_str()};
                std::vector<const char*> output_names{output_name.c_str()};
                
                // Run inference
                auto output_tensors = session.Run(
                    Ort::RunOptions{nullptr},
                    input_names.data(), &input_tensor, 1,
                    output_names.data(), 1
                );
                
                // Process output based on type
                int label = -1;
                float confidence = 0.0f;
                
                if (output_tensors.size() > 0) {
                    if (output_type_ == ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64) {
                        int64_t* output = output_tensors[0].GetTensorMutableData<int64_t>();
                        label = static_cast<int>(output[0]) - 1; // Match Python's -1 adjustment
                        confidence = 1.0f; // No confidence score for integer outputs
                    } 
                    else if (output_type_ == ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT) {
                        float* output = output_tensors[0].GetTensorMutableData<float>();
                        int max_idx = std::distance(output, std::max_element(output, output + 3));
                        label = max_idx;
                        confidence = output[max_idx];
                        LOG_INFO(std::to_string(confidence));        
                    }
                    
                    QString activity;
                    if (label == 0) activity = "Work";
                    else if (label == 1) activity = "Relax";
                    else if (label == 2) activity = "Fall";
                    else activity = "Unknown";
                    if (result_callback) {
                        result_callback(activity);
                    }
                    
                    window_.clear();
                    ready = false;
                }
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Error in CaptureIMU: " + std::string(e.what()));
        }
    }

    std::vector<float> compute_features(const std::deque<std::array<float, 3>>& win) {
        auto mean = [](const std::vector<float>& v) {
            return std::accumulate(v.begin(), v.end(), 0.0f) / v.size();
        };
        auto stddev = [&](const std::vector<float>& v, float m) {
            float s = 0.0f; for (auto x : v) s += (x-m)*(x-m);
            return std::sqrt(s/v.size());
        };
        auto var = [&](const std::vector<float>& v, float m) {
            float s = 0.0f; for (auto x : v) s += (x-m)*(x-m);
            return s/v.size();
        };
        auto ener = [](const std::vector<float>& v) {
            float s = 0.0f; for (auto x : v) s += x*x; return s;
        };
        auto pearson = [](const std::vector<float>& a, const std::vector<float>& b) {
            if (a.size() != b.size()) return std::make_pair(0.0f, 0.0f);
            
            float sum_a = std::accumulate(a.begin(), a.end(), 0.0f);
            float sum_b = std::accumulate(b.begin(), b.end(), 0.0f);
            float mean_a = sum_a / a.size();
            float mean_b = sum_b / b.size();
            
            float cov = 0.0f, var_a = 0.0f, var_b = 0.0f;
            for (size_t i = 0; i < a.size(); ++i) {
                float da = a[i] - mean_a;
                float db = b[i] - mean_b;
                cov += da * db;
                var_a += da * da;
                var_b += db * db;
            }
            
            float denom = std::sqrt(var_a * var_b);
            float r = (denom != 0) ? cov / denom : 0.0f;
            return std::make_pair(r, 0.0f); // p-value not calculated
        };        
        std::vector<float> x, y, z;
        for (const auto& arr : win) {
            x.push_back(arr[0]);
            y.push_back(arr[1]);
            z.push_back(arr[2]);
        }
        float x_mean = mean(x), y_mean = mean(y), z_mean = mean(z);
        float xstd = stddev(x, x_mean), ystd = stddev(y, y_mean), zstd = stddev(z, z_mean);
        float xvar = var(x, x_mean), yvar = var(y, y_mean), zvar = var(z, z_mean);
        float xner = ener(x), yner = ener(y), zner = ener(z);
        auto [xcor_r, xcor_p] = pearson(x, y);
        auto [ycor_r, ycor_p] = pearson(y, z);
        auto [zcor_r, zcor_p] = pearson(z, x);
        // std::vector<float> feats(18, 0.0f);
        // p-values not calculated here
        std::vector<float> feats = {x_mean, y_mean, z_mean, xstd, ystd, zstd, xvar, yvar, zvar,
                    xner, yner, zner, xcor_r, ycor_r, zcor_r, xcor_p, ycor_p, zcor_p};
        // Fill feats[...] using window data
        return feats;
    }
};
