#ifndef AUDIO_H
#define AUDIO_H

#include <gst/gst.h>
#include <iostream>
#include <string>
#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <array>
#include "Logger.h"
#include <algorithm> // Add this

//g++ -o main main.cpp `pkg-config --cflags --libs gstreamer-1.0`

class AudioStreamer {
public:
    AudioStreamer(std::string _audio_outcoming_pipeline) : 
    audio_outcoming_pipeline(_audio_outcoming_pipeline){
    }

    void init(){
        try {            
            // Initialize GStreamer
            gst_init(nullptr, nullptr);

            // Create GStreamer pipeline
            GError *error = nullptr;
            pipeline = gst_parse_launch(audio_outcoming_pipeline.c_str(), &error);
            if (!pipeline) {
                LOG_ERROR("Failed to create pipeline: " + std::string(error->message));
                g_clear_error(&error);
                throw std::runtime_error("Failed to create GStreamer pipeline");
            }

            LOG_INFO("--- Audio streaming pipeline");

            // Create bus to get events from GStreamer pipeline
            bus = gst_element_get_bus(pipeline);
            gst_bus_add_signal_watch(bus);
            g_signal_connect(bus, "message::eos", G_CALLBACK(&AudioStreamer::on_eos), this);
            g_signal_connect(bus, "message::error", G_CALLBACK(&AudioStreamer::on_error), this);
            _init = true;
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong while initing the AudioStreamer: " + std::string(e.what()));      
        }

    };

    void run() {
        try {
            gst_element_set_state(pipeline, GST_STATE_PLAYING);
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong while runing the AudioStreamer: " + std::string(e.what()));      
        }
    };

