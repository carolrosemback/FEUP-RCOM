#ifndef APP_H
#define APP_H

#define TRANSMITTER 1
#define RECEIVER 0

typedef struct AppLayer
{
    int fd;     // File Descriptor that matches intended port
    int status; // TRANSMITTER or RECEIVER
} AppLayer;

#endif