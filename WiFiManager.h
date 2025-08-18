#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <iostream>
#include "Configuration.h"
#include "HTTPSession.h"
#include <fstream>
#include <string>
#include <memory>
#include <csignal>
#include <vector>
#include <algorithm>
#include <thread>
#include <chrono>
#include <regex>
#include <stdexcept>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include "Logger.h"
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <jsoncpp/json/json.h> // Correct path for jsoncpp header
#include <iostream>
#include <regex>
#include <string>
#include <memory>
#include <future>

struct Connection {
    std::string uri;
    std::string ssid;
    std::string password;

    // Constructor to initialize Connection with ssid and password
    Connection(const std::string& uri, const std::string& ssid, const std::string& password)
        : uri(uri), ssid(ssid), password(password) {}
    Connection() = default;  // Default constructor
};

class WiFiManager {
public:
    WiFiManager(const std::string& wireless_interface, Configuration& _config, HTTPSession& _session)
        : wireless_interface(wireless_interface), current_network(""), current_hostname(""), force_connection(false), 
          _connected(false), _http_ok(false), _vpn_ok(false), vpn_process(-1), config(_config), session(_session) {
            LOG_INFO("WiFiManager Constructor");
        }

    void init() {
        connections.clear();
        current_network.clear();
        current_hostname.clear();
        force_connection = false;
        _connected = false;
        _http_ok = false;
        _vpn_ok = false;
        vpn_process = -1;
        // if (!check_wifi(1)) {
        //     enable_wifi();
        // }
    }

    int run() {
        try {
            LOG_INFO("WiFiManager run method started");
            bool connected = isConnected();
            if (connected != _connected) {            
                _connected = connected;
                bool _auth = authenticationAndSetupProcedure();
                if (_auth)
                    return runConnected();
                else
                    return 0;
            } else if (force_connection) {
                LOG_INFO("Not connected or force connection requested. Trying to connect.");
                tryConnect();
                return 0;
            }
            return 0;
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong in run WiFiManager: " + std::string(e.what()));      
            return 0; // Return -1 if volume level is not found
        }
    }

    void setConnections(const Json::Value& conn, bool _force_connection = false) {
        try {
            connections.clear();
            for (const auto& connection : conn) {
                std::string ssid = connection["ssid"].asString();
                std::string password = connection["password"].asString();
                std::string uri = connection["uri"].asString();
                connections.emplace_back(uri, ssid, password); // Correctly construct the Connection with two arguments
            }
            if (_force_connection) {
                tryConnect();
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong in setConnections  WiFiManager: " + std::string(e.what())); 
        }
    }

    void setForceConnection(bool force) {
        force_connection = force;
    }

    void scan() {
        try {
            std::stringstream command;
            command << "iw " << wireless_interface << " scan";
            FILE* pipe = popen(command.str().c_str(), "r");
            if (!pipe) {
                LOG_ERROR("EFailed to execute scan command");
                throw std::runtime_error("Failed to execute scan command");
            }

            std::string output;
            char buffer[128];
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                output += buffer;
            }
            pclose(pipe);

            std::regex ssid_regex("\\tSSID: (.*)");
            std::sregex_iterator begin(output.begin(), output.end(), ssid_regex), end;
            networks.clear();
            for (auto it = begin; it != end; ++it) {
                networks.push_back(it->str(1));
            }

            LOG_INFO("[WIFI] Available networks: " + output);
        } catch (const std::exception& e) {
            LOG_ERROR("[WIFI] Error while scanning: " + std::string(e.what()));
        }
    }

    void stopVpn() {
        try {
            // Search for the OpenVPN process
            std::string result = exec_command("pgrep openvpn");
            std::istringstream iss(result);
            std::string line;
            std::vector<pid_t> pids;

            // Read the PIDs line by line
            while (std::getline(iss, line)) {
                try {
                    pid_t pid = std::stoi(line);
                    if (pid > 0) {
                        pids.push_back(pid);
                    }
                } catch (const std::exception& e) {
                    LOG_ERROR("Error parsing process ID: " + std::string(e.what()));
                }
            }

            // If no PIDs found, log and return
            if (pids.empty()) {
                LOG_INFO("No OpenVPN process running.");
                return;
            }

            // Attempt to kill each PID found
            for (const auto& pid : pids) {
                // Verify if the process still exists before attempting to kill it
                std::string proc_path = "/proc/" + std::to_string(pid);
                if (access(proc_path.c_str(), F_OK) != 0) {
                    LOG_ERROR("OpenVPN process (PID: " + std::to_string(pid) + ") no longer exists.");
                    continue;
                }

                // Send SIGTERM to the process
                std::stringstream pkill_cmd;
                pkill_cmd << "sudo kill -SIGTERM " << pid;
                int ret_code = system(pkill_cmd.str().c_str());
                if (ret_code == 0) {
                    LOG_INFO("OpenVPN process (PID: " + std::to_string(pid) + ") terminated successfully.");
                } else {
                    LOG_ERROR("Failed to terminate OpenVPN process (PID: " + std::to_string(pid) + ").");
                }
            }

        } catch (const std::exception& e) {
            LOG_ERROR("Error stopping VPN: " + std::string(e.what()));
        }
    }

