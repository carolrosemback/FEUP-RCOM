#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include "datalink.h"
#include <signal.h>

int port_restore();
int port_setup();

// Used to mimic the automaton in state_machine_set-message.pdf
// These functions only equals 1 iteration of the automaton for their respective frames
void check_ua_byte(BYTE s, int *control_frames);
void check_set_byte(BYTE s, int *control_frames);
void timeout_set();
void timeout_i();

AppLayer app_layer;
LinkLayer link_layer;

int tries, flag, received_control, failed_connection,
    received_rr, frame_size, port_configured = FALSE;

BYTE set[5], ns = 0;
BYTE data_packet[DATA_FRAMES_MAX_SIZE];

int llopen(AppLayer upper_layer)
{
    // making AppLayer info available to all functions in this page
    app_layer = upper_layer;
    int res;
    if (!port_configured)
    {
        res = port_setup();
        if (res < 0) //error
            return res;
        port_configured = TRUE;
    }

    BYTE ssss;
    read(app_layer.fd,&ssss,1);
    printf("Hello\n");

    int control_frames[5];
    memset(control_frames, FALSE, 5 * sizeof(int));

    tries = 0;
    flag = TRUE;
    received_control = FALSE;
    failed_connection = FALSE;
    if (app_layer.status == TRANSMITTER)
    {
        set[0] = F_SET;
        set[1] = A_SET;
        set[2] = C_SET;
        set[3] = BCC1_SET;
        set[4] = F_SET;

        (void)signal(SIGALRM, timeout_set);
        res = write(app_layer.fd, set, 5);
        if (res < 0)
        {
            perror("Error in write()");
            return -1;
        }

        BYTE ua_byte;
        while (!received_control)
        {
            if (failed_connection)
            {
                fprintf(stderr, "Unable to connect after %d tries\n", link_layer.numTransmissions);
                return -1;
            }
            if (flag)
            {
                alarm(link_layer.timeout); // signal function
                flag = FALSE;
            }
            // if nothing is read it is not required to enter the if statement
            if (read(app_layer.fd, &ua_byte, 1) > 0)
            {
                check_ua_byte(ua_byte, control_frames);
            }
        }
    }
    else
    {
        BYTE set_byte;
        while (!received_control)
        {
            read(app_layer.fd, &set_byte, 1);
            check_set_byte(set_byte, control_frames);
        }

        BYTE ua[5];
        ua[0] = F_UA;
        ua[1] = A_UA;
        ua[2] = C_UA;
        ua[3] = BCC1_UA;
        ua[4] = F_UA;
        write(app_layer.fd, ua, 5);
    }
    alarm(0);
    return app_layer.fd;
}

// App data package is always smaller than Data-Link package
int llwrite(BYTE *buff, int length)
{
    printf("ns=%d\n",ns);
    data_packet[0] = F_SET;
    data_packet[1] = A_SET;
    data_packet[2] = (ns == 1) ? 0x40 : 0x00;
    data_packet[3] = data_packet[1] ^ data_packet[2];

    // Creating the Data Frame
    BYTE bcc2 = 0;
    int cur_pos = 4;
    for (int i = 0; i < length; i++, cur_pos++)
    {
        // BYTE STUFFING
        if (buff[i] == ESC_BYTE1)
        {
            data_packet[cur_pos] = REPLACE_BYTE1;
            cur_pos++;
            data_packet[cur_pos] = REPLACE_BYTE2;
        }
        else if (buff[i] == ESC_BYTE2)
        {
            data_packet[cur_pos] = REPLACE_BYTE1;
            cur_pos++;
            data_packet[cur_pos] = REPLACE_BYTE3;
        }
        else // Normal case
            data_packet[cur_pos] = buff[i];
        bcc2 = bcc2 ^ buff[i];
    }

    data_packet[cur_pos] = bcc2;
    cur_pos++;
    data_packet[cur_pos] = F_SET;

    frame_size = cur_pos + 1;
    printf("length %d\n", frame_size);
    write(app_layer.fd, data_packet, frame_size);

    // Variables used for timeout function
    tries = 0;
    flag = TRUE;
    received_rr = FALSE;
    failed_connection = FALSE;
    (void)signal(SIGALRM, timeout_i);

    BYTE ack;
    // Wrote data frame now we wait for the reply
    while (!received_rr)
    {
        if (failed_connection)
        {
            fprintf(stderr, "Unable to connect after %d tries\n", link_layer.numTransmissions);
            return -1;
        }
        if (flag)
        {
            alarm(link_layer.timeout); // signal function
            flag = FALSE;
        }
        if (read(app_layer.fd, &ack, 1) > 0)
        {
            // Emissor successfully received frame
            if ((ack == RR_0 && ns == 1) || (ack == RR_1 && ns == 0))
            {
                received_rr = TRUE;
                ns = (ns == 1) ? 0 : 1;
                return frame_size;
            }
            // Emissor received frame with errors
            else if ((ack == REJ_0 && ns == 1) || (ack == REJ_1 && ns == 0))
            {
                alarm(0);
                timeout_i();
                alarm(link_layer.timeout);
                flag = FALSE;
            }
        }
    }

    ns = (ns == 1) ? 0 : 1;
    return 0;
}

