#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h> // for Sleep, usleep
#include <termios.h>
#include <fcntl.h>

// Function prototype to avoid implicit declaration warning
int gnss_bridge_put_data(uint8_t *buffer, int length);

#define GNSS_BINARY_PREAMBLE1     (0x04)
#define GNSS_BINARY_PREAMBLE2     (0x24)
#define GNSS_BINARY_ENDWORD1      (0xAA)
#define GNSS_BINARY_ENDWORD2      (0x44)
#define GNSS_BINARY_PREAMBLE_SIZE (2)
#define GNSS_BINARY_CHECKSUM_SIZE (1)
#define GNSS_BINARY_ENDWORD_SIZE  (2)
#define GNSS_BINARY_MESSAGE_ID_SIZE      (2)
#define GNSS_BINARY_PAYLOAD_LENGTH_SIZE  (2)
#define GNSS_BINARY_PAYLOAD_HEADER_SIZE  (GNSS_BINARY_MESSAGE_ID_SIZE + GNSS_BINARY_PAYLOAD_LENGTH_SIZE)

#define GNSS_EPO_DATA_SIZE        (72)
#define GNSS_EPO_START_END_SIZE   (1)
#define GNSS_EPO_GPS_SV           (32)
#define GNSS_EPO_GLONASS_SV       (24)

typedef enum {
    GNSS_EPO_MODE_GPS,
    GNSS_EPO_MODE_GLONASS,
    GNSS_EPO_MODE_GALILEO,
    GNSS_EPO_MODE_BEIDOU
} gnss_epo_mode_t;

typedef enum {
    GNSS_EPO_DATA_EPO_START = 1200,
    GNSS_EPO_DATA_EPO_DATA  = 1201,
    GNSS_EPO_DATA_EPO_END   = 1202
} gnss_epo_data_id_t;

typedef struct {
    uint16_t message_id;
    uint16_t data_size;
    uint8_t *data;
} gnss_binary_payload_t;

uint8_t gnss_binary_calculate_binary_checksum(const gnss_binary_payload_t *payload)
{
    uint8_t checksum = 0;
    uint8_t *pheader = (uint8_t*)payload;
    uint16_t i;

    for (i = 0; i < GNSS_BINARY_PAYLOAD_HEADER_SIZE; i++)
        checksum ^= *pheader++;

    for (i = 0; i < payload->data_size; i++)
        checksum ^= payload->data[i];

    return checksum;
}

int16_t gnss_binary_encode_binary_packet(uint8_t *buffer, uint16_t max_buffer_size, const gnss_binary_payload_t *payload)
{
    if (!buffer || !payload || !payload->data) return -1;
    uint16_t required_length = payload->data_size + GNSS_BINARY_PREAMBLE_SIZE +
        GNSS_BINARY_CHECKSUM_SIZE + GNSS_BINARY_ENDWORD_SIZE + GNSS_BINARY_PAYLOAD_HEADER_SIZE;
    if (max_buffer_size < required_length) return -1;

    memset(buffer, 0, max_buffer_size);

    buffer[0] = GNSS_BINARY_PREAMBLE1;
    buffer[1] = GNSS_BINARY_PREAMBLE2;
    memcpy(&buffer[2], payload, GNSS_BINARY_PAYLOAD_HEADER_SIZE);
    memcpy(&buffer[2 + GNSS_BINARY_PAYLOAD_HEADER_SIZE], payload->data, payload->data_size);

    uint8_t *p = &buffer[2 + GNSS_BINARY_PAYLOAD_HEADER_SIZE + payload->data_size];
    *p++ = gnss_binary_calculate_binary_checksum(payload);
    *p++ = GNSS_BINARY_ENDWORD1;
    *p   = GNSS_BINARY_ENDWORD2;

    return required_length;
}

typedef struct {
    FILE *file;
    int type;
} gnss_epo_data_t;

int gnss_epo_fopen(gnss_epo_data_t *data_p, const char* filename, int write)
{
    data_p->file = fopen(filename, write ? "wb" : "rb");
    data_p->type = write ? 1 : 0;
    return (data_p->file != NULL);
}

