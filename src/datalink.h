// Calcular o bcc2 antes do byte stuffing e fazer stuffing do bcc2
#ifndef DATALINK_H
#define DATALINK_H

#include <termios.h>
#include "app.h"

// Used in signal alarm() function
#define ALARM_TIMEOUT 3
// Max transmissions allowed after timeout
#define RETRANS_MAX 3

// Maximum amount of time the function read() wait for a byte
#define MAX_TIME_TRANSMITTER 5
#define MAX_TIME_RECEIVER 5
// Minimum amount of bytes to be received for read() to exit function
#define MIN_BYTES_TRANSMITTER 0
#define MIN_BYTES_RECEIVER 1


#define FALSE 0
#define TRUE 1

// SET
#define F_SET 0x7E
#define A_SET 0x03
#define C_SET 0x03
#define BCC1_SET (A_SET ^ C_SET)

// UA
#define F_UA 0x7E
#define A_UA 0x03
#define C_UA 0x07
#define BCC1_UA (A_UA ^ C_UA)

#define FRAME_SIZE 20
#define BAUDRATE B38400

// Escape bytes
#define ESC_BYTE1 0x7E
#define ESC_BYTE2 0x7D

#define REPLACE_BYTE1 0x7D
#define REPLACE_BYTE2 0x5E
#define REPLACE_BYTE3 0x5D


// Minimum of 9 bytes to fit control frames
#define DATA_FRAMES_MAX_SIZE DATA_PACKAGE*2 + 6
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
    char frame[FRAME_SIZE];        /*Trama*/
    struct termios oldtio;         /*Port old settings*/
} LinkLayer;

int llopen(AppLayer);
int llwrite(BYTE* buff, int length);
int llread(BYTE* buff);
int llclose();
BYTE* byte_destuffing(BYTE *frame, unsigned int *size);
BYTE* retrieve_data(BYTE *frame, unsigned int size, unsigned int *data_size);
void create_frame(BYTE a, BYTE c, BYTE* new_frame);


#endif
