#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include "/usr/include/jsoncpp/json/json.h"
#include <stdexcept>
#include <algorithm>
#include "Logger.h"
#include <regex>
//sudo apt-get install libjsoncpp-dev
//g++ -o main main.cpp -ljsoncpp

struct IMUConfig {
    std::string imu_model_path;
    std::string i2c_device;
    int i2c_addr;
    int WHO_AM_I;
    int CTRL1;
    int ON_CTRL1;
    int OUT_X_L, OUT_X_H, OUT_Y_L, OUT_Y_H, OUT_Z_L, OUT_Z_H;
};

class Configuration {

    public:
        std::string config_path;
        std::string client_address;
        int server_port;
        std::string location;
        int audio_streaming_port_server;
        int audio_streaming_port_client;
        std::string path_to_save_file;
        std::string api_key;
        std::string api_url;
        int status_update;
        std::string uri_avatars;
        std::string uri_images;
        std::string certificate_file;
        std::string gps_file;
        std::string battery_file;
        double voltage_hi;
        double voltage_mid;
        std::string download_file;
        std::string upload_file;
        std::string battery_file_full;
        std::string battery_file_mid;
        std::string battery_file_low;
        std::string battery_file_empty;
        std::string image_folder;
        std::string default_video_quality;
        std::string default_video_quality_call;
        int standby_delay;
        int power_off_delay;
        std::string wireless_interface;
        std::string wifi_default_ssid;
        std::string wifi_default_password;
        std::string wifi_default_uri;
        std::string wifi_file;
        std::string avatar_file;
        std::string wifi_base_ssid;
        std::string wifi_connect;
        std::string wifi_icon_nohttp;
        std::string wifi_icon_novpn;
        std::string wifi_icon_nowifi;
        int bandwidth_measurement_port;
        int bandwidth_measurement_buffer;
        int bandwidth_measurement_runs;
        int bandwidth_measurement_delay;
        int bandwidth_measurement_iterations;
        int period;
        int bitrate;
        int speriod;
        std::string camera_device;    
        int maintab;  
        int rotate;    
        int font_size;    
        std::string todo;
        // std::string vosk_model;
        std::string default_language;
        int debug;
        int level;
        int testbench;
        int streaming_codex;
        std::string script_gps;
        std::string script_vpn;
        int width;
        int height;
        int swidth;
        int sheight;
        std::string _vl_loopback;
        std::string _vl_loopback_small;
        std::string snapshot_pipeline;
        std::string snapshot_file;
        std::string _vs_streaming;
        std::string _vs_streaming_original;
        std::string _vp_remote;
        std::string audio_incoming_pipeline;
        std::string microphone_pipeline_str;
        std::string phone_pipeline_str;
        std::string pipeline_description;
        // std::string grammar_json;
        double microphoneVolume;
        std::string audio_outcoming_pipeline;
        double critical_threshold;
        double low_threshold;
        double low_battery_delay_short;
        double low_battery_delay_long;
        std::string LOG_BOOK_FILE_PATH;
        std::string SHUTDOWN_FILE_PATH;
        int MIN_WORKING_SECONDS;
        int RED_WORKING_SECONDS;
        int MINIMUM_N_READINGS;
        float READING_DELAY;
        int SAMPLING_INTERVAL;
        float VDD_VOLTAGE;
        float GAIN_FACTOR;        
        int battery_i2c_channel;
        int battery_i2c_address;
        std::string ssl_cert_path;
        std::map<std::string, int> number_mappings;
        IMUConfig imu; 

