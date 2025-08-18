#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <boost/asio.hpp>

using namespace std;
using namespace boost::asio;
using namespace boost::system;

// Global variables
mutex data_mutex;
map<int, int> gps_satellites_available;
map<int, int> glonass_satellites_available;
string status = "Unknown";
string mode = "Unknown";

// File paths
const string CSV_PATH = "home/x_user/my_camera_project/data/gps.csv";
const string LOG_PATH = "home/x_user/my_camera_project/data/gps.log";

// Logging toggle
const bool LOGGING = true;

// Serial port
io_context io;
serial_port sp(io);

// Helper function to calculate average
double average(const map<int, int>& m) {
    if (m.empty()) return 0.0;
    int sum = 0;
    for (auto& p : m) sum += p.second;
    return static_cast<double>(sum) / m.size();
}

// Helper function to get max
int max_value(const map<int, int>& m) {
    if (m.empty()) return 0;
    int max_val = m.begin()->second;
    for (auto& p : m) if (p.second > max_val) max_val = p.second;
    return max_val;
}

// Helper to sort values descending
vector<int> sorted_values_desc(const map<int, int>& m) {
    vector<int> result;
    for (auto& p : m) result.push_back(p.second);
    sort(result.rbegin(), result.rend());
    return result;
}

// Parse GGA sentence
void parse_gga(const vector<string>& tokens) {
    if (tokens.size() < 8) return;

    double latitude = stod(tokens[2]);
    double longitude = stod(tokens[4]);

    // Convert latitude from DDMM.MMMM to decimal
    double lat_deg = floor(latitude / 100);
    double lat_min = latitude - lat_deg * 100;
    latitude = lat_deg + lat_min / 60;

    // Convert longitude from DDDMM.MMMM to decimal
    double lon_deg = floor(longitude / 100);
    double lon_min = longitude - lon_deg * 100;
    longitude = lon_deg + lon_min / 60;

    if (tokens[3] == "S") latitude = -latitude;
    if (tokens[5] == "W") longitude = -longitude;

    string line = to_string(latitude) + "," + to_string(longitude);

    // Write to CSV
    ofstream csv(CSV_PATH);
    if (csv.is_open()) {
        csv << line << endl;
        csv.close();
    }

    // Log output
    if (LOGGING) {
        ofstream log(LOG_PATH);
        if (log.is_open()) {
            log << "Time: " << tokens[1] << "\n";
            log << "Coordinates: " << latitude << ", " << longitude << "\n";
            log << "Used satellites: " << tokens[7] << "\n";
            log << "Status: " << status << "\n";
            log << "Mode: " << mode << "\n";

            if (!gps_satellites_available.empty()) {
                double avg_gps = average(gps_satellites_available);
                int max_gps = max_value(gps_satellites_available);
                vector<int> snr_gps = sorted_values_desc(gps_satellites_available);
                stringstream ss;
                for (int s : snr_gps) ss << s << " ";
                log << "GPS      " << gps_satellites_available.size() << ", AVG SNR " << avg_gps << " db, MAX SNR " << max_gps << " db, SNR [" << ss.str() << "]\n";
            }

            if (!glonass_satellites_available.empty()) {
                double avg_glo = average(glonass_satellites_available);
                int max_glo = max_value(glonass_satellites_available);
                vector<int> snr_glo = sorted_values_desc(glonass_satellites_available);
                stringstream ss;
                for (int s : snr_glo) ss << s << " ";
                log << "GLONASS  " << glonass_satellites_available.size() << ", AVG SNR " << avg_glo << " db, MAX SNR " << max_glo << " db, SNR [" << ss.str() << "]\n";
            }

            log.close();
        }
    }
}

// Parse GLL or GLGSV sentences
void parse_gll(const vector<string>& tokens) {
    if (tokens.size() >= 3) {
        if (tokens[2] == "A") {
            status = "Data valid";
        } else {
            status = "Data not valid";
        }
    }

    if (tokens.size() >= 5) {
        if (tokens[4] == "A") {
            mode = "Autonomous";
        } else if (tokens[4] == "D") {
            mode = "DGPS";
        } else {
            mode = "Unknown";
        }
    }
}

// Parse GPGSV (GPS satellite info)
void parse_gpgsv(const vector<string>& tokens) {
    static bool reset = true;
    if (tokens.size() < 4) return;

    int msg_num = 1;
    try {
        msg_num = stoi(tokens[1]); // Message number
    } catch (...) {
        return; // Invalid message number, skip
    }

    if (msg_num == 1) {
        lock_guard<mutex> lock(data_mutex);
        gps_satellites_available.clear();
        reset = false;
    }

    int count = (tokens.size() - 4) / 4;
    for (int i = 0; i < count && i < 4; ++i) {
        if (tokens.size() <= 4 + i * 4) continue;
        string prn_str = tokens[4 + i * 4];
        string snr_str = (tokens.size() > 7 + i * 4) ? tokens[7 + i * 4] : "";

        if (prn_str.empty()) continue;

        int prn = 0;
        int snr = 0;

        try {
            prn = stoi(prn_str);
        } catch (...) {
            continue; // Skip invalid PRN
        }

        try {
            snr = snr_str.empty() ? 0 : stoi(snr_str);
        } catch (...) {
            snr = 0; // Default to 0 on error
        }

        if (prn > 0 && snr >= 0) {
            lock_guard<mutex> lock(data_mutex);
            gps_satellites_available[prn] = snr;
        }
    }
}

