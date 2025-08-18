#ifndef HTTPSESSION_H
#define HTTPSESSION_H

#include <iostream>
#include <string>
#include <random>
#include <chrono>
#include <thread>
#include <fstream>
#include <nlohmann/json.hpp> // Include for JSON parsing
#include <curl/curl.h> // Include for HTTP requests
#include "Logger.h"
#include <cstring>
#include <net/if.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "Timer.h"
#include <functional>
#include "Configuration.h"
#include <atomic>
#include <filesystem>
#include <zip.h>
#include <sstream>
#include <mutex>
namespace fs = std::filesystem;

class HTTPSession {
public:

    struct GPSData {
            double lat;
            double lng;
    };
    
    HTTPSession(const std::string& api_url, const std::string& ssl_cert_path, const std::string& api_key, std::string _helmet_status, Configuration& config)
        : api_url(api_url), ssl_cert_path(ssl_cert_path), api_key(api_key), helmet_status(_helmet_status),  config(config), Remote_IP(" "), 
        operator_status("Work"), old_status(""), current_status("2"), _gpsdata({0.0, 0.0})  {
        LOG_INFO("HTTPSession Constructor");        
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }
 
    ~HTTPSession() {
        timer.stop();
        // bingtimer.stop();
        curl_global_cleanup();  // Global cleanup called once during object destruction
    }