        Configuration(const std::string &path) : config_path(path) {
            try {
                LOG_INFO("Configuration Constructor");
                std::ifstream file(path);
                if (!file.is_open()) {
                    LOG_ERROR("Failed to open configuration file.");
                    throw std::runtime_error("Failed to open configuration file.");
                }
                std::ostringstream buffer;
                buffer << file.rdbuf();
                std::string text = buffer.str();

                Json::Value config;
                Json::CharReaderBuilder reader;
                std::string errs;

                std::istringstream s(text);
                if (!Json::parseFromStream(reader, s, &config, &errs)) {
                    LOG_ERROR("Failed to parse configuration JSON: " + std::string(errs));
                    throw std::runtime_error("Failed to parse configuration JSON: " + errs);
                }
                // Set up the environmental variables
                client_address = config["client_address"].asString();
                server_port = config["server_port"].asInt();
                location = config["video_source"].asString();
                audio_streaming_port_server = config["audio_streaming_port_server"].asInt();
                audio_streaming_port_client = config["audio_streaming_port_client"].asInt();
                path_to_save_file = config["path_to_save_file"].asString();
                api_key = config["api_key"].asString();
                api_url = config["api_url"].asString();
                status_update = config["status_update"].asInt();
                uri_avatars = config["uri_avatars"].asString();
                uri_images = config["uri_images"].asString();
                certificate_file = config["certificate_file"].asString();
                gps_file = config["gps_file"].asString();
                battery_file = config["battery_file"].asString();
                voltage_hi = config["voltage_hi"].asDouble();
                voltage_mid = config["voltage_mid"].asDouble();
                download_file = config["download_file"].asString();
                upload_file = config["upload_file"].asString();
                battery_file_full = config["battery_file_full"].asString();
                battery_file_mid = config["battery_file_mid"].asString();
                battery_file_low = config["battery_file_low"].asString();
                battery_file_empty = config["battery_file_empty"].asString();
                image_folder = config["image_folder"].asString();
                default_video_quality = config["default_video_quality"].asString();
                default_video_quality_call = config["default_video_quality_call"].asString();
                standby_delay = config["standby_delay"].asInt();
                power_off_delay = config["power_off_delay"].asInt();

                wireless_interface = config["wireless_interface"].asString();
                wifi_default_ssid = config["wifi_default_ssid"].asString();
                wifi_default_password = config["wifi_default_password"].asString();
                wifi_default_uri = config["wifi_default_uri"].asString();
                wifi_file = config["wifi_file"].asString();
                wifi_base_ssid = config["wifi_base_ssid"].asString();

                avatar_file = config["avatar_file"].asString();
                wifi_connect = config["wifi_connect"].asString();
                wifi_icon_nohttp = config["wifi_icon_nohttp"].asString();
                wifi_icon_novpn = config["wifi_icon_novpn"].asString();
                wifi_icon_nowifi = config["wifi_icon_nowifi"].asString();

                // UDP bandwidth measurement
                bandwidth_measurement_port = config["bandwidth_measurement_port"].asInt();
                bandwidth_measurement_buffer = config["bandwidth_measurement_buffer"].asInt();
                bandwidth_measurement_runs = config["bandwidth_measurement_runs"].asInt();
                bandwidth_measurement_delay = config["bandwidth_measurement_delay"].asInt();
                bandwidth_measurement_iterations = config["bandwidth_measurement_iterations"].asInt();
                
                period = config["period"].asInt();
                speriod = config["speriod"].asInt();
                bitrate = config["bitrate"].asInt();
                maintab = config["maintab"].asInt();
                rotate = config["rotate"].asInt();
                font_size = config["font_size"].asInt();
                camera_device = config["camera_device"].asString();                
                todo = config["todo"].asString();
                // vosk_model = config["vosk_model"].asString();
                default_language = config["default_language"].asString();
                debug = config["debug"].asInt();
                level = config["level"].asInt();
                testbench = config["testbench"].asInt();
                streaming_codex = config["streaming_codex"].asInt();
                script_gps = config["script_gps"].asString();
                script_vpn = config["script_vpn"].asString();
                width = config["width"].asInt();
                height = config["height"].asInt();
                swidth = config["swidth"].asInt();
                sheight = config["sheight"].asInt();

                // sets loopback pipeline 
                int fps = 15;
                if (period == 0)
                    fps = 30;
                else if (period == 1)
                    fps = 15;
                period = fps;
                // std::cout << "period : " << period  << std::endl;
                _vl_loopback = config["pipelines"]["_vl_loopback"].asString();
                _vl_loopback = replacePlaceholder(_vl_loopback, "$video", camera_device);
                _vl_loopback = replacePlaceholder(_vl_loopback, "$Width", std::to_string(width));
                _vl_loopback = replacePlaceholder(_vl_loopback, "$Height", std::to_string(height));
                // std::cout << "_vl_loopback : " << _vl_loopback  << std::endl;
                // sets small loopback pipeline
                _vl_loopback_small = config["pipelines"]["_vl_loopback_small"].asString();
                _vl_loopback_small = replacePlaceholder(_vl_loopback_small, "$video", camera_device);

                // sets snapshot pipeline
                snapshot_pipeline = config["pipelines"]["snapshot_pipeline"].asString();
                snapshot_pipeline = replacePlaceholder(snapshot_pipeline, "$video", camera_device);
                snapshot_pipeline = replacePlaceholder(snapshot_pipeline, "$Width", std::to_string(width));
                snapshot_pipeline = replacePlaceholder(snapshot_pipeline, "$Height", std::to_string(height));

                // sets snapshot file
                snapshot_file = config["pipelines"]["snapshot_file"].asString();

                // sets streaming pipeline
                if (speriod == 0)
                    fps = 25;
                else if (speriod == 1)
                    fps = 15;
                speriod = fps;
                // std::cout << "speriod : " << speriod  << std::endl;
                if (streaming_codex == 0)
                    _vs_streaming_original = config["pipelines"]["_vs_streaming"].asString();
                else if (streaming_codex == 1)
                    _vs_streaming_original = config["pipelines"]["_vs_streaming_265"].asString();
                _vs_streaming = replacePlaceholder(_vs_streaming_original, "$Width", std::to_string(swidth));
                _vs_streaming = replacePlaceholder(_vs_streaming, "$Height", std::to_string(sheight));
                _vs_streaming = replacePlaceholder(_vs_streaming, "$FPS", std::to_string(fps));
                _vs_streaming = replacePlaceholder(_vs_streaming, "$bitrate", std::to_string(bitrate));
                // std::cout << "_vs_streaming : " << _vs_streaming  << std::endl;

                // sets remote pipeline
                _vp_remote = config["pipelines"]["_vp_remote"].asString();
                _vp_remote = replacePlaceholder(_vp_remote, "$REMOTE_PORT", std::to_string(server_port));

                // sets incoming audio pipeline
                audio_incoming_pipeline = config["pipelines"]["audio_incoming"].asString();
                audio_incoming_pipeline = replacePlaceholder(audio_incoming_pipeline, "$AUDIO_PORT_CLIENT", std::to_string(audio_streaming_port_client));
                
                // sets outgoing audio pipeline
                microphoneVolume = 2.0;
                audio_outcoming_pipeline = config["pipelines"]["audio_outcoming"].asString();
                audio_outcoming_pipeline = replacePlaceholder(audio_outcoming_pipeline, "$AUDIO_PORT_SERVER", std::to_string(audio_streaming_port_server));

                microphone_pipeline_str = config["pipelines"]["microphone_pipeline"].asString();
                phone_pipeline_str = config["pipelines"]["headphones_pipeline"].asString();
                pipeline_description = config["pipelines"]["pipeline_description"].asString();
                pipeline_description = replacePlaceholder(pipeline_description, "$level", std::to_string(level));
                // std::ostringstream oss;
                // oss << "[";
                // const Json::Value& grammar = config["grammar"];
                // for (Json::ArrayIndex i = 0; i < grammar.size(); ++i) {
                //     if (i > 0) oss << ",";
                //     std::string word = grammar[i].asString(); // Get raw UTF-8 string
                //     oss << "\"" << word << "\""; // Manually quote without escaping
                // }
                // oss << "]";
                // grammar_json = oss.str();
                
                critical_threshold = config["critical_threshold"].asDouble();
                low_threshold = config["low_threshold"].asDouble();
                low_battery_delay_short = config["low_battery_delay_short"].asDouble();
                low_battery_delay_long = config["low_battery_delay_long"].asDouble();

                // POWER_MANAGEMENT_H Configuration variables
                LOG_BOOK_FILE_PATH = config["LOG_BOOK_FILE_PATH"].asString();
                MIN_WORKING_SECONDS = config["MIN_WORKING_SECONDS"].asInt();
                RED_WORKING_SECONDS = config["RED_WORKING_SECONDS"].asInt();
                MINIMUM_N_READINGS = config["MINIMUM_N_READINGS"].asInt();
                READING_DELAY = config["READING_DELAY"].asFloat();
                SAMPLING_INTERVAL = config["SAMPLING_INTERVAL"].asInt();
                VDD_VOLTAGE = config["VDD_VOLTAGE"].asFloat();
                GAIN_FACTOR = config["GAIN_FACTOR"].asFloat();
                battery_i2c_channel = config["battery_i2c_channel"].asInt();
                battery_i2c_address = std::stoi(config["battery_i2c_address"].asString(), nullptr, 16);

                // HTTPS SSL certificate
                ssl_cert_path = config["ssl_cert_path"].asString();
                // Add to the constructor (after parsing the JSON):
                const Json::Value& mappings = config["number_mappings"];
                for (const auto& mapping : mappings) {
                    std::string word = mapping["word"].asString();
                    int number = mapping["number"].asInt();
                    number_mappings[word] = number;
                }
                if (config.isMember("imu")) {
                    const auto& imu_j = config["imu"];
                    imu.imu_model_path = imu_j["imu_model_path"].asString();
                    imu.i2c_device = imu_j["i2c_device"].asString();
                    imu.i2c_addr = imu_j["i2c_addr"].asInt();
                    imu.WHO_AM_I = imu_j["WHO_AM_I"].asInt();
                    imu.CTRL1 = imu_j["CTRL1"].asInt();
                    imu.ON_CTRL1 = imu_j["ON_CTRL1"].asInt();
                    imu.OUT_X_L = imu_j["OUT_X_L"].asInt();
                    imu.OUT_X_H = imu_j["OUT_X_H"].asInt();
                    imu.OUT_Y_L = imu_j["OUT_Y_L"].asInt();
                    imu.OUT_Y_H = imu_j["OUT_Y_H"].asInt();
                    imu.OUT_Z_L = imu_j["OUT_Z_L"].asInt();
                    imu.OUT_Z_H = imu_j["OUT_Z_H"].asInt();
                }
                LOG_INFO("Finish Reading Config File");
            } catch (const std::exception &e) {
                LOG_ERROR("Error Configuration Constructor: " + std::string(e.what()));
            }
        };

