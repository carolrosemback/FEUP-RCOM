
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include "datalink.h"
#include "app.h"

int main(int argc, char const *argv[])
{
    if (argc < 2)
    {
        printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
        return (1);
    }

    AppLayer app_layer;

    app_layer.fd = open(argv[1], O_RDWR | O_NOCTTY);
    if (app_layer.fd < 0)
    {
        perror("Unable to open port");
        return -1;
    }
    /*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.
    */
    if (argv[1] < 0)
    {
        perror(argv[1]);
        return (-1);
    }

    char mode = argv[2][0];
    if (mode == 'T')
    {
        app_layer.status = TRANSMITTER;
    }
    else if (mode == 'R')
        app_layer.status = RECEIVER;
    else
    {
        fprintf(stderr, "No valid letter was entered\n");
        return -1;
    }

    // O que devo fazer quando llopen falha? (continuar até dar, desistir...)
    // onde guardo o link_layer (dou como argumento &link_layer, defino no app...)
    if (llopen(app_layer) <= 0)
        return -1;
    printf("Connected with success\n");

    if (app_layer.status == TRANSMITTER)
    {
        BYTE buff[DATA_PACKAGE];
        FILE *file_to_send = fopen(argv[3], "rb");
        if (file_to_send <= 0)
        {
            perror("fopen\n");
            return -1;
        }

        int length = 0;
        while ((length = fread(buff, sizeof(BYTE), DATA_PACKAGE, file_to_send)) > 0)
        {
            llwrite(buff, length);
        }

        //BYTE str2[8] = {0x00, 0xff, 0x7e, 0x5e, 0x7e, 0x7d, 0x5d, 0xff};
        // BYTE str2[2] = {0x7e, 0x7d};
        // llwrite(str2, 2);
        llclose();
        fclose(file_to_send);
    }
    else
    {
        int length;
        FILE *file_to_write;
        file_to_write = fopen(argv[3], "wb");
        if (file_to_write <= 0)
        {
            perror("fopen\n");
            return -1;
        }
        do
        {
            // Creating data package for the app
            BYTE buff[DATA_PACKAGE];
            length = llread(buff);
            if (length <= 0)
                return length;
            // printf("Read %d bytes\n", length);
            // for (int i = 0; i < length; i++)
            //     printf("%x ", buff[i] & 0xff);

            fwrite(buff, length, sizeof(BYTE), file_to_write);
        } while (length != 0);
        fclose(file_to_write);
    }

    return 0;
}