    bool authenticate() {                   
        try {
            long new_time = get_system_time();
            set_system_time(1603984187);
            for (int i = 0; i < 5; ++i) {
                
                std::string url = api_url + "/api/helmet-ping?" + std::to_string(random_bits());
                std::string req_body = R"({"reset": true, "mac": ")" + get_mac("wlan0") + R"("})";
                std::string response;  
                long response_code;
                
                post(url, req_body, response, response_code);
                
                if (response_code == 200) {
                    auto data = nlohmann::json::parse(response);
                    set_system_time(data["epoch"]);
                    return true;
                }
                else {
                    if (i == 4) {
                        set_system_time(new_time);
                        return false;
                    }
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));            
            }
            set_system_time(new_time);
            return false;
        } catch (const std::exception& e) {
            LOG_ERROR("Warning: Something went wrong while authenticating: " + std::string(e.what()));
            return false;
        }
    }

    void get_certificate(const std::string& certificate_file) {
        try {
            std::string url = api_url + "/api/helmets/certificate?" + std::to_string(random_bits());
            std::string response;
            long response_code;

            post(url, "", response, response_code, 20L);

            if (response_code == 200) {
                LOG_INFO("[HTTP] GET /api/helmets/certificate " + std::to_string(response_code));
                std::ofstream out_file(certificate_file, std::ios::binary);
                if (out_file) {
                    out_file.write(response.c_str(), response.size());
                    out_file.close();
                } else {
                    LOG_ERROR("Failed to write certificate file");
                    throw std::runtime_error("Failed to write certificate file");
                }
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Error: Something went wrong while getting the certificate: " + std::string(e.what()));
        }
    }

    void record_ip(const std::string& client_address) {
        try {
            std::string url = api_url + "/api/helmets/record-ip?" + std::to_string(random_bits());
            std::string req_body = R"({"ipv4": ")" + client_address + R"("})";
            std::string response;
            long response_code;
            post(url, req_body, response, response_code);
            LOG_INFO("[HTTP] POST /helmets/record-ip " + std::to_string(response_code));
        } catch (const std::exception& e) {
            LOG_ERROR("Error: Something went wrong while recording IP: " + std::string(e.what()));
        }
    }

    bool is_http_available(const std::string& host) {
        try {
            for (int i = 0; i < 10; ++i) {
                    std::string url = "https://" + host + "/api/helmets/link";
                    std::string response;
                    long response_code;

                    post(url, "", response, response_code);

                    if (response_code == 200) {
                        LOG_INFO("[WIFI] HTTP traffic ok");
                        return true;
                    }
            }
            return false;
        } catch (const std::exception& e) {
            LOG_ERROR("Error: Something went wrong while checking for HTTP connection: " + std::string(e.what()));
            return false;
        }
    }

    bool is_vpn_available() {
        try {
            for (int i = 0; i < 10; ++i) {
                std::string url = "https://10.8.0.1/api/helmets/link";
                std::string response;
                long response_code;

                post(url, "", response, response_code);

                if (response_code == 200) {
                    LOG_INFO("[WIFI] VPN traffic ok");
                    return true;
                }
                
            }
            return false;
        } catch (const std::exception& e) {
            LOG_ERROR("Error: Something went wrong while checking for VPN connection: " + std::string(e.what()));
            return false;
        }
    }

    void update_api_url(const std::string& _api_url) {
        api_url = _api_url;
        LOG_INFO("update api_url " + api_url);
    }

    void update_helmet_status(const std::string _helmet_status) {
        std::lock_guard<std::mutex> lock(helmet_status_mutex);
        helmet_status = _helmet_status;
    }

    std::string get_helmet_status() {
        std::lock_guard<std::mutex> lock(helmet_status_mutex);
        return helmet_status;        
    }

    std::string get_operator_status() {
        return operator_status;        
    }

    void set_operator_status(std::string _operator_status) {   
        operator_status = _operator_status;        
    }

    void generate_notify() {
        // Start a timer that calls a update_status every 5000 ms (5 second)
        timer.start(config.status_update, 0, [this]() { update_status(); });
        // Start a timer that calls a send_ping every 5000 ms (1 second)
        // bingtimer.start(1000, [this]() { send_ping(); });
    }

    void stop_notify() {
        // Stop a timer
        timer.stop();
    }

    void set_update_status_callback(const std::function<void(nlohmann::json, std::string)> &callback) {
        update_status_callback = callback;
    }

    void record_event(const std::string& cmd, const std::string& data) {
        try {
            nlohmann::json req_body_json = {{"event",{{"cmd", cmd},{"data", data}}}};
            std::string url = api_url + "/api/helmets/record-event?" + std::to_string(random_bits());
            std::string req_body = req_body_json.dump();
            std::string response;  
            long response_code;

            post(url, req_body, response, response_code);
            if (response_code == 200) {
                auto data = nlohmann::json::parse(response);
                LOG_INFO("[HTTP] POST /helmets/record-event " + std::to_string(response_code));         
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong while record_event: " + std::string(e.what()));            
        }           
    }

    void update_event(int idCallEvent, std::string key, std::string value ) {
        try {
            nlohmann::json req_body_json = {
                {"idCallEvent", idCallEvent},
                {"k", key},
                {"v", value}
            };
            std::string url = api_url + "/api/helmets/update-event?" + std::to_string(random_bits());
            std::string req_body = req_body_json.dump();
            std::string response;  
            long response_code;

            post(url, req_body, response, response_code);
            if (response_code == 200) {
                auto data = nlohmann::json::parse(response);
                LOG_INFO(data.dump());   
                LOG_INFO("[HTTP] POST /helmets/update-event " + std::to_string(response_code));         
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong while update_event: " + std::string(e.what()));            
        }    
    }
    
    void request_wifi() {
        try {
            nlohmann::json req_body_json = {{"mac", get_mac("wlan0")}};
            std::string url = api_url + "/api/helmets/wifi-list?" + std::to_string(random_bits());
            std::string req_body = req_body_json.dump();
            std::string response;  
            long response_code;

            post(url, req_body, response, response_code, 10L);
            if (response_code == 200) {
                auto data = nlohmann::json::parse(response);
                LOG_INFO(data.dump());   
                LOG_INFO("[HTTP] POST /helmets/wifi-list " + std::to_string(response_code));   
                save_wifi_list(data["wifi"]);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong while request_wifi: " + std::string(e.what()));      
        }              
    }

    void request_support() {
        try {
            nlohmann::json req_body_json = {{"mac", get_mac("wlan0")}};
            std::string url = api_url + "/api/helmet-request-support?" + std::to_string(random_bits());
            std::string req_body = req_body_json.dump();
            std::string response;  
            long response_code;

            post(url, req_body, response, response_code);
            if (response_code == 200) {            
                auto data = nlohmann::json::parse(response);
                LOG_INFO(data.dump());   
                LOG_INFO("[HTTP] POST /api/helmet-request-support " + std::to_string(response_code));   
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong while request_support: " + std::string(e.what()));      
        } 
    }

    void standalone_request() {
        try {
            nlohmann::json req_body_json = {{"reset", false}, {"mac", get_mac("wlan0")}, {"status", "11"}};
            std::string url = api_url + "/api/helmets/status?" + std::to_string(random_bits());
            std::string req_body = req_body_json.dump();
            std::string response;  
            long response_code;

            post(url, req_body, response, response_code);
            if (response_code == 200) {            
                auto data = nlohmann::json::parse(response);
                LOG_INFO(data.dump());   
                update_helmet_status(data["helmet_status"]);
                LOG_INFO("[HTTP] POST /helmets/status " + std::to_string(response_code));   
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong while standalone_request: " + std::string(e.what()));      
        } 
    }
    
    void terminate_support() {
        try {
            nlohmann::json req_body_json = {{"mac", get_mac("wlan0")}};
            std::string url = api_url + "/api/helmet-terminate-support?" + std::to_string(random_bits());
            std::string req_body = req_body_json.dump();
            std::string response;  
            long response_code;

            post(url, req_body, response, response_code);
            if (response_code == 200) {
                auto data = nlohmann::json::parse(response);
                LOG_INFO(data.dump());   
                LOG_INFO("[HTTP] POST /api/helmet-terminate-support " + std::to_string(response_code));   
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong while request_wifi: " + std::string(e.what()));      
        } 
    }

    void handle_tasks_next() {
        try {
            nlohmann::json req_body_json = {{"mac", get_mac("wlan0")}};
            std::string url = api_url + "/api/helmets/tasks/next?" + std::to_string(random_bits());
            std::string req_body = req_body_json.dump();
            std::string response;  
            long response_code;

            post(url, req_body, response, response_code);
            if (response_code == 200) {
                auto data = nlohmann::json::parse(response);
                LOG_INFO(data.dump());   
                LOG_INFO("[HTTP] POST /helmets/tasks/next " + std::to_string(response_code));   
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong while handle_tasks_next: " + std::string(e.what()));      
        }
    }

    void handle_tasks_back() {
        try {
            nlohmann::json req_body_json = {{"mac", get_mac("wlan0")}};
            std::string url = api_url + "/api/helmets/tasks/back?" + std::to_string(random_bits());
            std::string req_body = req_body_json.dump();
            std::string response;  
            long response_code;

            post(url, req_body, response, response_code);
            if (response_code == 200) {
                auto data = nlohmann::json::parse(response);
                LOG_INFO(data.dump());   
                LOG_INFO("[HTTP] POST /helmets/tasks/back " + response_code);   
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong while handle_tasks_back: " + std::string(e.what()));      
        }
    }

    void handle_tasks_reset() {
        try {
            nlohmann::json req_body_json = {{"mac", get_mac("wlan0")}};
            std::string url = api_url + "/api/helmets/tasks/reset?" + std::to_string(random_bits());
            std::string req_body = req_body_json.dump();
            std::string response;  
            long response_code;

            post(url, req_body, response, response_code);
            if (response_code == 200) {
                auto data = nlohmann::json::parse(response);
                LOG_INFO(data.dump());   
                LOG_INFO("[HTTP] POST /helmets/tasks/reset " + std::to_string(response_code));   
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong while handle_tasks_reset: " + std::string(e.what()));      
        }
    }

    int check_standalone() {
        try {
            std::string url = api_url + "/api/FTP/service/check?";
            std::string response;  
            long response_code;

            get(url, response, response_code);
            if (response_code == 200) {            
                LOG_INFO("[HTTP] GET /api/FTP/service/check " + std::to_string(response_code));
                return 1; // Service available
            } else if (response_code == 404) {
                LOG_INFO("No tasks or documents assigned");   
                return 0; // No content
            } else {
                LOG_ERROR("Unexpected response code: " + std::to_string(response_code));
                return 2; // Error
            }

        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong while check_standalone: " + std::string(e.what()));      
            return 2;
        } 
    }

    void Download_standalone_FILES() {
        try {
            std::string url = api_url + "/api/FTP/service?";
            long response_code;
            std::string temp_zip = "downloaded_file.zip";
            
            // Use empty string for response and specify output file
            std::string empty_response;
            get(url, empty_response, response_code, 500L, temp_zip);

            if (response_code == 200 ) {            
                LOG_INFO("Download successful, starting extraction");
                if (fs::exists(config.todo)) {
                    handle_remove_readonly(config.todo);
                }
                extract_zip(temp_zip, config.todo);
                LOG_INFO("[HTTP] File downloaded and extracted successfully");

                // Clean up temporary zip
                if (fs::exists(temp_zip)) {
                    fs::remove(temp_zip);
                }
            } else {
                LOG_ERROR("Download failed with code: " + std::to_string(response_code));
                if (fs::exists(temp_zip)) {
                    fs::remove(temp_zip);
            }
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Download failed: " + std::string(e.what()));
        } 
    }

    int check_status() {
        std::string url = api_url + "/api/helmets/status?";
        std::string response;  
        long response_code;
        get(url, response, response_code);
        if (response_code == 200) {  
            LOG_INFO("[HTTP] GET /api/helmets/status " + std::to_string(response_code));   
            if (!response.empty()) {
                data = nlohmann::json::parse(response); 
                if (data.contains("helmet_status") && !data["helmet_status"].is_null() && data["helmet_status"].is_string()) {
                    LOG_INFO(data["helmet_status"]);
                    if (data["helmet_status"].get<std::string>().find("Standalone") != std::string::npos) {
                        return 1;
                    } else {
                        return 0;                    
                    }
                } else {
                    // JSON does not have valid "helmet_status"
                    return 0;
                }
            } else { 
                // Empty response
                return 0;
            }
        } else {
            // Non-200 status code
            return 0;
        }
    }

    GPSData getGPS() {
        return _gpsdata;
    }

private:
    std::string api_url;
    std::string ssl_cert_path;
    std::string api_key;
    std::string helmet_status;
    Configuration& config;
    std::string Remote_IP;
    std::string operator_status;
    std::string old_status, current_status;
    Timer timer;
    // Timer bingtimer;
    std::function<void(nlohmann::json, std::string)> update_status_callback;
    nlohmann::json data;
    std::mutex helmet_status_mutex;
    GPSData _gpsdata;

    static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
        ((std::string*)userp)->append((char*)contents, size * nmemb);
        return size * nmemb;
    }

    static size_t write_file_callback(void* ptr, size_t size, size_t nmemb, void* userp) {
        FILE* stream = static_cast<FILE*>(userp);
        if (!stream) {
            LOG_ERROR("File stream is null");
            return 0;
        }
        size_t written = fwrite(ptr, size, nmemb, stream);
        return written;
    }

    void post(const std::string& url, const std::string& req_body, std::string& response, long& response_code, long TIMEOUT=5L) {
        try {
            response.clear(); // Clear any previous response content
            CURL* session = curl_easy_init();
            if (!session) {
                LOG_ERROR("CURL session is not initialized");
                throw std::runtime_error("CURL session is not initialized");
            }

            if (session) 
            {
                curl_easy_setopt(session, CURLOPT_URL, url.c_str());
                curl_easy_setopt(session, CURLOPT_POST, 1L);
                curl_easy_setopt(session, CURLOPT_POSTFIELDS, req_body.c_str());
                curl_easy_setopt(session, CURLOPT_WRITEFUNCTION, write_callback);
                curl_easy_setopt(session, CURLOPT_WRITEDATA, &response);
                curl_easy_setopt(session, CURLOPT_TIMEOUT, TIMEOUT);
                // curl_easy_setopt(session, CURLOPT_SSL_VERIFYPEER, 1L); // Keep enabled
                // curl_easy_setopt(session, CURLOPT_SSL_VERIFYHOST, 2L); // Enable host verification
                // curl_easy_setopt(session, CURLOPT_CAINFO, config.ssl_cert_path.c_str()); 
                curl_easy_setopt(session, CURLOPT_SSL_VERIFYPEER, 0L); 
                curl_easy_setopt(session, CURLOPT_SSL_VERIFYHOST, 0L); 

                struct curl_slist* headers = nullptr;
                std::string header_str = "X-Api-Key: " + api_key + "\r\nHost: mydigitalhelmet.com";
                headers = curl_slist_append(headers, header_str.c_str());
                headers = curl_slist_append(headers, "Content-Type: application/json");
                curl_easy_setopt(session, CURLOPT_HTTPHEADER, headers);
                LOG_INFO("Sending POST request to: " + url);
                if (req_body != "")
                    LOG_INFO("Sending this POST request : " + req_body);

                CURLcode res = curl_easy_perform(session);
                if (res != CURLE_OK) {
                    LOG_ERROR("CURL failed with error: " + std::string(curl_easy_strerror(res)));
                    curl_slist_free_all(headers);
                    curl_easy_cleanup(session);
                    throw std::runtime_error(curl_easy_strerror(res));
                }
                curl_easy_getinfo(session, CURLINFO_RESPONSE_CODE, &response_code);
                LOG_INFO("Response Code: " + std::to_string(response_code));
                curl_easy_cleanup(session);
                curl_slist_free_all(headers);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong while sending post: " + std::string(e.what()));      
        }
    };

    void get(const std::string& url, std::string& response, long& response_code, 
             long TIMEOUT=5L, const std::string& output_file = "") {
        FILE* file = nullptr;
        try {
            response.clear();
            CURL* session = curl_easy_init();
            if (!session) {
                LOG_ERROR("CURL session is not initialized");
                throw std::runtime_error("CURL session is not initialized");
            }

            // Open file if output path is provided
            if (!output_file.empty()) {
                file = fopen(output_file.c_str(), "wb");
                if (!file) {
                    LOG_ERROR("Failed to open file for writing: " + output_file);
                    curl_easy_cleanup(session);
                    throw std::runtime_error("Failed to open output file");
                }
            }

            if (session) {
                curl_easy_setopt(session, CURLOPT_URL, url.c_str());
                
                // Set write callback based on output mode
                if (file) {
                    curl_easy_setopt(session, CURLOPT_WRITEFUNCTION, write_file_callback);
                    curl_easy_setopt(session, CURLOPT_WRITEDATA, file);
                } else {
                curl_easy_setopt(session, CURLOPT_WRITEFUNCTION, write_callback);
                curl_easy_setopt(session, CURLOPT_WRITEDATA, &response);
                }
                
                curl_easy_setopt(session, CURLOPT_TIMEOUT, TIMEOUT);
                curl_easy_setopt(session, CURLOPT_SSL_VERIFYPEER, 0L); 
                curl_easy_setopt(session, CURLOPT_SSL_VERIFYHOST, 0L);
                curl_easy_setopt(session, CURLOPT_TCP_KEEPALIVE, 1L);  // Enable TCP keep-alive
                curl_easy_setopt(session, CURLOPT_TCP_KEEPIDLE, 30L);  // Idle time before sending keep-alive
                curl_easy_setopt(session, CURLOPT_TCP_KEEPINTVL, 15L); // Interval between keep-alive probes

                struct curl_slist* headers = nullptr;
                std::string header_str = "X-Api-Key: " + api_key + "\r\nHost: mydigitalhelmet.com";
                headers = curl_slist_append(headers, header_str.c_str());
                curl_easy_setopt(session, CURLOPT_HTTPHEADER, headers);
                LOG_INFO("Sending GET request to: " + url);

                CURLcode res = curl_easy_perform(session);
                if (res != CURLE_OK) {
                    LOG_ERROR("CURL failed with error: " + std::string(curl_easy_strerror(res)));
                    if (file) {
                        fclose(file);
                        // Remove partial file on error
                        if (fs::exists(output_file)) {
                            fs::remove(output_file);
                        }
                    }
                    curl_slist_free_all(headers);
                    curl_easy_cleanup(session);
                    throw std::runtime_error(curl_easy_strerror(res));
                }
                curl_easy_getinfo(session, CURLINFO_RESPONSE_CODE, &response_code);
                LOG_INFO("Response Code: " + std::to_string(response_code));
                curl_easy_cleanup(session);
                curl_slist_free_all(headers);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong during GET: " + std::string(e.what()));      
            if (file) {
                fclose(file);
                // Remove partial file on exception
                if (fs::exists(output_file)) {
                    fs::remove(output_file);
                }
        }
        }
        if (file) {
            fclose(file);
        }
    };

    std::string get_mac(const std::string& interface) const {
        try {
            int fd = socket(AF_INET, SOCK_DGRAM, 0);
            if (fd == -1) {
                LOG_ERROR("Failed to create socket.");
                return "00:00:00:00:00:00";
            }

            struct ifreq ifr;
            strncpy(ifr.ifr_name, interface.c_str(), IFNAMSIZ - 1);

            if (ioctl(fd, SIOCGIFHWADDR, &ifr) == -1) {
                LOG_ERROR("Failed to obtain MAC address.");
                close(fd);
                return "00:00:00:00:00:00";
            }

            close(fd);

            unsigned char* mac = reinterpret_cast<unsigned char*>(ifr.ifr_hwaddr.sa_data);
            char mac_address[18];
            snprintf(mac_address, sizeof(mac_address), "%02x:%02x:%02x:%02x:%02x:%02x",
                    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

            return std::string(mac_address);
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong while get_mac: " + std::string(e.what()));    
            return "00:00:00:00:00:00";  
        }
    }

    void set_system_time(long epoch) const {
        try {
            std::string command = "date -s @" + std::to_string(epoch);  // Removed 'sudo'
            int result = std::system(command.c_str());
            if (result != 0) {
                LOG_ERROR("Failed to set system time.");
                throw std::runtime_error("Failed to set system time.");
            }
            LOG_INFO("System time set to epoch: " + std::to_string(epoch));
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong in set_system_time: " + std::string(e.what()));   
        }
    }
    
    long get_system_time() const {
        // Get current system time as duration since epoch
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        // Convert to seconds (epoch time)
        return std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    }
    
    long random_bits() const {
        return std::random_device{}();
    }

    std::string createRequestBody(const std::string mac, std::string status, double _temperature) {
        try {
            if (status !="") {
                nlohmann::json req_body_json = {
                    {"reset", false},
                    {"mac", mac},
                    {"gps", {
                        {"lat", _gpsdata.lat},
                        {"lng", _gpsdata.lng}
                    }},
                    {"status", status},
                    {"temperature", _temperature}
                };
                // Convert JSON object to string
                return req_body_json.dump();
            }
            else if (helmet_status.find("Standalone") != std::string::npos) {
                nlohmann::json req_body_json = {
                    {"reset", false},
                    {"mac", mac},
                    {"gps", {
                        {"lat", _gpsdata.lat},
                        {"lng", _gpsdata.lng}
                    }},
                    {"status", "2"},
                    {"temperature", _temperature}
                };
                // Convert JSON object to string
                return req_body_json.dump();
            }
            else {
                nlohmann::json req_body_json = {
                    {"reset", false},
                    {"mac", mac},
                    {"gps", {
                        {"lat", _gpsdata.lat},
                        {"lng", _gpsdata.lng}
                    }},
                    {"temperature", _temperature}
                };
                // Convert JSON object to string
                return req_body_json.dump();
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong in createRequestBody: " + std::string(e.what()));   
            return "";
        }        
    }

    void readGPS(){
        try {
            double lat = 55.75222; double lng = 37.61556;
            std::ifstream file(config.gps_file.c_str());
            if (file.is_open()) {
                std::string line;
                if (std::getline(file, line)) {
                    if (!line.empty() && line.find(',') != std::string::npos) {
                        std::istringstream stream(line);
                        std::string lat_str, lng_str;

                        if (std::getline(stream, lat_str, ',') && std::getline(stream, lng_str, ',')) {
                            lat = std::stod(lat_str);
                            lng = std::stod(lng_str);
                            _gpsdata = {lat, lng};
                        }
                    }
                }            
            }
            else {
                LOG_ERROR("Error: Could not open gps file.");
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Error while reading GPS coordinates: " + std::string(e.what()));
        }
    }

    double readTemperature() {
        try {
            std::ifstream file("/sys/class/thermal/thermal_zone0/temp");
            if (!file.is_open()) {
                LOG_ERROR("Could not open temperature file.");
                return 0.0;
            }

            std::string output;
            std::getline(file, output);
            file.close();

            // Convert the output to a float and divide by 1000 to get Celsius
            return std::stod(output) / 1000.0;
            
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong while reading temperature: " + std::string(e.what()));            
            return 0.0;
        }
    }
    
    std::string readIMUStatus() {
        // LOG_INFO(helmet_status); 
        // LOG_INFO(operator_status); 
        // LOG_INFO(old_status); 
        // LOG_INFO(current_status);
        if (helmet_status.find("active") != std::string::npos){
           if(operator_status == "Fall") {
                return "9";
           }           
           else if(operator_status == "Work") {
                return "4";
           }           
           else if(operator_status == "Relax") {
                return "10";
           }
        }
        else if (helmet_status.find("standby") != std::string::npos){    
           if(operator_status == "Fall") {
                return "5";
           }           
           else if(operator_status == "Work") {
                return "2";
           }           
           else if(operator_status == "Relax") {
                return "6";
           }
        }
        else if (helmet_status.find("request") != std::string::npos){
            if(operator_status == "Fall") {
                return "7";
           }           
           else if(operator_status == "Work") {
                return "3";
           }           
           else if(operator_status == "Relax") {
                return "8";
           }
        }
        return "2";
    }

    void update_status() {
        try {            
            std::string _status = "";
            readGPS();
            current_status = readIMUStatus();
            if (current_status != old_status && config.testbench == 0) {
                old_status = current_status;
                _status = current_status;
            }
            double temperature = readTemperature();

            std::string url = api_url + "/api/helmets/status?" + std::to_string(random_bits());
            std::string req_body = createRequestBody(get_mac("wlan0"), _status, temperature);
            std::string response;  
            long response_code;

            post(url, req_body, response, response_code, 10L);
            LOG_INFO(response);
            LOG_INFO(std::to_string(response_code));
            if (response_code == 200) {
                if (!response.empty()){
                    data = nlohmann::json::parse(response);
                    LOG_INFO("[HTTP] POST /helmets/status " + std::to_string(response_code)); 
                    if (data.contains("helmet_status") && !data["helmet_status"].is_null() && data["helmet_status"].is_string()) {
                        if (data.contains("call_details")) {
                            if (helmet_status.find("standby") != std::string::npos || helmet_status.find("request") != std::string::npos) {  
                                std::string _avatar = "";  
                                if (data["call_details"]["avatar"].is_null()) {
                                    _avatar = get_avatar("");
                                } else if (data["call_details"]["avatar"].is_string()) {
                                    _avatar = get_avatar(data["call_details"]["avatar"]);
                                } else {
                                    _avatar = get_avatar("");
                                }
                                data["call_details"]["avatar"] = _avatar;
                                if (Remote_IP == " ") {
                                    Remote_IP = data["call_details"]["ipv4"];
                                    LOG_INFO("Desktop Client IP: " + Remote_IP);
                                }
                                else if (Remote_IP != data["call_details"]["ipv4"]) {
                                    Remote_IP = data["call_details"]["ipv4"];
                                    LOG_INFO("Desktop Client IP: " + Remote_IP);
                                }                                                       
                            }
                            if (update_status_callback) {                                                               
                                helmet_status = operator_status + "_active";  
                                LOG_INFO(helmet_status); 
                                update_status_callback(data["call_details"], "active");
                            }               
                        }
                        else {
                            // LOG_INFO(data["helmet_status"]); 
                            helmet_status = data["helmet_status"];
                            LOG_INFO(helmet_status); 
                            if (update_status_callback) {                      
                                update_status_callback(data, "standby");
                            }                    
                        }                                            
                    }
                }
                else {
                    LOG_INFO(data["helmet_status"]);
                    // update_helmet_status(data["helmet_status"]);
                }                      
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong update_status: " + std::string(e.what()));            
        }        
    }

    void save_wifi_list(nlohmann::json _data) {
        try {
            std::ofstream file(config.wifi_file.c_str());
            LOG_INFO("Save Wifi List");
            if (file.is_open()) {
                file << _data.dump(); // Writes the access point list
                file.close();        // Closes the file
            } else {
                LOG_ERROR("Unable to open file for writing: " + config.wifi_file);   
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong while save_wifi_list: " + std::string(e.what()));      
        }
    }

    std::string get_avatar(std::string _data) {
        try {            
            std::string url = config.uri_avatars + _data;
            std::string response;  
            long response_code;

            post(url, "", response, response_code, 10L);
            if (response_code == 200) {
                LOG_INFO("[HTTP] GET " + url + " " + std::to_string(response_code));      
                std::ofstream out_file(url.c_str(), std::ios::binary);
                if (out_file) {
                    out_file.write(response.c_str(), response.size());
                    out_file.close();
                    return std::string(config.image_folder +  _data);
                } else {
                    LOG_ERROR("Failed to write avatar file");
                    return std::string(config.image_folder + "tomas.jpg");
                }        
            }
            else{
                LOG_ERROR("Failed to download avatar");
                return std::string(config.image_folder + "tomas.jpg");
            }    
                
        }catch (const std::exception& e) {
            LOG_ERROR("Error: Something went wrong while getting the avatar: " + std::string(e.what()));
            return std::string(config.image_folder + "tomas.jpg");
        }
    }

    void send_ping() {
        try {
            double temperature = readTemperature();
            std::string url = api_url + "/api/helmet-ping?" + std::to_string(random_bits());
            std::string req_body = createRequestBody(get_mac("wlan0"), "", temperature);
            std::string response;  
            long response_code;

            post(url, req_body, response, response_code);
            if (response_code == 200) {
                LOG_INFO(response);        
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong while handle_tasks_reset: " + std::string(e.what()));      
        }
    }

    void handle_remove_readonly(const fs::path& path) {
        try {
            fs::permissions(path, fs::perms::owner_write, fs::perm_options::add);
            fs::remove_all(path);
        } catch (const fs::filesystem_error& e) {
            std::cerr << "Error removing " << path << ": " << e.what() << '\n';
        }
    }

    void extract_zip(const std::string& zip_path, const std::string& extract_dir) {
        int err = 0;
        zip* z = zip_open(zip_path.c_str(), 0, &err);
        if (!z) {
            std::cerr << "Error opening zip file\n";
            return;
        }

        fs::create_directories(extract_dir);
        
        zip_int64_t num_entries = zip_get_num_entries(z, 0);
        for (zip_int64_t i = 0; i < num_entries; i++) {
            const char* name = zip_get_name(z, i, 0);
            if (!name) continue;

            fs::path full_path = fs::path(extract_dir) / name;
            
            zip_file* f = zip_fopen_index(z, i, 0);
            if (!f) continue;

            std::ofstream out(full_path, std::ios::binary);
            char buf[4096];
            zip_int64_t n;
            while ((n = zip_fread(f, buf, sizeof(buf))) > 0) {
                out.write(buf, n);
            }
            zip_fclose(f);
            out.close();
        }
        zip_close(z);
    }
};

#endif // HTTPSESSION_H