        std::string replacePlaceholder(const std::string &str, const std::string &placeholder, const std::string &value) {
            std::string result = str;
            try {
                std::string result = str;
                size_t pos = result.find(placeholder);
                while (pos != std::string::npos) {
                    result.replace(pos, placeholder.length(), value);
                    pos = result.find(placeholder, pos + value.length());
                }
                return result;
            } catch (const std::exception &e) {
                LOG_ERROR("Error in  replacePlaceholder : " + std::string(e.what()));
                return result;
            }
        };
        
        void updateAudioOutcoming(const std::string &addr) {
            audio_outcoming_pipeline = replacePlaceholder(audio_outcoming_pipeline, "$SERVER_ADDRESS", addr);
            LOG_INFO("updateAudioOutcoming " + audio_outcoming_pipeline);
        };

        void updateApiUri(const std::string &uri) {
            try {
                // Substitute only IP and keep the rest
                size_t pos = api_url.find("//");
                if (pos != std::string::npos) {
                    size_t start = pos + 2;
                    size_t end = api_url.find('/', start);
                    api_url.replace(start, end - start, uri);
                }
            } catch (const std::exception &e) {
                LOG_ERROR("Something went wrong while updating the URI: " + std::string(e.what()));
            }
        };

        void updatestreaming(int _bitrate, int _fps, int _width, int _height) {
            try {
                swidth = _width;
                sheight = _height;
                speriod = (int) (1000 / _fps);
                _vs_streaming = replacePlaceholder(_vs_streaming_original, "$Width", std::to_string(_width));
                _vs_streaming = replacePlaceholder(_vs_streaming, "$Height", std::to_string(_height));
                _vs_streaming = replacePlaceholder(_vs_streaming, "$FPS", std::to_string(_fps));
                _vs_streaming = replacePlaceholder(_vs_streaming, "$bitrate", std::to_string(_bitrate));
            } catch (const std::exception &e) {
                LOG_ERROR("Error Configuration updatestreaming: " + std::string(e.what()));
            }
        };