    void updateVpnPerformance(const std::string& file_path) {
        try {
            std::vector<std::string> lines;
            bool tun_mtu_found = false;
            bool mssfix_found = false;
            bool keepalive_found = false;
            // bool fragment_found = false;
            // bool remote_found = false;
            // bool compress_found = false;
            // bool sndbuf_found = false;
            // bool rcvbuf_found = false;

    
            // Read existing config
            std::ifstream in(file_path);
            std::string line;
            
            while (std::getline(in, line)) {
                // Check and replace existing directives
                if (line.find("tun-mtu") != std::string::npos) {
                    line = "tun-mtu 1500";
                    tun_mtu_found = true;
                }
                else if (line.find("mssfix") != std::string::npos) {
                    line = "mssfix 1450";
                    mssfix_found = true;
                }
                else if (line.find("keepalive") != std::string::npos) {
                    line = "keepalive 10 60";
                    keepalive_found = true;
                }
                // else if (line.find("fragment") != std::string::npos) {
                //     line = "fragment 1400";
                //     fragment_found = true;
                // }
                // else if (line.find("remote") != std::string::npos) {
                //     line = line + "\n proto udp";
                //     remote_found = true;
                // }
                // else if (line.find("compress") != std::string::npos || 
                //          line.find("comp-lzo") != std::string::npos) {
                //     line = "compress lz4";
                //     compress_found = true;
                // }
                // else if (line.find("sndbuf") != std::string::npos) {
                //     line = "sndbuf 393216";
                //     sndbuf_found = true;
                // }
                // else if (line.find("rcvbuf") != std::string::npos) {
                //     line = "rcvbuf 393216";
                //     rcvbuf_found = true;
                // }
    
                lines.push_back(line);
            }
            in.close();
    
            // Add missing directives in the correct section
            bool in_speed_section = false;
            bool in_connection_main = false;
            for (auto& l : lines) {
                if (l.find("# speed session") != std::string::npos) {
                    in_speed_section = true;
                    continue;
                }                
                if (in_speed_section) {
                    if (!tun_mtu_found && l.empty()) {
                        l = "tun-mtu 1500\n" + l;
                        tun_mtu_found = true;
                    }
                    if (!mssfix_found && l.empty()) {
                        l = "mssfix 1360\n" + l;
                        mssfix_found = true;
                    }
                    // if (!fragment_found && l.empty()) {
                    //     l = "fragment 1400\n" + l;
                    //     fragment_found = true;
                    // }
                    // if (!sndbuf_found && l.empty()) {
                    //     l = "sndbuf 393216\n" + l;
                    //     sndbuf_found = true;
                    // }
                    // if (!rcvbuf_found && l.empty()) {
                    //     l = "rcvbuf 393216\n" + l;
                    //     rcvbuf_found = true;
                    // }
                }
                if (l.find("# connection main") != std::string::npos) {
                    in_connection_main = true;
                    continue;
                }                
                if (in_connection_main) {
                    if (!keepalive_found && l.empty()) {
                        l = "keepalive 10 60\n" + l;
                        keepalive_found = true;
                    }
                }

            }
    
            // Add compress if missing (after security section)
            // if (!compress_found) {
            //     auto it = std::find_if(lines.begin(), lines.end(), 
            //         [](const std::string& s) { return s.find("cipher none") != std::string::npos; });
                
            //     if (it != lines.end()) {
            //         lines.insert(it + 1, "compress lz4");
            //     }
            // }
    
            // Write modified config
            std::ofstream out(file_path);
            for (const auto& l : lines) {
                out << l << "\n";
            }
            out.close();
    
            LOG_INFO("Added performance parameters to VPN config");
        }
        catch (const std::exception& e) {
            LOG_ERROR("Performance config update failed: " + std::string(e.what()));
        }
    }

