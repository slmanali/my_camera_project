#ifndef SPEECHThREAD_H
#define SPEECHThREAD_H

#include <iostream>
#include <atomic>
#include <functional>
#include <chrono>
#include <thread>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include "vosk_api.h"
#include "Logger.h"
#include <jsoncpp/json/json.h>

class speechThread {
public:
    speechThread(std::string model_path,  std::string grammar_json, std::string pipeline_description, int timeout_seconds = 3)
        : stop(true), model_path(model_path), grammar_json(grammar_json), pipeline_description(pipeline_description), timeout_seconds(timeout_seconds) { 
        try{       
            LOG_INFO("speechThread Constructor");
            initialize_vosk();
            initialize_gstreamer();
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong in speechThread Constructor: " + std::string(e.what()));    
        }

    }

    ~speechThread() {
        stopThread();
        cleanup();
    }

    void setCommandCallback(std::function<void(const std::string&)> callback) {
        command_callback = callback;
    }

    void start() {
        try {
            if (stop) {
                stop = false;
            gst_element_set_state(pipeline, GST_STATE_PLAYING);
            timeout_thread = std::thread(&speechThread::timeout_checker, this);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong while start speechThread: " + std::string(e.what()));   
        }
    }
    
    void stopThread() {
        stop = true;
        
        // Stop GStreamer pipeline first
        if (pipeline) {
        gst_element_set_state(pipeline, GST_STATE_NULL);
        }

        // Join timeout thread
        if (timeout_thread.joinable()) {
            timeout_thread.join();
        }
    }

    bool getstatus() {
        return stop;
    }


private:
    std::atomic<bool> stop;
    std::string model_path;
    std::string grammar_json;
    std::string pipeline_description;
    int timeout_seconds;
    std::chrono::time_point<std::chrono::system_clock> last_audio_time;
    std::thread timeout_thread;
    
    GstElement *pipeline = nullptr;
    GstElement *appsink = nullptr;
    VoskModel *model = nullptr;
    VoskRecognizer *rec = nullptr;
    std::function<void(const std::string&)> command_callback;
    std::atomic<bool> paused_;
    std::mutex cleanup_mutex;

    void initialize_vosk() {
        try{ 
            vosk_gpu_init();
            model = vosk_model_new(model_path.c_str());
            if (!model) throw std::runtime_error("Failed to load Vosk model");
            
            // Use grammar-enabled recognizer
            rec = vosk_recognizer_new_grm(model, 16000.0, grammar_json.c_str());        
            // rec = vosk_recognizer_new(model, 1600/0.0);

            if (!rec) throw std::runtime_error("Failed to create Vosk recognizer");
            
            vosk_recognizer_set_max_alternatives(rec, 1);  // Get more alternatives
            vosk_recognizer_set_words(rec, 1);
            vosk_recognizer_set_partial_words(rec, 0);     // Disable partial words
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong while initialize_vosk speechThread: " + std::string(e.what()));   
        }
    }

    void initialize_gstreamer() {
        try {
            gst_init(nullptr, nullptr);
            pipeline = gst_parse_launch(pipeline_description.c_str(), nullptr);
            appsink = gst_bin_get_by_name(GST_BIN(pipeline), "myappsink");
            g_object_set(appsink, "emit-signals", TRUE, NULL);
            g_signal_connect(appsink, "new-sample", G_CALLBACK(on_new_sample), this);
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong while initialize_gstreamer speechThread: " + std::string(e.what()));   
        }
    }

    static GstFlowReturn on_new_sample(GstElement* sink, gpointer user_data) {
        return static_cast<speechThread*>(user_data)->process_sample(sink);
    }

    GstFlowReturn process_sample(GstElement* sink) {
        try {
            std::lock_guard<std::mutex> lock(cleanup_mutex); // ADDED: Lock during processing
        
            if (stop || !rec) return GST_FLOW_OK; 
            GstSample* sample = gst_app_sink_pull_sample(GST_APP_SINK(sink));
            if (!sample) return GST_FLOW_ERROR;
        
            GstBuffer* buffer = gst_sample_get_buffer(sample);
            GstMapInfo map;
            if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
                last_audio_time = std::chrono::system_clock::now();
                
                if (vosk_recognizer_accept_waveform(rec, 
                    reinterpret_cast<const char*>(map.data), map.size)) { 
                    process_final_result();
                }
                gst_buffer_unmap(buffer, &map);
            }
            gst_sample_unref(sample);
            return GST_FLOW_OK;
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong while process_sample speechThread: " + std::string(e.what()));   
            return GST_FLOW_ERROR;
        }
        
    }

    void timeout_checker() {
        try {
        while (!stop) {
            auto now = std::chrono::system_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                    now - last_audio_time).count();

                if (elapsed >= timeout_seconds) {
                force_final_result();
                    last_audio_time = now;
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong while timeout_checker speechThread: " + std::string(e.what()));   
        }
    }

    void process_final_result() {
        try {
            const char* result = vosk_recognizer_result(rec);
            handle_result(result);
            vosk_recognizer_reset(rec);
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong while process_final_result speechThread: " + std::string(e.what()));   
        }
    }

    void force_final_result() {
        try {
            std::lock_guard<std::mutex> lock(cleanup_mutex);
            if (!rec) return;
            const char* result = vosk_recognizer_final_result(rec);
            handle_result(result);
            vosk_recognizer_reset(rec);
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong while force_final_result speechThread: " + std::string(e.what()));   
        }
    }

    void handle_result(const char* result) {
        try {
            if (!result || !command_callback) return;
            
            // std::cout << "Raw Vosk Result: " << result << std::endl;  // Debug output
            
            Json::Value root;
            Json::Reader reader;
            if (reader.parse(result, root)) {
                if (root.isMember("alternatives") && root["alternatives"].isArray()) {
                    for (const auto& alt : root["alternatives"]) {
                        if (alt.isMember("text") && alt.isMember("confidence")) {
                            std::string text = alt["text"].asString();
                            float confidence = alt["confidence"].asFloat();
                            if (!text.empty()) {
                                std::cout << "Processed text: " << text 
                                        << " (confidence: " << confidence << ")" << std::endl;
                            }
                            // float normalized_confidence = std::min(100.0f, confidence * 100.0f / 150.0f); // Example scaling
                            if (!text.empty() && confidence > 50.0f) {
                                LOG_INFO("Recognized text: " + text + " Confidence: " + std::to_string(confidence));  
                                command_callback(text);
                            }
                        }
                    }
                }
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong while handle_result speechThread: " + std::string(e.what()));   
        } 
    }

    void cleanup() {
        std::lock_guard<std::mutex> lock(cleanup_mutex); // ADDED: Lock during cleanup
        
        if (rec) {
            vosk_recognizer_free(rec);
            rec = nullptr;
        }
        if (model) {
            vosk_model_free(model);
            model = nullptr;
        }
        if (pipeline) {
            gst_object_unref(pipeline);
            pipeline = nullptr;
        }
    }
};

#endif // SPEECHThREAD_H