        // Method to update the default language
        void updateDefaultLanguage(const std::string& new_language) {
            try {
                // Read the entire file into a string
                std::string content = readFileToString(config_path);
                // LOG_INFO("Original content: " + content);

                // Find the position of "default_language":
                size_t key_pos = content.find("\"default_language\":");
                if (key_pos == std::string::npos) {
                    LOG_ERROR("Failed to find 'default_language' key in the configuration file");
                    return;
                }

                // Find the start of the value (next quote after the key)
                size_t value_start = content.find('"', key_pos + 19); // 19 is length of "\"default_language\":"
                if (value_start == std::string::npos) {
                    LOG_ERROR("Failed to find the start of the value for 'default_language'");
                    return;
                }

                // Find the end of the value (next quote after value_start)
                size_t value_end = content.find('"', value_start + 1);
                if (value_end == std::string::npos) {
                    LOG_ERROR("Failed to find the end of the value for 'default_language'");
                    return;
                }

                // Replace the old value with the new language
                content.replace(value_start + 1, value_end - value_start - 1, new_language);

                // Write the updated content back to the file
                std::ofstream out_file(config_path, std::ios::out | std::ios::binary);
                if (!out_file.is_open()) {
                    LOG_ERROR("Failed to open configuration file for writing: " + config_path);
                    throw std::runtime_error("Failed to open configuration file for writing.");
                }
                out_file << content;
                out_file.close();

                // Verify the update
                // std::string updated_content = readFileToString(config_path);
                // LOG_INFO("File content after saving: " + updated_content);

                // Update the member variable
                default_language = new_language;
                LOG_INFO("Default language updated to: " + new_language);
            } catch (const std::exception& e) {
                LOG_ERROR("Error updating default language: " + std::string(e.what()));
            }
        }

        // Helper function to read file content into a string
        std::string readFileToString(const std::string& path) {
            std::ifstream file(path);
            if (!file.is_open()) {
                LOG_ERROR("Failed to open file: " + path);
                throw std::runtime_error("Failed to open file: " + path);
            }
            std::stringstream buffer;
            buffer << file.rdbuf();
            file.close();
            // LOG_INFO("CONFIGURATION file content: " +  buffer.str());
            return buffer.str();
        }       

        // Add a public method to access the mappings:
        const std::map<std::string, int>& getNumberMappings() const {
            return number_mappings;
        }

};
#endif // CONFIGURATION_H