    void quit() {
        try {
            if (_init) {
                gst_element_set_state(pipeline, GST_STATE_NULL);
                _init = false;
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong while quiting the AudioStreamer: " + std::string(e.what()));      
        }
    };

    void update_pipline(std::string _audio_outcoming_pipeline){
        audio_outcoming_pipeline = _audio_outcoming_pipeline;
        
    }

    ~AudioStreamer() {
        if (pipeline) {
            gst_object_unref(pipeline);
        }
        if (bus) {
            gst_object_unref(bus);
        }
    };

private:
    GstElement *pipeline;
    GstBus *bus;
    std::string audio_outcoming_pipeline;
    bool _init = false;

    static void on_eos([[maybe_unused]] GstBus *bus, [[maybe_unused]] GstMessage *msg, gpointer user_data) {
        try {
            AudioStreamer *self = static_cast<AudioStreamer *>(user_data);
            LOG_INFO("on_eos(): seeking to start of audio");
            gst_element_seek_simple(
                self->pipeline,
                GST_FORMAT_TIME,
                (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
                0
            );
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong in on_eos Audio streamer: " + std::string(e.what()));      
        }
    };

    static void on_error([[maybe_unused]] GstBus *bus, [[maybe_unused]] GstMessage *msg, [[maybe_unused]] gpointer user_data) {
        try {
            GError *err;
            gchar *debug_info;
            gst_message_parse_error(msg, &err, &debug_info);
            LOG_ERROR("on_error(): " + std::string(err->message));
            g_clear_error(&err);
            g_free(debug_info);
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong in on_error Audio streamer: " + std::string(e.what()));      
        }
    };
};

class AudioPlayer {
public:
    AudioPlayer(std::string _audio_incoming_pipeline) :
    audio_incoming_pipeline(_audio_incoming_pipeline) {}
    
    void init(){
        try {
            // Initialize GStreamer
            gst_init(nullptr, nullptr);

            // Create GStreamer pipeline
            GError *error = nullptr;            
            
            pipeline = gst_parse_launch(audio_incoming_pipeline.c_str(), &error);
            
            if (error) {
                LOG_ERROR("Error in pipeline creation: " + std::string(error->message));
                g_clear_error(&error);
                return;
            }
            if (!pipeline) {
                LOG_ERROR("Failed to create pipeline: " + std::string(error->message));
                g_clear_error(&error);
                throw std::runtime_error("Failed to create GStreamer pipeline");
            } else {
                LOG_INFO("Pipeline created successfully");
            }                      
            // Create bus to get events from GStreamer pipeline
            bus = gst_element_get_bus(pipeline);
            gst_bus_add_signal_watch(bus);
            g_signal_connect(bus, "message::eos", G_CALLBACK(&AudioPlayer::on_eos), this);
            g_signal_connect(bus, "message::error", G_CALLBACK(&AudioPlayer::on_error), this);
            _init = true;
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong while initing the AudioPlayer: " + std::string(e.what()));      
        }
    };

    void run() {
        try {
            GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
            if (ret == GST_STATE_CHANGE_FAILURE) {
                LOG_ERROR("Failed to set pipeline to PLAYING state");
                throw std::runtime_error("Pipeline state change failed");
            }
            LOG_INFO("AudioPlayer pipeline started");
        } catch (const std::exception& e) {
            LOG_ERROR("Error running AudioPlayer: " + std::string(e.what()));
        }
    };

    void quit() {
        try {
            if (_init) {
                gst_element_set_state(pipeline, GST_STATE_NULL);
                _init = false;
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong while quiting the AudioPlayer: " + std::string(e.what()));      
        }
    };

    void update_pipline(std::string _audio_incoming_pipeline){
        audio_incoming_pipeline = _audio_incoming_pipeline;        
        LOG_INFO("update audio_incoming_pipeline " + audio_incoming_pipeline);
    }
    
    ~AudioPlayer() {
        if (pipeline) {
            gst_object_unref(pipeline);
        }
        if (bus) {
            gst_object_unref(bus);
        }
    };

private:
    GstElement *pipeline;
    GstBus *bus;
    std::string audio_incoming_pipeline;    
    bool _init = false;

    static void on_eos([[maybe_unused]] GstBus *bus, [[maybe_unused]] GstMessage *msg, gpointer user_data) {
        try {
            AudioPlayer *self = static_cast<AudioPlayer *>(user_data);
            LOG_INFO("on_eos(): seeking to start of audio");
            gst_element_seek_simple(
                self->pipeline,
                GST_FORMAT_TIME,
                (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
                0
            );
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong in on_eos AudioPlayer: " + std::string(e.what()));      
        }
    };

    static void on_error([[maybe_unused]] GstBus *bus, [[maybe_unused]] GstMessage *msg, gpointer user_data) {
        try {
            AudioPlayer *self = static_cast<AudioPlayer*>(user_data);
            GstStateChangeReturn ret = gst_element_set_state(self->pipeline, GST_STATE_PLAYING);
            if (ret == GST_STATE_CHANGE_FAILURE) {
                LOG_ERROR("Failed to set pipeline to PLAYING state");
                throw std::runtime_error("Pipeline state change failed");
            }
            LOG_INFO("AudioPlayer pipeline started");
        } catch (const std::exception& e) {
            LOG_ERROR("Error running AudioPlayer: " + std::string(e.what()));
        }
    };

    
};

class AudioControl {
public:
    void setVolumeLevel(int volume) {
        try {
            std::string command = "amixer -c wm8904audio set Headphone " + std::to_string(volume) ;
            executeCommand(command);
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong in setVolumeLevel AudioControl: " + std::string(e.what()));
        }
    }

    int getVolumeLevel() {
        try {
            std::string command = "amixer -c wm8904audio get Headphone";
            std::string output = executeCommand(command);
            std::istringstream stream(output);
            std::string line;

            while (std::getline(stream, line)) {
                if (line.find("Front Left:") != std::string::npos) {
                    size_t start = line.find(":") + 1;
                    size_t end = line.find("[") - 1;
                    if (start != std::string::npos && end != std::string::npos) {
                        return std::stoi(line.substr(start, end - start));
                    }
                }
            }
            return -1; // Return -1 if volume level is not found
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong in getVolumeLevel AudioControl: " + std::string(e.what()));     
            return -1; // Return -1 if volume level is not found 
        }
        }

    std::string getCaptureInputType() {        
        try {
            std::string command = "amixer -c wm8904audio get 'Capture Input'";
            std::string output = executeCommand(command);
            std::istringstream stream(output);
            std::string line;
            
            while (std::getline(stream, line)) {
                if (line.find("Item0:") != std::string::npos) {
                    size_t start = line.find("'") + 1;
                    size_t end = line.rfind("'");
                    if (start != std::string::npos && end != std::string::npos && start < end) {
                        return line.substr(start, end - start);
                    }
                }
            }
            return ""; // Return empty string if capture input type is not found
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong in getCaptureInputType AudioControl: " + std::string(e.what()));      
            return ""; // Return empty string if capture input type is not found
        }
    };

    void setCaptureInputType(const std::string &capture_input_type) {
        try {            
            std::string command = "amixer -c wm8904audio cset name=\"Capture Input\" " + capture_input_type;
            executeCommand(command);
            std::string _val = getCaptureInputType();
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong in setCaptureInputType AudioControl: " + std::string(e.what()));      
        }

    };

    void setCaptureInputVolume(int volume){
        try {
            std::string command = "amixer -c wm8904audio set Capture " + std::to_string(volume) ;
            executeCommand(command);
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong in setCaptureInputVolume AudioControl: " + std::string(e.what()));      
        }
    }

    int getCaptureInputVolume(){
        try {
            std::string command = "amixer -c wm8904audio get Capture";
            std::string output = executeCommand(command);
            std::istringstream stream(output);
            std::string line;

            while (std::getline(stream, line)) {
                if (line.find("Front Left:") != std::string::npos) {
                    size_t capturePos = line.find("Capture");
                    if (capturePos != std::string::npos) {
                        size_t start = capturePos + 7; // "Capture" is 7 characters
                        // Skip non-digit characters until a digit is found
                        while (start < line.size() && !isdigit(line[start])) {
                            start++;
                        }
                        size_t end = start;
                        // Collect all consecutive digits
                        while (end < line.size() && isdigit(line[end])) {
                            end++;
                        }
                        if (start < end) {
                            std::string volumeStr = line.substr(start, end - start);
                            // Ensure the string contains only digits
                            if (!volumeStr.empty() && std::all_of(volumeStr.begin(), volumeStr.end(), ::isdigit)) {
                                return std::stoi(volumeStr);
                            } else {
                                LOG_ERROR("Extracted Capture string is not a valid number: " + volumeStr);
                            }
                        }
                    }
                }
            }
            LOG_ERROR("Capture volume not found in amixer output");
            return -1; // Return -1 if volume level is not found
        } catch (const std::exception& e) {
            LOG_ERROR("Error in getCaptureInputVolume: " + std::string(e.what()));
            return -1;
        }
    }

    void setDigitalPlaybackVolume(int volume) {
        try {
            std::string command = "amixer -c wm8904audio set Digital playback " + std::to_string(volume) ;
            executeCommand(command);
        } catch (const std::exception& e) {
            LOG_ERROR("Error in setDigitalPlaybackVolume: " + std::string(e.what()));
        }
    }

    int getDigitalPlaybackVolume() {
        try {
            std::string command = "amixer -c wm8904audio get Digital";
            std::string output = executeCommand(command);
            std::istringstream stream(output);
            std::string line;

            while (std::getline(stream, line)) {
                if (line.find("Front Left:") != std::string::npos) {
                    size_t playbackPos = line.find("Playback");
                    if (playbackPos != std::string::npos) {
                        size_t start = playbackPos + 8; // "Playback" is 8 characters
                        // Skip non-digit characters until a digit is found
                        while (start < line.size() && !isdigit(line[start])) {
                            start++;
                        }
                        size_t end = start;
                        // Collect all consecutive digits
                        while (end < line.size() && isdigit(line[end])) {
                            end++;
                        }
                        if (start < end) {
                            std::string volumeStr = line.substr(start, end - start);
                            // Ensure the string contains only digits
                            if (!volumeStr.empty() && std::all_of(volumeStr.begin(), volumeStr.end(), ::isdigit)) {
                                return std::stoi(volumeStr);
                            } else {
                                LOG_ERROR("Extracted Playback string is not a valid number: " + volumeStr);
                            }
                        }
                    }
                }
            }
            return -1; // Return -1 if volume level is not found
        } catch (const std::exception& e) {
            LOG_ERROR("Error in getDigitalPlaybackVolume: " + std::string(e.what()));
            return -1;
        }
    }

    void setDigitalCaptureVolume(int volume) {
        try {
            std::string command = "amixer -c wm8904audio set Digital capture " + std::to_string(volume) ;
            executeCommand(command);
        } catch (const std::exception& e) {
            LOG_ERROR("Error in setDigitalCaptureVolume: " + std::string(e.what()));
        }
    }

    int getDigitalCaptureVolume() {
        try {
            std::string command = "amixer -c wm8904audio get Digital";
            std::string output = executeCommand(command);
            std::istringstream stream(output);
            std::string line;

            while (std::getline(stream, line)) {
                if (line.find("Front Left:") != std::string::npos) {
                    size_t capturePos = line.find("Capture");
                    if (capturePos != std::string::npos) {
                        size_t start = capturePos + 7; // "Capture" is 7 characters
                        // Skip non-digit characters until a digit is found
                        while (start < line.size() && !isdigit(line[start])) {
                            start++;
                        }
                        size_t end = start;
                        // Collect all consecutive digits
                        while (end < line.size() && isdigit(line[end])) {
                            end++;
                        }
                        if (start < end) {
                            std::string volumeStr = line.substr(start, end - start);
                            // Ensure the string contains only digits
                            if (!volumeStr.empty() && std::all_of(volumeStr.begin(), volumeStr.end(), ::isdigit)) {
                                return std::stoi(volumeStr);
                            } else {
                                LOG_ERROR("Extracted Capture string is not a valid number: " + volumeStr);
                            }
                        }
                    }
                }
            }
            return -1; // Return -1 if volume level is not found
        } catch (const std::exception& e) {
            LOG_ERROR("Error in getDigitalCaptureVolume: " + std::string(e.what()));
            return -1;
        }
    }

    void setLineOutputVolume(int volume) {
        try {
            std::string command = "amixer -c wm8904audio set 'Line Output' " + std::to_string(volume) ;
            executeCommand(command);
        } catch (const std::exception& e) {
            LOG_ERROR("Error in setLineOutputVolume: " + std::string(e.what()));
        }
    }

    int getLineOutputVolume() {
        try {
            std::string command = "amixer -c wm8904audio get 'Line Output'";
            std::string output = executeCommand(command);
            std::istringstream stream(output);
            std::string line;

            while (std::getline(stream, line)) {
                if (line.find("Front Left:") != std::string::npos) {
                    if (line.find("Front Left:") != std::string::npos) {
                        size_t start = line.find(":") + 1;
                        size_t end = line.find("[") - 1;
                        if (start != std::string::npos && end != std::string::npos) {
                            return std::stoi(line.substr(start, end - start));
                        }
                    }
                }
            }
            return -1;
        } catch (const std::exception& e) {
            LOG_ERROR("Error in getLineOutputVolume: " + std::string(e.what()));
            return -1;
        }
    }   

    void setDigitalPlaybackBoostVolume(int volume) {
        try {
            std::string command = "amixer -c wm8904audio set 'Digital Playback Boost' " + std::to_string(volume) ;
            executeCommand(command);
        } catch (const std::exception& e) {
            LOG_ERROR("Error in setDigitalPlaybackBoostVolume: " + std::string(e.what()));
        }
    }

    int getDigitalPlaybackBoostVolume() {
        try {
            std::string command = "amixer -c wm8904audio get 'Digital Playback Boost'";
            std::string output = executeCommand(command);
            std::istringstream stream(output);
            std::string line;

            while (std::getline(stream, line)) {
                if (line.find("Mono:") != std::string::npos) {
                    if (line.find("Mono:") != std::string::npos) {
                        size_t start = line.find(":") + 1;
                        size_t end = line.find("[") - 1;
                        if (start != std::string::npos && end != std::string::npos) {
                            return std::stoi(line.substr(start, end - start));
                        }
                    }
                }
            }
            return -1;
        } catch (const std::exception& e) {
            LOG_ERROR("Error in getDigitalPlaybackBoostVolume: " + std::string(e.what()));
            return -1;
        }
    }   

    void setDigitalSidetoneVolume(int volume) {
        try {
            std::string command = "amixer -c wm8904audio set 'Digital Sidetone' " + std::to_string(volume) ;
            executeCommand(command);
        } catch (const std::exception& e) {
            LOG_ERROR("Error in setDigitalSidetoneVolume: " + std::string(e.what()));
        }
    }

    int getDigitalSidetoneVolume() {
        try {
            std::string command = "amixer -c wm8904audio get 'Digital Sidetone'";
            std::string output = executeCommand(command);
            std::istringstream stream(output);
            std::string line;

            while (std::getline(stream, line)) {
                if (line.find("Front Left:") != std::string::npos) {
                if (line.find("Front Left:") != std::string::npos) {
                        size_t start = line.find(":") + 1;
                        size_t end = line.find("[") - 1;
                    if (start != std::string::npos && end != std::string::npos) {
                        return std::stoi(line.substr(start, end - start));
                        }
                    }
                }
            }
            return -1;
        } catch (const std::exception& e) {
            LOG_ERROR("Error in getDigitalSidetoneVolume: " + std::string(e.what()));
            return -1;
        }
    }

private:
    std::string executeCommand(const std::string &command) {
        std::array<char, 128> buffer;
        std::string result;
        FILE *pipe = popen(command.c_str(), "r");
        if (!pipe) {
            LOG_ERROR("popen() failed!");
            throw std::runtime_error("popen() failed!");
        }
        try {
            while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
                result += buffer.data();
            }
        } catch (...) {
            pclose(pipe);
            LOG_ERROR("on_error(): in the catch");
            throw;
        }
        pclose(pipe);
        return result;
    };
};

#endif // AUDIO_H
