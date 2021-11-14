
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
    if(app_layer.fd < 0){
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

    printf("Type T if you wish to transmit data or type R if instead you wish to receive data:\n");
    char mode;
    scanf("%c", &mode);
    if (mode == 'T')
    {
        app_layer.status = TRANSMITTER;
    }
    else if (mode == 'R')
        app_layer.status = RECEIVER;
    else
    {
        fprintf(stderr,"No valid letter was entered\n");
        return -1;
    }

    // O que devo fazer quando llopen falha? (continuar até dar, desistir...)
    // onde guardo o link_layer (dou como argumento &link_layer, defino no app...)
    llopen(app_layer);
    llclose();

    return 0;
}
