#include <iostream>
#include <fstream>
#include <iomanip>
#include <ctime>
#include <vector>
#include <cstring>
#include <cstdint>

// Constants
constexpr uint32_t SECONDS_PER_HOUR = 3600;
constexpr uint32_t GPS_OFFSET_SECONDS = 315964786;
constexpr uint32_t HOURS_PER_WEEK = 168;

// Convert GPS hour to UTC time string
std::string Convert2UTC(int32_t GPSHour) {
    time_t gpsTime = static_cast<time_t>(GPSHour) * SECONDS_PER_HOUR + GPS_OFFSET_SECONDS;
    struct tm* tm = gmtime(&gpsTime);
    char buffer[80];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm);
    return std::string(buffer);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <EPO_File>" << std::endl;
        return 1;
    }

    const std::string inputFile = argv[1];
    std::ifstream fi(inputFile, std::ios::binary);
    if (!fi) {
        std::cerr << "Error: Could not open file " << inputFile << std::endl;
        return 1;
    }

    // Read the first 75 bytes to determine EPO type
    std::vector<uint8_t> header(75);
    if (!fi.read(reinterpret_cast<char*>(header.data()), header.size())) {
        std::cerr << "Error reading header" << std::endl;
        return 1;
    }

    // Determine EPO type based on header
    size_t EPO_SET_SIZE = 0;
    if (memcmp(header.data(), header.data() + 60, 3) == 0) {
        std::cout << "Opening EPO Type I file" << std::endl;
        EPO_SET_SIZE = 1920;
    } else if (memcmp(header.data(), header.data() + 72, 3) == 0) {
        std::cout << "Opening EPO Type II file" << std::endl;
        EPO_SET_SIZE = 2304;
    } else {
        std::cerr << "Error: Unknown file type." << std::endl;
        return 1;
    }

    // Reset file position to beginning
    fi.clear();
    fi.seekg(0, std::ios::beg);

    // Read and process EPO sets
    int sets = 0;
    int32_t start = 0;
    int32_t last_epo_start = 0;

    while (true) {
        std::vector<uint8_t> epo_set(EPO_SET_SIZE);
        fi.read(reinterpret_cast<char*>(epo_set.data()), epo_set.size());
        
        // Check if we read a full set
        if (fi.gcount() != static_cast<std::streamsize>(EPO_SET_SIZE)) {
            break;
        }
        sets++;

        // Extract GPS hour from first 3 bytes (little-endian)
        int32_t epo_start = static_cast<int32_t>(epo_set[0]) |
                           (static_cast<int32_t>(epo_set[1]) << 8) |
                           (static_cast<int32_t>(epo_set[2]) << 16);

        // Sign-extend 24-bit value to 32-bit
        if (epo_start & 0x800000) {
            epo_start |= 0xFF000000;
        }

        if (sets == 1) {
            start = epo_start;
        }
        last_epo_start = epo_start;

        // Calculate week and hour
        uint32_t gps_week = epo_start / HOURS_PER_WEEK;
        uint32_t hour_in_week = epo_start % HOURS_PER_WEEK;

        // Print set information
        std::cout << "Set: " << std::setw(4) << sets << ".  "
                  << "GPS Wk: " << std::setw(4) << gps_week << "  "
                  << "Hr: " << std::setw(3) << hour_in_week << "  "
                  << "Sec: " << std::setw(6) << (hour_in_week * SECONDS_PER_HOUR) << "  "
                  << Convert2UTC(epo_start) << " to "
                  << Convert2UTC(epo_start + 6) << " UTC" << std::endl;
    }

    // Print summary
    if (sets > 0) {
        std::cout << std::setw(4) << sets << " EPO sets.  Valid from "
                  << Convert2UTC(start) << " to "
                  << Convert2UTC(last_epo_start + 6) << " UTC" << std::endl;
    } else {
        std::cout << "No valid EPO sets found" << std::endl;
    }

    return 0;
}