// Lembrar do caso em que o ua não é recebido e recebo outro set
int llread(BYTE* buff)
{
    BYTE frame[DATA_FRAMES_MAX_SIZE], rj=0, rr=0;
    BYTE input, *final_fr, *data, *destuffed_data;
    int control_frame[4];
    unsigned long int index=0;
    int verified = FALSE, data_size=0;
; 

    while(tries < 3)
    {
        alarm(3);
        while(!verified)
        {
            read(app_layer.fd, input, 1);
            frame[index] = input;
            switch(input){
            case F_SET:
                control_frame[0] = TRUE;
                break;
            case A_SET:
            if(control_frame[0])
                control_frame[1] = TRUE;
                break;
            case C_UA:
            if(control_frame[1])
                control_frame[2] = TRUE;
                break;
            case BCC1_SET:
            if (control_frame[2])
                control_frame[3] = TRUE;
                break;
            default:
                if(input == F_SET){
                    verified = TRUE;
                    break;
                }
                index++;
                break;  
              
            }
            alarm(0);
        }
    
    }

    // frame fitting to real size
    BYTE *res = malloc(sizeof(BYTE)*index);
    memcpy(res, frame, sizeof(BYTE)*index);

    data = retrieve_data(final_fr,index,&data_size);
    destuffed_data = byte_destuffing(frame, &index);
    BYTE bcc2 = 0, r_bcc2 = destuffed_data[index-1];

    for(int i = 0; i<index-1; i++)
        bcc2 ^= data[i];

    rj = (frame[2] == 0b010000000 ? REJ_0 : REJ_1);
    rr = (frame[2] == 0b010000000 ? RR_0 : RR_1);
    

    BYTE new_frame[5];
    if(bcc2 != r_bcc2){
        printf("ERROR in BCC2!");
        if(write(app_layer.fd, new_frame, 5) < 5)
        {
            printf("ERROR writing new_frame");
            create_frame(frame[1], rj, new_frame);
        }
    }

    create_frame(frame[1], rr, new_frame);
    memcpy(buff, destuffed_data, data_size-1);
    free(final_fr);
    free(data); 
    free(destuffed_data);
    return frame_size;
}


void create_frame(BYTE a, BYTE c, BYTE* new_frame)
{
    new_frame[0] = F_SET;
    new_frame[1] = a;
    new_frame[2] = c;
    new_frame[3] = new_frame[1] ^ new_frame[2];
    new_frame[4] = F_SET;
}



BYTE* retrieve_data(BYTE *fr, unsigned int size, unsigned int *index) {
    *index = size - 5;
    BYTE *data = (unsigned char*)malloc(*index);
    data = memcpy(data, &fr[4], *index);
    return data;
}


BYTE* byte_destuffing(BYTE *frame, unsigned int *size)
{
    BYTE* res;
    unsigned int n_size, escapes = 0;
    int offset=0;
    for (int i = 0; i < *size;i++){
        if(i < (*size - 1)) //gets bcc
        {
            if(frame[i] == 0x7d && frame[i + 1] == 0x5e)
                escapes++;

            if(frame[i] == 0x7d && frame[i + 1] == 0x5d)
                escapes++;
        }
    }
    n_size = *size - escapes;
    res = malloc(sizeof(BYTE)*n_size);
    *size = n_size; //updates size 
    for (unsigned long i = 0; i < n_size;){
        if(frame[i + offset] == 0x7d && frame[i + 1 + offset] == 0x5e){
            res[i++] = F_SET; //writes F_SET at i, and increments i for next position
            offset++;
        }
        else if(frame[i+offset] == 0x7d && frame[i+1+offset] == 0x5d){
            res[i++] = ESC_BYTE2;
            offset++;
        }
        else //not an stuffed byte, goes to next position
            res[i++] = frame[i+offset];
    }
    return res;
}

