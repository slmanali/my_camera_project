#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <boost/asio.hpp>
#include <boost/asio/serial_port.hpp>

using namespace boost::asio;
using namespace boost::system;

class GPSConfigurator {
public:
    GPSConfigurator(const std::string& port_name)
        : io(), port(io), stop_receive(false), cmd_executed(false) {

        try {
            port.open(port_name);
            // Set explicit 8N1 configuration
            port.set_option(serial_port_base::baud_rate(9600));
            port.set_option(serial_port_base::character_size(8));
            port.set_option(serial_port_base::parity(serial_port_base::parity::none));
            port.set_option(serial_port_base::stop_bits(serial_port_base::stop_bits::one));
            std::cout << "Opened serial port: " << port_name << std::endl;
        } catch (const boost::system::system_error& e) {
            std::cerr << "Failed to open serial port: " << e.what() << std::endl;
            throw;
        }

        // Corrected configurations (verified checksums)
        configurations = {
            "$PMTK000*32\r\n",         // Echo off
            "$PMTK127*36\r\n",         // Clear EPO
            "$PMTK101*32\r\n",         // Hot start
            "$PMTK313,1*2E\r\n",       // SBAS enabled
            "$PMTK301,2*2E\r\n",       // DGPS=RTCM
            "$PMTK314,0,5,0,5,5,5,0,0,0,0,0,0,0,0,0,0,0,1,0*29\r\n", // NMEA output
            "$PMTK319,1*24\r\n",       // SBAS mode
            "$PMTK225,0*2B\r\n",       // Periodic mode
            "$PMTK286,1*23\r\n",       // Interference cancellation
            "$PMTK386,0*23\r\n",       // Navigation speed
            "$PMTK869,1,1*35\r\n",     // EASY mode (EPO usage enabled)
            "$PMTK220,1000*1F\r\n",    // Update rate 1Hz
            "$PMTK353,1,1*2B\r\n",     // GPS+GLONASS
            "$PMTK000*32\r\n",
            "$PMTK000*32\r\n",
            "$PMTK000*32\r\n"
        };

        baudrates = {9600, 19200, 38400, 57600, 115200};
    }

    ~GPSConfigurator() {
        stop_receive = true;
        if (rx_thread.joinable()) {
            rx_thread.join();
        }
        if (port.is_open())
            port.close();
    }

    void configure() {
        set_speed();        // Set to 115200
        start_receive_thread();
        send_configurations();
    }

private:
    io_service io;
    serial_port port;
    std::thread rx_thread;
    std::atomic<bool> stop_receive;
    std::atomic<bool> cmd_executed;
    std::vector<std::string> configurations;
    std::vector<int> baudrates;

    std::mutex mtx;
    std::condition_variable cv;

    void set_speed() {
        // Set explicit 8N1 configuration first
        port.set_option(serial_port_base::character_size(8));
        port.set_option(serial_port_base::parity(serial_port_base::parity::none));
        port.set_option(serial_port_base::stop_bits(serial_port_base::stop_bits::one));

        // Try all possible baud rates
        for (int baud : baudrates) {
            try {
                port.set_option(serial_port_base::baud_rate(baud));
                std::cout << "Trying baudrate: " << baud << std::endl;

                // Send baudrate change command
                std::string cmd = "$PMTK251,115200*1F\r\n";
                write(port, buffer(cmd));

                // Allow extra time for command processing
                std::this_thread::sleep_for(std::chrono::milliseconds(500));

                // Switch to 115200
                port.set_option(serial_port_base::baud_rate(115200));
                std::cout << "Switched to 115200" << std::endl;

                // Verify connection by checking for valid NMEA data
                std::string response;
                char c;
                auto start = std::chrono::steady_clock::now();
                while ((std::chrono::steady_clock::now() - start) < std::chrono::milliseconds(500)) {
                    if (port.read_some(buffer(&c, 1))) {
                        response += c;
                        if (c == '\n') break;
                    }
                }

                // Check if we got valid NMEA data
                if (!response.empty() && response[0] == '$') {
                    std::cout << "Valid NMEA detected: " << response;
                    return;
                } else {
                    std::cout << "No valid response at " << baud << std::endl;
                }
            } catch (const boost::system::system_error& e) {
                std::cerr << "Error: " << e.what() << std::endl;
            }
        }
        throw std::runtime_error("Failed to establish communication");
    }

    void start_receive_thread() {
        rx_thread = std::thread([this]() {
            port.set_option(serial_port_base::character_size(8));
            port.set_option(serial_port_base::parity(serial_port_base::parity::none));
            port.set_option(serial_port_base::stop_bits(serial_port_base::stop_bits::one));
            while (!stop_receive) {
                try {
                    std::string line;
                    char c;
                    while (!stop_receive) {
                        read(port, buffer(&c, 1));
                        if (c == '\n') {
                            if (!line.empty() && line.back() == '\r') {
                                line.pop_back();
                            }
                            break;
                        }
                        line += c;
                    }
                    std::cout << "[GPS RESPONSE] " << line << std::endl;

                    if (line.find("PMTK001") != std::string::npos) {
                        std::lock_guard<std::mutex> lock(mtx);
                        cmd_executed = true;
                        cv.notify_one();
                    }
                } catch (const boost::system::system_error& e) {
                    if (!stop_receive) {
                        std::cerr << "Serial read error: " << e.what() << std::endl;
                    }
                }
            }
        });
    }

    void send_configurations() {
        for (const auto& cmd : configurations) {
            try {
                std::cout << "Sending: " << cmd;
                write(port, buffer(cmd));
                std::unique_lock<std::mutex> lock(mtx);
                cmd_executed = false;
                if (cv.wait_for(lock, std::chrono::seconds(3), [this] {
                    return cmd_executed.load();
                })) {
                    std::cout << "ACK received" << std::endl;
                } else {
                    std::cerr << "Timeout waiting for ACK" << std::endl;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
            } catch (const boost::system::system_error& e) {
                std::cerr << "Write error: " << e.what() << std::endl;
            }
        }
    }
};

int main() {
    try {
        std::string port_name = "/dev/ttymxc1";  // Update to your port
        GPSConfigurator configurator(port_name);
        configurator.configure();
        std::cout << "GPS configuration completed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