    void sedVpn(const std::string& file_path) {
        try {
            updateVpnPerformance(file_path);
            std::stringstream command;
            command << "sed -i '/remote /c\\remote " << current_hostname << " 1194 tcp4' " << file_path;
            int ret_code = system(command.str().c_str());
            if (ret_code != 0) {
                throw std::runtime_error("Failed to update VPN configuration file");
            }
            LOG_INFO("Updated remote line in " + file_path + " successfully.");
        } catch (const std::exception& e) {
            LOG_ERROR("Something went wrong while editing the VPN configuration file: " + std::string(e.what()));
        }
    }

    bool startVpn(const std::string& file_path) {
        try {
            // // Update VPN configuration file
            // std::string command_user = "grep -qxF 'user nobody' " + file_path + " || echo 'user nobody' >> " + file_path;
            // std::system(command_user.c_str());

            // std::string command_group = "grep -qxF 'group nogroup' " + file_path + " || echo 'group nogroup' >> " + file_path;
            // std::system(command_group.c_str());

            // // Start OpenVPN process
            // std::string command = "sudo /usr/sbin/openvpn --config " + file_path + "&";

            // Ensure the script is executable
            std::string chmod_cmd = "chmod +x " + config.script_vpn;
            system(chmod_cmd.c_str());

            // Construct the command to run the script with sudo and the config file as an argument
            std::string command = "sudo " + config.script_vpn + " " + file_path + " &";
            vpn_output = std::make_shared<std::FILE*>(popen(command.c_str(), "r"));
            if (!vpn_output) {
                LOG_ERROR("Failed to start VPN process.");
                return false;
            }
            LOG_INFO("VPN started successfully.");

            // Start VPN listener thread and wait for it to complete
            std::promise<bool> initPromise;
            auto initPromisePtr = std::make_shared<std::promise<bool>>(std::move(initPromise));
            std::future<bool> initFuture = initPromisePtr->get_future();
            vpn_listener_thread = std::thread([this, initPromisePtr]() mutable {
                try {
                    vpnProcessListener(initPromisePtr);
                } catch (const std::exception& e) {
                    LOG_ERROR("Exception in vpnProcessListener: " + std::string(e.what()));
                    initPromisePtr->set_value(false); // Signal failure if exception occurs
                }
            });        

            // Wait for the listener to detect "Initialization Sequence Completed"
            LOG_INFO("Waiting for VPN initialization sequence to complete...");
            if (initFuture.wait_for(std::chrono::seconds(20)) == std::future_status::timeout) {
                LOG_ERROR("Timeout waiting for VPN initialization.");
                return false;
            }
            else {
                // bool success = initFuture.get();  // Will wait until vpnProcessListener sets the value
                vpn_listener_thread.join(); // Ensure the thread is finished before returning            
                LOG_INFO("VPN initialization sequence completed with status: true");
                return true;
            }

        } catch (const std::exception& e) {
            LOG_ERROR("An error occurred while starting VPN: " + std::string(e.what()));
            return false;
        }
    }

    void vpnProcessListener(std::shared_ptr<std::promise<bool>> initPromise) {
        try {
            LOG_INFO("[VPN] Starting vpn_process_listener");
            bool h_authenticated = false;

            char buffer[256];
            std::string line;
            std::regex ip_regex("/sbin/ip addr add dev tun0 ([^/]+)/24");

            while (fgets(buffer, sizeof(buffer), *vpn_output.get()) != nullptr && line.find("Initialization Sequence Completed") == std::string::npos) {
                line = buffer;
                line = line.substr(0, line.find_last_not_of("\n\r") + 1); // Remove trailing newline

                // LOG_INFO("[VPN] " + line);

                if (line.find("Initialization Sequence Completed") != std::string::npos) {
                    LOG_INFO("[VPN] Initialization Sequence Completed");
                    LOG_INFO("config.client_address "+ config.client_address);
                    session.record_ip(config.client_address);
                    LOG_INFO("h_authenticated");
                    h_authenticated = true;
                    initPromise->set_value(true); // Signal success
                    break;
                } else if (line.find("/sbin/ip addr add dev tun0") != std::string::npos) {
                    std::smatch match;
                    std::string found = "127.0.0.1";

                    if (std::regex_search(line, match, ip_regex) && match.size() > 1) {
                        found = match.str(1);
                    }
                    LOG_INFO("[VPN] New address " + found);
                    // Update audio incoming pipeline
                    config.client_address = found;
                }        
            }
            if (!h_authenticated) {
                LOG_INFO("[VPN] Initialization failed or process exited prematurely");
                initPromise->set_value(false); // Signal failure if not authenticated
            }

            pclose(*vpn_output.get());
        } catch (const std::exception& e) {
            LOG_ERROR("An error occurred while vpnProcessListener : " + std::string(e.what()));
        }
    }