int llclose()
{
    int res = port_restore();
    if (res != 0) // error
        return res;
    return 0;
}

// used to load driver port with new configurations
// stores old configurations in struct link_layer
int port_setup()
{
    link_layer.baudRate = BAUDRATE;
    link_layer.timeout = ALARM_TIMEOUT;
    link_layer.numTransmissions = RETRANS_MAX;

    struct termios newtio;

    // saving old driver port configs to struct link_layer
    if (tcgetattr(app_layer.fd, &(link_layer.oldtio)) == -1)
    { /* save current port settings */
        perror("tcgetattr");
        return -1;
    }

    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;

    if (app_layer.status == TRANSMITTER)
    {
        newtio.c_cc[VTIME] = MAX_TIME_TRANSMITTER;
        newtio.c_cc[VMIN] = MIN_BYTES_TRANSMITTER;
    }
    else
    {
        newtio.c_cc[VTIME] = MAX_TIME_RECEIVER;
        newtio.c_cc[VMIN] = MIN_BYTES_RECEIVER;
    }

    tcflush(app_layer.fd, TCIOFLUSH);
    // Changing driver port settings
    if (tcsetattr(app_layer.fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        return -1;
    }
    return 0;
}

// Restores port original settings and closes file descriptor
int port_restore()
{
    sleep(1);
    if (tcsetattr(app_layer.fd, TCSANOW, &(link_layer.oldtio)) == -1)
    {
        perror("tcsetattr");
        return -1;
    }
    if (close(app_layer.fd) == -1)
    {
        perror("unable to close file descriptor");
        return -1;
    }
    return 0;
}

void check_ua_byte(BYTE s, int *control_frames)
{
    switch (s)
    {
    case F_UA:
        // Read 2nd Flag (End of UA)
        if (control_frames[3])
        {
            control_frames[4] = TRUE;
            received_control = TRUE;
        }
        // Received normal Flag
        else
        {
            control_frames[0] = TRUE;
            control_frames[1] = control_frames[2] = control_frames[3] = FALSE;
        }
        break;
    case A_UA:
        if (control_frames[0])
            control_frames[1] = TRUE;
        break;
    case C_UA:
        if (control_frames[1])
            control_frames[2] = TRUE;
        break;
    case BCC1_UA:
        if (control_frames[2])
            control_frames[3] = TRUE;
        break;
    default:
        // Restarting SET verification
        control_frames[0] = control_frames[1] = control_frames[2] = control_frames[3] = FALSE;
        break;
    }
}

// Used to mimic automaton in state_machine_set-message.pdf
// This function only equals 1 iteration of the automaton
void check_set_byte(BYTE s, int *control_frames)
{
    // Switch used to simulate SET State Machine
    switch (s)
    {
    case F_SET:
        // Received end of UA
        if (control_frames[3])
        {
            control_frames[4] = TRUE;
            received_control = TRUE;
        }

        // Received normal Flag
        else
        {
            control_frames[0] = TRUE;
            control_frames[1] = control_frames[2] = control_frames[3] = FALSE;
        }
        break;
    case A_SET:
        // C_SET case (which coincidentally has the same number as A_SET)
        if (control_frames[1])
            control_frames[2] = TRUE;
        // Actual A_SET case
        else if (control_frames[0])
            control_frames[1] = TRUE;
        break;
    case BCC1_SET:
        if (control_frames[2])
            control_frames[3] = TRUE;
        break;
    default:
        control_frames[0] = control_frames[1] = control_frames[2] = control_frames[3] = FALSE;
        break;
    }
}

// Function responsible for the timeout of the transmitor in the llopen() function
void timeout_set()
{
    if (received_control)
        return;

    if (tries >= link_layer.numTransmissions)
    {
        failed_connection = TRUE;
        return;
    }
    tries++;
    printf("set timeout: escrevendo # %d\n", tries);
    write(app_layer.fd, set, 5);
    flag = TRUE;
}

// Function responsible for the timeout of the transmitor in the llwrite() function
void timeout_i()
{
    if (received_rr)
        return;
    if (tries >= link_layer.numTransmissions)
    {
        failed_connection = TRUE;
        return;
    }
    tries++;
    printf("data timeout: escrevendo # %d\n", tries);
    write(app_layer.fd, data_packet, frame_size);
    flag = TRUE;
}