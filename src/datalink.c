#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "datalink.h"
#include <signal.h>

// Duvida: Esta parte ainda é data link, ou é network configuration ou outro layer?
int port_restore();
int port_setup();

// Used to mimic the automaton in state_machine_set-message.pdf
// These functions only equals 1 iteration of the automaton for their respective frames
void check_ua_byte(BYTE s, int *control_frames);
int check_set_byte(BYTE s, int *control_frames);
void timeout_set();

AppLayer app_layer;
LinkLayer link_layer;

int tries = 0, flag = TRUE, received_control = FALSE, failed_connection = FALSE, port_configured = FALSE;
BYTE set[5];

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

    int control_frames[5];
    memset(control_frames, FALSE, 5 * sizeof(int));

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

        printf("Sent set\n");
        for (int i = 0; i < 5; i++)
            printf("0x%02x\n", set[i]);

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
                flag = 0;
            }
            // if nothing is read it is not required to enter the if statement
            if (read(app_layer.fd, &ua_byte, 1) > 0)
            {
                check_ua_byte(ua_byte, control_frames);
                printf("READ |0x%02x|\n", ua_byte);
            }
        }
    }
    else
    {
        BYTE set_byte;
        printf("||\n");
        while (!received_control)
        {
            read(app_layer.fd, &set_byte, 1);
            check_set_byte(set_byte, control_frames);
            printf("|0x%02x|\n", set_byte);
        }

        printf("--------------\n");

        BYTE ua[5];
        ua[0] = F_UA;
        ua[1] = A_UA;
        ua[2] = C_UA;
        ua[3] = BCC1_UA;
        ua[4] = F_UA;

        for (int i = 0; i < 5; i++)
            printf("|0x%02x|\n", ua[i]);
        write(app_layer.fd, ua, 5);
    }

    return 0;
}

int llclose()
{
    int res = port_restore();
    if (res < 0) // error
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
int check_set_byte(BYTE s, int *control_frames)
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
    return TRUE;
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
    printf("escrevendo # %d\n", tries);
    write(app_layer.fd, set, 5);
    flag = TRUE;
}