    void disable_wifi() {
        try {
            exec_command("sudo nmcli radio wifi off 2>&1");
            LOG_INFO("WIFI Disabled successfully.");
        } catch (const std::exception& e) {
            LOG_ERROR("[WIFI] Error while disabling wifi: " + std::string(e.what()));
        }
    }

    void enable_wifi() {
        try {
            exec_command("sudo nmcli radio wifi on 2>&1");
            LOG_INFO("WIFI Enabled successfully.");
        } catch (const std::exception& e) {
            LOG_ERROR("[WIFI] Error while enabling wifi: " + std::string(e.what()));
        }
    }    

    int check_wifi(const int _max_attempts = 30) {
        try {
            int Wconnected = 2;
            int attempts = 0;
            while (attempts++ < _max_attempts) {
                std::string status = exec_command("nmcli device status 2>&1");
                Wconnected = isWlan0Connected(status);                
                if (Wconnected == 0) {
                    LOG_INFO("WIFI Connected successfully.");
                    return 0;
                }                    
                else if (Wconnected == 1)  {         
                    LOG_INFO("WIFI NOT Connected.");
                    return 1;
                }
                else if (Wconnected == 2)  {         
                    LOG_INFO("WIFI UNAVAILABLE.");
                    return 2;
                }
                else if (Wconnected == 3)  {         
                    LOG_INFO("WIFI connecting (configuring).");
                    attempts -= 1;
              }
            } 
            
        } catch (const std::exception& e) {
            LOG_ERROR("[WIFI] Error while checking wifi: " + std::string(e.what()));
            return 2;
        }
        return 2;
    }

private:
    std::string wireless_interface;
    std::string current_network;
    std::string current_hostname;
    bool force_connection;
    bool _connected;
    bool _http_ok;
    bool _vpn_ok;
    pid_t vpn_process;
    std::shared_ptr<std::FILE*> vpn_output;
    std::thread vpn_listener_thread;
    std::vector<Connection> connections;
    std::vector<std::string> networks;
    Configuration& config;
    HTTPSession& session;

    int isWlan0Connected(const std::string& statusOutput) {
        std::istringstream iss(statusOutput);
        std::string line;
        
        while (std::getline(iss, line)) {
            if (line.find("DEVICE") != std::string::npos) {
                continue;
            }
            
            std::istringstream lineStream(line);
            std::string device, type, state, connection;
            lineStream >> device >> type >> state >> connection;
            
            if (device == "wlan0") {
                LOG_INFO("state  " + state);
                if (state == "connected") {
                    return 0;
                }
                else if (state == "disconnected") {
                    return 1;
                }
                else if (state == "unavailable") {
                    return 2;
                }
                else if (state == "connecting"){
                    return 3;
                }
                else if (state == "configuring"){
                    return 3;
                }
            }
        }
        return 2;
    }