void parse_satellite_data(const vector<string>& tokens, map<int, int>& satellites_map, const string& system_name) {
    if (tokens.size() < 4) return; // Ensure we have at least 4 tokens

    int msg_num = 1;
    int total_msgs = 1;

    try {
        if (!tokens[1].empty()) msg_num = stoi(tokens[1]);
        else return;
    } catch (...) {
        return; // Invalid message number
    }

    try {
        if (!tokens[2].empty()) total_msgs = stoi(tokens[2]);
        else return;
    } catch (...) {
        return; // Invalid total messages
    }

    if (msg_num == 1) {
        lock_guard<mutex> lock(data_mutex);
        satellites_map.clear();
    }

    int count = (tokens.size() - 4) / 4;
    for (int i = 0; i < count && i < 4; ++i) {
        if (tokens.size() <= 4 + i * 4) continue;

        string prn_str = tokens[4 + i * 4];
        string snr_str = (tokens.size() > 7 + i * 4) ? tokens[7 + i * 4] : "";

        if (prn_str.empty()) continue;

        int prn = 0;
        int snr = 0;

        try {
            prn = stoi(prn_str);
        } catch (...) {
            continue;
        }

        try {
            snr = snr_str.empty() ? 0 : stoi(snr_str);
        } catch (...) {
            snr = 0;
        }

        if (prn > 0 && snr >= 0) {
            lock_guard<mutex> lock(data_mutex);
            satellites_map[prn] = snr;
        }
    }
}

// Parse GLGSV (GLONASS satellite info)
void parse_glgsa(const vector<string>& tokens) {
    static bool reset = true;
    if (tokens.size() < 4) return;

    int msg_num = 1;
    try {
        msg_num = stoi(tokens[1]); // Message number
    } catch (...) {
        return; // Invalid message number, skip
    }

    if (msg_num == 1) {
        lock_guard<mutex> lock(data_mutex);
        glonass_satellites_available.clear();
        reset = false;
    }

    int count = (tokens.size() - 4) / 4;
    for (int i = 0; i < count && i < 4; ++i) {
        if (tokens.size() <= 4 + i * 4) continue;
        string prn_str = tokens[4 + i * 4];
        string snr_str = (tokens.size() > 7 + i * 4) ? tokens[7 + i * 4] : "";

        if (prn_str.empty()) continue;

        int prn = 0;
        int snr = 0;

        try {
            prn = stoi(prn_str);
        } catch (...) {
            continue; // Skip invalid PRN
        }

        try {
            snr = snr_str.empty() ? 0 : stoi(snr_str);
        } catch (...) {
            snr = 0; // Default to 0 on error
        }

        if (prn > 0 && snr >= 0) {
            lock_guard<mutex> lock(data_mutex);
            glonass_satellites_available[prn] = snr;
        }
    }
}

// Tokenize NMEA sentence
vector<string> tokenize(const string& s) {
    vector<string> tokens;
    size_t start = 0, end = 0;
    while ((end = s.find(',', start)) != string::npos) {
        tokens.push_back(s.substr(start, end - start));
        start = end + 1;
    }
    tokens.push_back(s.substr(start));
    return tokens;
}

// Main receive loop
void receive_loop() {
    char buffer[1024];
    while (true) {
        try {
            size_t len = sp.read_some(boost::asio::buffer(buffer));
            string raw(buffer, len);
            istringstream iss(raw);
            string line;

            while (getline(iss, line, '\n')) {
                if (line.empty() || line[0] != '$') continue;

                vector<string> tokens = tokenize(line.substr(1)); // Skip $
                string type = tokens[0];
                // cout << line << endl;
                if (type == "GPGGA") {
                    parse_gga(tokens);
                } else if (type == "GPGLL") {
                    parse_gll(tokens);
                } else if (type == "GPGSV") {
                    // parse_gpgsv(tokens);
                    parse_satellite_data(tokens, gps_satellites_available, "GPS");
                } else if (type == "GLGSV") {
                    parse_satellite_data(tokens, glonass_satellites_available, "GLONASS");
                    // parse_glgsa(tokens);
                }
            }
        } catch (const boost::system::system_error&) {
            cerr << "Error reading from serial port" << endl;
        }
    }
}

int main() {
    try {
        // Open serial port
        sp.open("/dev/ttymxc1");
        sp.set_option(serial_port_base::baud_rate(115200));
        sp.set_option(serial_port_base::character_size(8));
        sp.set_option(serial_port_base::stop_bits(serial_port_base::stop_bits::one));
        sp.set_option(serial_port_base::parity(serial_port_base::parity::none));
        sp.set_option(serial_port_base::flow_control(serial_port_base::flow_control::none));

        cout << "Serial port opened successfully." << endl;

        // Start background thread
        thread receiver(receive_loop);
        receiver.detach();

        // Keep main alive
        while (true) {
            this_thread::sleep_for(std::chrono::milliseconds(500));
        }

    } catch (const exception& e) {
        cerr << "Exception: " << e.what() << endl;
        return 1;
    }

    return 0;
}
//g++ -std=c++17 -pthread gps_parser.cpp -o gps_parser -lboost_system -lboost_thread
//./gps_parser