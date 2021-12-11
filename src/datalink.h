#ifndef DATALINK_H
#define DATALINK_H

#include <termios.h>
#include "app.h"

// Used in signal alarm() function
#define ALARM_TIMEOUT 3
// Max transmissions allowed after timeout
#define RETRANS_MAX 3

// Maximum amount of time the function read() wait for a byte
#define MAX_TIME_TRANSMITTER 1
#define MAX_TIME_RECEIVER 1
// Minimum amount of bytes to be received for read() to exit function
#define MIN_BYTES_TRANSMITTER 0
#define MIN_BYTES_RECEIVER 1

#define FALSE 0
#define TRUE 1

#define FLAG 0x7E

// Control Fields
#define DISC 0x0B
#define SET 0x03
#define UA 0x07
#define RR_0 0x05
#define RR_1 0x85
#define REJ_0 0x01
#define REJ_1 0x81
#define DATA_CTRL_1 0x40
#define DATA_CTRL_0 0x00

// Address Fields
// Commands sent by the transmitter and replies sent by receiver
#define AF_TRANS 0x03
// Commands sent by the receiver and replies sent by transmitter
#define AF_REC 0x01

#define BAUDRATE B9600

// Escape bytes
#define ESC_BYTE1 0x7E
#define ESC_BYTE2 0x7D

#define REPLACE_BYTE1 0x7D
#define REPLACE_BYTE2 0x5E
#define REPLACE_BYTE3 0x5D

// Minimum of 9 bytes to fit control frames
#define DATA_FRAMES_MAX_SIZE DATA_PACKET * 2 + 6
#define RR_0 0x05
#define RR_1 0x85
#define REJ_0 0x01
#define REJ_1 0x81

typedef unsigned char BYTE;
typedef struct LinkLayer
{
    int baudRate;                  /*Velocidade de transmissão*/
    unsigned int sequenceNumber;   /*Número de sequência da trama: 0, 1*/
    unsigned int timeout;          /*Valor do temporizador: 1 s*/
    unsigned int numTransmissions; /*Número de tentativas em caso de falha*/
    struct termios oldtio;         /*Port old settings*/
} LinkLayer;

int llopen(AppLayer);
int llwrite(BYTE *buff, int length);
int llread(BYTE *buff);
int llclose();

#endif
