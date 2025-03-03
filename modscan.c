#include <modbus/modbus.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define DEVICE "/dev/ttyO1"
#define BAUDRATE 9600
#define HOST_IP_FILE "/etc/envision/info/host_ip"
#define PORT 12345

// Check serial port access
int check_serial_port(const char *port) {
    int fd = open(port, O_RDWR | O_NOCTTY);
    if (fd == -1) {
        fprintf(stderr, "Error: Cannot access %s: %s\n", port, strerror(errno));
        return -1;
    }
    close(fd);
    return 0;
}

// Read listener IP address from file
int read_host_ip(char *ip, size_t len) {
    FILE *file = fopen(HOST_IP_FILE, "r");
    if (file == NULL) {
        fprintf(stderr, "Error: Cannot open %s: %s\n", HOST_IP_FILE, strerror(errno));
        return -1;
    }
    if (fgets(ip, len, file) == NULL) {
        fprintf(stderr, "Error: Cannot read IP address from %s\n", HOST_IP_FILE);
        fclose(file);
        return -1;
    }
    // Remove newline character if present
    ip[strcspn(ip, "\n")] = 0;
    fclose(file);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s device.id address length modbus_point_type\n", argv[0]);
        return EXIT_FAILURE;
    }

    int device_id = atoi(argv[1]);
    int address = atoi(argv[2]);
    int length = atoi(argv[3]);
    int modbus_point_type = atoi(argv[4]);

    modbus_t *ctx;
    uint16_t tab_reg[length];
    char result_str[256] = {0}; // Buffer to store the result string
    char host_ip[INET_ADDRSTRLEN]; // Buffer to store the listener IP address

    printf("Checking serial port: %s\n", DEVICE);
    if (check_serial_port(DEVICE) != 0) {
        return EXIT_FAILURE;
    }

    if (read_host_ip(host_ip, sizeof(host_ip)) != 0) {
        return EXIT_FAILURE;
    }

    printf("Listener IP: %s\n", host_ip);

    ctx = modbus_new_rtu(DEVICE, BAUDRATE, 'N', 8, 1);
    if (ctx == NULL) {
        fprintf(stderr, "Error: Modbus RTU initialization failed.\n");
        return EXIT_FAILURE;
    }

    if (modbus_set_slave(ctx, device_id) == -1) {
        fprintf(stderr, "Error: Invalid slave ID %d\n", device_id);
        modbus_free(ctx);
        return EXIT_FAILURE;
    }

    modbus_set_debug(ctx, 1);  // Enable debug output

    if (modbus_connect(ctx) == -1) {
        fprintf(stderr, "Error: Modbus connection failed: %s\n", modbus_strerror(errno));
        modbus_free(ctx);
        return EXIT_FAILURE;
    }

    // Setup TCP connection
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        fprintf(stderr, "Error: Failed to create socket\n");
        modbus_close(ctx);
        modbus_free(ctx);
        return EXIT_FAILURE;
    }

    struct sockaddr_in server;
    server.sin_addr.s_addr = inet_addr(host_ip);
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        fprintf(stderr, "Error: Failed to connect to server\n");
        close(sock);
        modbus_close(ctx);
        modbus_free(ctx);
        return EXIT_FAILURE;
    }

    // Super-loop to read Modbus data and send it via TCP
    while (1) {
        int rc;
        switch(modbus_point_type) {
            case 1: // 01: COIL STATUS
                rc = modbus_read_bits(ctx, address, length, (uint8_t *)tab_reg);
                break;
            case 2: // 02: INPUT STATUS
                rc = modbus_read_input_bits(ctx, address, length, (uint8_t *)tab_reg);
                break;
            case 3: // 03: INPUT REGISTER
                rc = modbus_read_input_registers(ctx, address, length, tab_reg);
                break;
            case 4: // 04: HOLDING REGISTER
                rc = modbus_read_registers(ctx, address, length, tab_reg);
                break;
            default:
                fprintf(stderr, "Error: Invalid Modbus point type %d\n", modbus_point_type);
                close(sock);
                modbus_close(ctx);
                modbus_free(ctx);
                return EXIT_FAILURE;
        }

        if (rc == -1) {
            fprintf(stderr, "Error: Failed to read: %s\n", modbus_strerror(errno));
            snprintf(result_str, sizeof(result_str), "Error:%s", modbus_strerror(errno));
        } else {
            result_str[0] = '\0'; // Clear the result string
            for (int i = 0; i < rc; i++) {
                char buffer[16];
                snprintf(buffer, sizeof(buffer), "%d ", tab_reg[i]);
                strcat(result_str, buffer);
            }
        }

        // Send the result string or error response via TCP
        if (send(sock, result_str, strlen(result_str), 0) < 0) {
            fprintf(stderr, "Error: Failed to send data\n");
        } else {
            printf("Sent: %s\n", result_str);
        }

        sleep(1); // Delay for 1 second before the next iteration
    }

    close(sock);
    modbus_close(ctx);
    modbus_free(ctx);
    return EXIT_SUCCESS;
}