int gnss_epo_fread(gnss_epo_data_t *data_p, void *buf, int len)
{
    return fread(buf, 1, len, data_p->file);
}

void gnss_epo_fclose(gnss_epo_data_t *data_p)
{
    fclose(data_p->file);
}

int16_t gnss_epo_encode_binary(uint8_t *buffer, uint16_t max_buffer_size, uint8_t *temp_buffer, gnss_epo_data_id_t msg_id)
{
    gnss_binary_payload_t payload = {0};
    int16_t length = 0;
    payload.message_id = msg_id;
    if ((msg_id == GNSS_EPO_DATA_EPO_START) || (msg_id == GNSS_EPO_DATA_EPO_END)) {
        payload.data_size = GNSS_EPO_START_END_SIZE;
    } else if (msg_id == GNSS_EPO_DATA_EPO_DATA) {  // Fixed typo here
        payload.data_size = GNSS_EPO_DATA_SIZE;
    } else {
        printf("gnss_epo_encode_binary, msg_id wrong\n");
        return 0;
    }
    payload.data = temp_buffer;
    length = gnss_binary_encode_binary_packet(buffer, max_buffer_size, &payload);
    return length;
}

void gnss_epo_flash_aiding(gnss_epo_mode_t type)
{
    gnss_epo_data_t data;
    uint8_t buffer[100];
    uint8_t temp_buffer[GNSS_EPO_DATA_SIZE];
    const char* file_name = "/home/user/MTK14.EPO";
    uint8_t payload_type = 'G';
    int16_t length = 0;

    if (!gnss_epo_fopen(&data, file_name, 0)) {
        printf("Cannot open EPO file\n");
        return;
    }

    temp_buffer[0] = payload_type;
    length = gnss_epo_encode_binary(buffer, sizeof(buffer), temp_buffer, GNSS_EPO_DATA_EPO_START);
    gnss_bridge_put_data(buffer, length);
    usleep(10000);

    while (gnss_epo_fread(&data, temp_buffer, GNSS_EPO_DATA_SIZE)) {
        length = gnss_epo_encode_binary(buffer, sizeof(buffer), temp_buffer, GNSS_EPO_DATA_EPO_DATA);
        gnss_bridge_put_data(buffer, length);
        usleep(10000);
    }

    temp_buffer[0] = payload_type;
    length = gnss_epo_encode_binary(buffer, sizeof(buffer), temp_buffer, GNSS_EPO_DATA_EPO_END);
    gnss_bridge_put_data(buffer, length);

    printf("gnss_epo_flash_aiding finish: %d\n", type);
    gnss_epo_fclose(&data);
}

int serial_fd = -1;

int gnss_bridge_put_data(uint8_t *buffer, int length)
{
    if (serial_fd < 0) return -1;
    int written = write(serial_fd, buffer, length);
    if (written != length) {
        perror("Serial write");
    }
    tcdrain(serial_fd);
    return written;
}

int open_serial(const char* portname, int baudrate)
{
    serial_fd = open(portname, O_RDWR | O_NOCTTY | O_SYNC);
    if (serial_fd < 0) {
        perror("Open serial port");
        return -1;
    }
    struct termios tty;
    memset(&tty, 0, sizeof tty);
    if (tcgetattr(serial_fd, &tty) != 0) {
        perror("tcgetattr");
        close(serial_fd);
        serial_fd = -1;
        return -1;
    }
    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_iflag &= ~IGNBRK;
    tty.c_lflag = 0;
    tty.c_oflag = 0;
    tty.c_cc[VMIN]  = 1;
    tty.c_cc[VTIME] = 1;
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~(PARENB | PARODD);
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;
    if (tcsetattr(serial_fd, TCSANOW, &tty) != 0) {
        perror("tcsetattr");
        close(serial_fd);
        serial_fd = -1;
        return -1;
    }
    return 0;
}

int main()
{
    if (open_serial("/dev/ttymxc1", 115200) < 0) {
        printf("Failed to open serial\n");
        return 1;
    }
    gnss_epo_flash_aiding(GNSS_EPO_MODE_GPS);
    close(serial_fd);
    return 0;
}
// gcc -o epo_upload_demo epo_upload_demo.c
// ./epo_upload_demo