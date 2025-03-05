#include <modbus/modbus.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#define DEVICE "/dev/ttyO1"
#define BAUDRATE 9600

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

    printf("Checking serial port: %s\n", DEVICE);
    if (check_serial_port(DEVICE) != 0) {
        return EXIT_FAILURE;
    }

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

    int rc;
    switch(modbus_point_type) {
        case 1: // 01: COIL STATUS
            printf("Reading %d coils from address %d...\n", length, address);
            rc = modbus_read_bits(ctx, address, length, (uint8_t *)tab_reg);
            break;
        case 2: // 02: INPUT STATUS
            printf("Reading %d input bits from address %d...\n", length, address);
            rc = modbus_read_input_bits(ctx, address, length, (uint8_t *)tab_reg);
            break;
        case 3: // 03: INPUT REGISTER
            printf("Reading %d input registers from address %d...\n", length, address);
            rc = modbus_read_input_registers(ctx, address, length, tab_reg);
            break;
        case 4: // 04: HOLDING REGISTER
            printf("Reading %d holding registers from address %d...\n", length, address);
            rc = modbus_read_registers(ctx, address, length, tab_reg);
            break;
        default:
            fprintf(stderr, "Error: Invalid Modbus point type %d\n", modbus_point_type);
            modbus_close(ctx);
            modbus_free(ctx);
            return EXIT_FAILURE;
    }

    if (rc == -1) {
        fprintf(stderr, "Error: Failed to read: %s\n", modbus_strerror(errno));
    } else {
        printf("Read %d points successfully:\n", rc);
        for (int i = 0; i < rc; i++) {
            char buffer[16];
            snprintf(buffer, sizeof(buffer), "%d ", tab_reg[i]);
            strcat(result_str, buffer);
        }
        printf("Result: %s\n", result_str);
    }

    modbus_close(ctx);
    modbus_free(ctx);
    return (rc == -1) ? EXIT_FAILURE : EXIT_SUCCESS;
}