    bool isConnected() {
        try {
            std::stringstream command;
            command << "iw dev " << wireless_interface << " link";
            FILE* pipe = popen(command.str().c_str(), "r");
            if (!pipe) {
                LOG_ERROR("Failed to execute command.");
                return false;
            }

            char buffer[128];
            std::string output;
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                output += buffer;
            }
            pclose(pipe);

            std::smatch match;
            std::regex ssid_regex("SSID: (.+)");
            if (std::regex_search(output, match, ssid_regex) && match.size() > 1) {
                current_network = match.str(1);
                LOG_INFO("Current Network SSID: " + current_network);
                for (const auto& conn : connections) {
                    if (conn.ssid == current_network) {
                        current_hostname = conn.uri;
                        config.updateApiUri(current_hostname);  // Implement API URI update logic here
                        return true;
                    }
                }
            }
        } catch (const std::exception& e) {
            LOG_ERROR("[WIFI] Not connected: " + std::string(e.what()));
            current_network.clear();
            current_hostname.clear();
        }
        return false;
    }

    void tryConnect() {
        for (const auto& conn : connections) {
            if (connect(conn.ssid, conn.password)) {
                std::this_thread::sleep_for(std::chrono::seconds(5));
                
                // Verify connection to target network
                if (isConnected() && current_network == conn.ssid) {
                    force_connection = false;
                    LOG_INFO("Successfully connected to: " + conn.ssid);
                    return;
                }
            }
        }
        LOG_ERROR("[WIFI] Failed to connect to any known network");
    }

    bool connect(const std::string& ssid, const std::string& password) {
        try {
            // Forcefully delete ALL Wi-Fi profiles (more robust method)
            std::string delete_all_cmd = "sudo rm -f /etc/NetworkManager/system-connections/*.nmconnection";
            system(delete_all_cmd.c_str());
            
            // Reload NetworkManager to apply changes
            system("sudo nmcli connection reload");

            // Optional: Wait a moment for changes to take effect
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            // Create new connection profile
            std::string create_cmd = "sudo nmcli con add type wifi con-name \"" + ssid + 
                                    "\" ifname " + wireless_interface + 
                                    " ssid \"" + ssid + "\"";
            if (system(create_cmd.c_str()) != 0) {
                LOG_ERROR("Failed to create connection profile");
                return false;
            }
    
            // Set security parameters
            std::string security_cmd = "sudo nmcli con modify \"" + ssid + 
                                    "\" wifi-sec.key-mgmt wpa-psk wifi-sec.psk \"" + password + "\"";
            if (system(security_cmd.c_str()) != 0) {
                LOG_ERROR("Failed to set security parameters");
                return false;
            }
    
            // Set priority and autoconnect
            system(("sudo nmcli con modify \"" + ssid + "\" connection.autoconnect-priority 100").c_str());
            system(("sudo nmcli con modify \"" + ssid + "\" connection.autoconnect yes").c_str());

            // Force connection (disconnect current first)
            system(("sudo nmcli con down \"" + ssid + "\"").c_str());
            if (system(("sudo nmcli con up \"" + ssid + "\"").c_str()) != 0) {
                LOG_ERROR("Failed to activate connection");
                return false;
            }
    
            LOG_INFO("Successfully connected to: " + ssid);
            return true;
        } catch (const std::exception& e) {
            LOG_ERROR("Connection error: " + std::string(e.what()));
            return false;
        }
    }

    bool authenticationAndSetupProcedure() {
        LOG_INFO("Authentication and setup procedure started.");
        // Stop current VPN
        LOG_INFO("Stop Vpn.");
        stopVpn();
        LOG_INFO("Authenticate the helmet");
        if (session.authenticate()) {
            // Retrieve new certificate from server
            LOG_INFO("get the certificate");
            session.get_certificate(config.certificate_file);
            LOG_INFO("Prepare the sed command with proper escaping");
            sedVpn(config.certificate_file);    
            // Restart VPN
            // Start VPN and redirect output to write end of pipe
            LOG_INFO("[VPN] Start VPN process");
            // startVpn(config.certificate_file) ;     
            LOG_INFO("[VPN] Start VPN process listener");
            bool listener_status = startVpn(config.certificate_file);
            LOG_INFO("Authentication and setup procedure completed successfully");
            return listener_status;
        }
        else
            return false;
    }

    int runConnected() {
        if (_connected) {
            LOG_INFO("Connected. Checking HTTP and VPN status.");
            _http_ok = checkHttpAvailability(current_hostname);
            if (_http_ok) {
                LOG_INFO("HTTP connection established");
                _vpn_ok = checkVpnAvailability();
                if (_vpn_ok) {
                    LOG_INFO("VPN connection established");                    
                    // Update UI or perform connected operations
                    if (current_network.rfind("SH_LP_", 0) == 0)
                        return 1;
                    else
                        return 2;
                } else {
                    LOG_ERROR("VPN connection failed");                    
                    // Update UI for failed VPN
                    if (current_network.rfind("SH_LP_", 0) == 0)
                        return 3;
                    else
                        return 4;
                }
            } else {
                LOG_ERROR("HTTP connection failed");
                // Update UI for failed HTTP connection
                if (current_network.rfind("SH_LP_", 0) == 0)
                    return 5;
                else
                    return 6;
            }
        } else {
            LOG_ERROR("Not connected");
            // Update UI for not connected
            if (current_network.rfind("SH_LP_", 0) == 0)
                return 7;
            else
                return 8;
        }
    }

    bool checkHttpAvailability(const std::string& hostname) {
        // Placeholder for checking HTTP availability logic
        LOG_INFO("Checking HTTP availability for " + hostname);
        session.is_http_available(hostname);
        return true;  // Replace with actual logic
    }

    bool checkVpnAvailability() {
        // Placeholder for checking VPN availability logic
        LOG_INFO("Checking VPN availability");
        session.is_vpn_available();
        return true;  // Replace with actual logic
    }

    std::string exec_command(const std::string& cmd) {
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            LOG_ERROR("popen() failed!");
            throw std::runtime_error("popen() failed!");
        }

        char buffer[128];
        std::string result;
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }
        pclose(pipe);
        return result;
    }
    
};

#endif // WIFI_MANAGER_H
