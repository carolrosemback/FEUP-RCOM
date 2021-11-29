
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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

    if (llopen(app_layer) <= 0)
    {
        perror("llopen");
        return -1;
    }

    if (app_layer.status == TRANSMITTER)
    {

        FILE *file_to_send = fopen(argv[3], "rb");
        if (file_to_send <= 0)
        {
            perror("fopen\n");
            return -1;
        }

        struct stat st;
        if (stat(argv[3], &st) != 0)
        {
            perror("stat()");
            return -1;
        }
        off_t file_size = st.st_size;
        int l1 = strlen(argv[3]);
        char file_name[l1 + 1];
        file_name[l1] = '\0';

        // Passing file name
        for (int i = 0; i < l1; i++)
        {
            file_name[i] = ((BYTE)argv[3][i]);
        }

        // we will write the file size as a string on the control package
        char file_size_str[sizeof(off_t)];
        int l2 = sprintf(file_size_str, "%zu", file_size);
        if (l2 < 0)
        {
            perror("sprintf");
            return l2;
        }

        size_t cur_pos = 0;
        // 3 Bytes 2 Integers and 2 Variables (l1,l2)
        size_t control_size = 3 + 2 + l1 + l2;
        BYTE control_package[control_size];
        // Filling control package
        control_package[cur_pos] = C_START;
        cur_pos++;

        control_package[cur_pos] = FILE_NAME_ARG; // file name
        cur_pos++;

        control_package[cur_pos] = l1;
        cur_pos++;

        for (int i = 0; i < l1; i++, cur_pos++)
            control_package[cur_pos] = file_name[i];

        control_package[cur_pos] = FILE_SIZE_ARG; // FILE SIZE
        cur_pos++;

        control_package[cur_pos] = l2;
        cur_pos++;
        for (int i = 0; i < l2; i++, cur_pos++)
        {
            control_package[cur_pos] = file_size_str[i];
        }

        if (control_size > CONTROL_PACKAGE_MAX_SIZE)
        {
            perror("Control package too big");
            return -1;
        }

        // Sending control package
        int ret = 0;
        ret = llwrite(control_package, control_size);
        if (ret < 0)
        {
            perror("llwrite");
            return ret;
        }
        printf("Sending %s with %zu bytes\n", file_name, file_size);

        // // Prints the control packet
        // for (size_t i = 0; i < control_size; i++)
        // {
        //     if (i <= 5 || (i >= 2+1+l1 && i <= 2+2+l1))
        //         printf("0x%x|", control_package[i] & 0xff);
        //     else
        //         printf("%c|", control_package[i]);
        // }
        // printf("\n");

        ssize_t bytes_read;
        BYTE data_packet[DATA_PACKET];
        data_packet[0] = C_DATA;
        data_packet[1] = 0;
        while ((bytes_read = fread(data_packet + 4, 1, DATA_PACKET - 4, file_to_send)) != 0)
        {
            if (bytes_read < 0)
            {
                perror("fread");
                return bytes_read;
            }
            data_packet[2] = bytes_read / 256;
            data_packet[3] = bytes_read % 256;

            llwrite(data_packet, 4 + bytes_read);
            if (data_packet[3] != 0) // Then we reached the last data packet
            {
                control_package[0] = C_END;
                llwrite(control_package, control_size);
                break;
            }
            data_packet[1] = (data_packet[1] + 1) % 256;
        }
        fclose(file_to_send);
        return llclose();
    }
    else
    {
        BYTE control_package[CONTROL_PACKAGE_MAX_SIZE];
        long length = llread(control_package);
        if (length <= 0)
        {
            perror("Unable to receive Control Packet");
            return -1;
        }

        size_t cur_pos = 0;
        int field_size;
        off_t file_size = 0;
        char *file_name;

        BYTE control_field = control_package[0];
        cur_pos++;

        // Inside this if is where the control package will be processed
        if (control_field == C_START)
        {
            for (; cur_pos < length; cur_pos++)
            {
                if (control_package[cur_pos] == FILE_NAME_ARG)
                {
                    cur_pos++;
                    field_size = control_package[cur_pos];
                    cur_pos++;
                    file_name = (char *)malloc(field_size + 1);
                    for (int i = 0; i < field_size; i++, cur_pos++)
                    {
                        file_name[i] = control_package[cur_pos];
                    }
                    file_name[cur_pos] = '\0';
                }
                if (control_package[cur_pos] == FILE_SIZE_ARG)
                {
                    cur_pos++;
                    field_size = control_package[cur_pos];
                    cur_pos++;

                    char file_size_str[field_size + 1];
                    for (int i = 0; i < field_size; i++, cur_pos++)
                        file_size_str[i] = control_package[cur_pos];
                    file_size_str[field_size] = '\0';
                    file_size = atol(file_size_str);
                }
            }
        }
        printf("Saved to %s file with %zu bytes\n", file_name, file_size);

        FILE *file_to_write;
        file_to_write = fopen(file_name, "wb");

        if (file_to_write < 0)
        {
            perror("fopen");
            return -1;
        }

        BYTE data_packet[DATA_PACKET];

        int n = 0;

        while ((length = llread(data_packet)))
        {
            if(length < 0){
                perror("llread");
                return length;
            }
            if (data_packet[0] == C_END)
            {
                // Checking there is nothing more to read
                if (llread(data_packet) == 0)
                    break;
            }
            else if (n == data_packet[1])
            {
                n = (n + 1) % 256;
                fwrite(data_packet + 4, length - 4, 1, file_to_write);
            }
            else
            {
                perror("A data packet was lost\n");
                return -1;
            }
        }

        fclose(file_to_write);
        free(file_name);
        return length;
    }

    return 0;
}