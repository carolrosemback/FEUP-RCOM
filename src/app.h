#ifndef APP_H
#define APP_H

#define TRANSMITTER 1
#define RECEIVER 0

// 4 Bytes for processing and Data
#define DATA_PACKET 4 + 256 * 40
// Calculated max size was 255*2 + 4
#define CONTROL_PACKAGE_MAX_SIZE 256*2 + 4
#define C_START 0x02
#define C_END 0x03
#define C_DATA 0x01

#define FILE_SIZE_ARG 0x00
#define FILE_NAME_ARG 0x01

typedef struct AppLayer
{
    int fd;     // File Descriptor that matches intended port
    int status; // TRANSMITTER or RECEIVER
} AppLayer;

#endif