#include "datalink.h"
//#include "statistics.h"

int port_restore();
int port_setup();

// Used to mimic the automaton in state_machine_set-message.pdf
// These functions only equals 1 iteration of the automaton for their respective frames
void check_ua_byte(BYTE s, int *control_frames);
void check_set_byte(BYTE s, int *control_frames);
void check_disc_byte(BYTE s, int *control_frames);
void timeout_set();
void timeout_i();
void timeout_disc();
void llread_header_check(BYTE s, int *control_frames, BYTE *control_field);

AppLayer app_layer;
LinkLayer link_layer;

int tries, flag, received_control, failed_connection,
    received_rr, frame_size, port_configured = FALSE;

BYTE set[5], ns = 0;
BYTE data_packet[DATA_FRAMES_MAX_SIZE];

int llopen(AppLayer upper_layer)
{
    tries = 0;
    // making AppLayer info available to all functions in this page
    app_layer = upper_layer;
    int res;
    if (!port_configured)
    {
        res = port_setup();
        if (res < 0) // error
            return res;
        port_configured = TRUE;
    }

    int control_frames[5];
    memset(control_frames, FALSE, 5 * sizeof(int));

    received_control = FALSE;
    if (app_layer.status == TRANSMITTER)
    {
        flag = TRUE;
        failed_connection = FALSE;
        set[0] = FLAG;
        set[1] = AF_TRANS;
        set[2] = SET;
        set[3] = set[1] ^ set[2];
        set[4] = FLAG;

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
                alarm(0);
                port_restore();
                fprintf(stderr, "llopen Unable to connect after %d tries\n", link_layer.numTransmissions);
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
            while (read(app_layer.fd, &set_byte, 1) < 0)
                ;
            check_set_byte(set_byte, control_frames);
        }

        BYTE ua[5];
        ua[0] = FLAG;
        ua[1] = AF_TRANS;
        ua[2] = UA;
        ua[3] = ua[1] ^ ua[2];
        ua[4] = FLAG;
        write(app_layer.fd, ua, 5);
        // init_errors(); // Used to generate errors in llread()
    }
    alarm(0);
    return app_layer.fd;
}

// App data package is always smaller than Data-Link package
int llwrite(BYTE *buff, int length)
{

    data_packet[0] = FLAG;
    data_packet[1] = AF_TRANS;
    data_packet[2] = (ns == 1) ? DATA_CTRL_1 : DATA_CTRL_0;
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

    // Byte stuffing of bcc2
    if (bcc2 == ESC_BYTE1)
    {
        data_packet[cur_pos] = REPLACE_BYTE1;
        cur_pos++;
        data_packet[cur_pos] = REPLACE_BYTE2;
    }
    else if (bcc2 == ESC_BYTE2)
    {
        data_packet[cur_pos] = REPLACE_BYTE1;
        ++cur_pos;
        data_packet[cur_pos] = REPLACE_BYTE3;
    }
    else
        data_packet[cur_pos] = bcc2;
    cur_pos++;

    data_packet[cur_pos] = FLAG;

    frame_size = cur_pos + 1;

    // Variables used for timeout function
    tries = 0;
    flag = TRUE;
    received_rr = FALSE;
    failed_connection = FALSE;
    (void)signal(SIGALRM, timeout_i);

    write(app_layer.fd, data_packet, frame_size);

    BYTE ack;
    // Wrote data frame now we wait for the reply
    while (!received_rr)
    {
        if (failed_connection)
        {
            port_restore();
            fprintf(stderr, "llwrite() Unable to connect after %d tries\n", link_layer.numTransmissions);
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
                alarm(0);
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
    alarm(0);
    ns = (ns == 1) ? 0 : 1;
    return 0;
}

int llread(BYTE *buff)
{
    int control_frames[5];
    memset(control_frames, FALSE, 5 * sizeof(int));
    received_control = FALSE;
    BYTE b, control_field;
    while (!received_control)
    {
        while (read(app_layer.fd, &b, 1) < 0)
            ;
        /*
        if(b != FLAG) random_error_header(b);
        else entering_frame();
        */
        llread_header_check(b, control_frames, &control_field);
    }
    BYTE resp[5] = {FLAG, AF_TRANS, 0, 0, FLAG};

    /*
    Reading 1 bit to check if its FLAG (Supervision or Unnumbered frames)
    or if its another bit (Data frame)
    */
    while (read(app_layer.fd, &b, 1) < 0)
        ;

    if (b == FLAG) // Supervision or Unnumbered frames
    {
        // Transmitor did not receive UA
        if (control_field == SET)
        {
            resp[1] = AF_TRANS;
            resp[2] = UA;
            resp[3] = resp[1] ^ resp[2];
            write(app_layer.fd, resp, 5);
            return llread(buff);
        }
        else if (control_field == DISC)
        {
            resp[1] = AF_REC;
            resp[2] = DISC;
            resp[3] = resp[1] ^ resp[2];
            write(app_layer.fd, resp, 5);
            while (1)
            {
                while (read(app_layer.fd, resp, 5) < 0)
                    ;
                // if its a UA frame
                if (resp[0] == FLAG && resp[4] == FLAG && resp[1] == AF_REC && resp[2] == UA && resp[3] == (resp[1] ^ resp[2]))
                {
                    return 0;
                }
                // if the DISC frame again (UA was lost and transmittor timed out)
                else if (resp[0] == FLAG && resp[4] == FLAG && resp[1] == AF_TRANS && resp[2] == DISC && resp[3] == (resp[1] ^ resp[2]))
                {
                    write(app_layer.fd, resp, 5);
                }
            }
        }
    }
    else // Data frame case
    {
        // duplicate data frame
        if ((control_field == DATA_CTRL_0 && ns == 1) || (control_field == DATA_CTRL_1 && ns == 0))
        {
            resp[2] = (control_field == DATA_CTRL_0) ? RR_1 : RR_0;
            resp[3] = resp[1] ^ resp[2];
            write(app_layer.fd, resp, 5);
            return llread(buff);
        }
        // new data frame
        else if (control_field == DATA_CTRL_0 || control_field == DATA_CTRL_1)
        {
            int cur_pos = 0, bcc2 = 0;
            BYTE b2;
            while (1)
            {
                while (read(app_layer.fd, &b2, 1) < 0)
                    ;
                /*if(b2 != FLAG) b2 = random_error_data(b2); */
                // BYTE DESTUFFING
                if (b == REPLACE_BYTE1)
                {
                    if (b2 == REPLACE_BYTE2)
                        b = buff[cur_pos] = ESC_BYTE1;
                    else if (b2 == REPLACE_BYTE3)
                        b = buff[cur_pos] = ESC_BYTE2;
                    while (read(app_layer.fd, &b2, 1) < 0)
                        ;
                }
                // Data Frame ended
                if (b2 == FLAG)
                {
                    // exited_frame();
                    resp[1] = AF_TRANS;
                    if (bcc2 == b)
                    {
                        resp[2] = (control_field == DATA_CTRL_1) ? RR_0 : RR_1;
                        resp[3] = resp[1] ^ resp[2];
                        write(app_layer.fd, resp, 5);
                        ns = ns ? 0 : 1;
                        return cur_pos;
                    }
                    // Frame has errors
                    else
                    {
                        resp[2] = (control_field == DATA_CTRL_1) ? REJ_0 : REJ_1;
                        resp[3] = resp[1] ^ resp[2];
                        write(app_layer.fd, resp, 5);
                        return llread(buff);
                    }
                }
                else
                    buff[cur_pos] = b;
                bcc2 = bcc2 ^ buff[cur_pos];
                cur_pos++;
                b = b2;
            }
        }
    }
    return 0;
}

// Close used only by transmitter
int llclose()
{
    tries = 0;
    flag = TRUE;
    received_control = FALSE;
    failed_connection = FALSE;
    BYTE send[5] = {FLAG, AF_TRANS, DISC, AF_TRANS ^ DISC, FLAG};
    write(app_layer.fd, send, 5);

    (void)signal(SIGALRM, timeout_disc);

    int control_frames[5] = {FALSE, FALSE, FALSE, FALSE, FALSE};
    BYTE disc_byte;
    while (!received_control)
    {
        if (failed_connection)
        {
            port_restore();
            fprintf(stderr, "llclose() Unable to connect after %d tries\n", link_layer.numTransmissions);
            return -1;
        }
        if (flag)
        {
            alarm(link_layer.timeout); // signal function
            flag = FALSE;
        }
        // if nothing is read it is not required to enter the if statement
        if (read(app_layer.fd, &disc_byte, 1) > 0)
        {
            check_disc_byte(disc_byte, control_frames);
        }
    }
    send[1] = AF_REC;
    send[2] = UA;
    send[3] = send[1] ^ send[2];
    write(app_layer.fd, send, 5);
    alarm(0);
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
        perror("port_restore() unable to close file descriptor");
        return -1;
    }
    return 0;
}

void check_ua_byte(BYTE s, int *control_frames)
{
    switch (s)
    {
    case FLAG:
        // Read 2nd Flag (End of UA)
        if (control_frames[3])
            received_control = TRUE;
        // Received normal Flag
        else
        {
            control_frames[0] = TRUE;
            control_frames[1] = control_frames[2] = control_frames[3] = FALSE;
        }
        break;
    case AF_TRANS:
        if (control_frames[0])
            control_frames[1] = TRUE;
        else
            control_frames[0] = control_frames[1] = control_frames[2] = control_frames[3] = FALSE;
        break;
    case UA:
        if (control_frames[1])
            control_frames[2] = TRUE;
        else
            control_frames[0] = control_frames[1] = control_frames[2] = control_frames[3] = FALSE;
        break;
    case (AF_TRANS ^ UA):
        if (control_frames[2])
            control_frames[3] = TRUE;
        else
            control_frames[0] = control_frames[1] = control_frames[2] = control_frames[3] = FALSE;
        break;
    default:
        // Restarting UA verification
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
    case FLAG:
        // Received end of UA
        if (control_frames[3])
            received_control = TRUE;

        // Received normal Flag
        else
        {
            control_frames[0] = TRUE;
            control_frames[1] = control_frames[2] = control_frames[3] = FALSE;
        }
        break;
    case AF_TRANS:
        // SET case (which coincidentally has the same number as AF_TRANS)
        if (control_frames[1])
            control_frames[2] = TRUE;
        // Actual A case
        else if (control_frames[0])
            control_frames[1] = TRUE;
        else
            control_frames[0] = control_frames[1] = control_frames[2] = control_frames[3] = FALSE;
        break;
    case (AF_TRANS ^ SET):
        if (control_frames[2])
            control_frames[3] = TRUE;
        else
            control_frames[0] = control_frames[1] = control_frames[2] = control_frames[3] = FALSE;
        break;
    default:
        control_frames[0] = control_frames[1] = control_frames[2] = control_frames[3] = FALSE;
        break;
    }
}

void check_disc_byte(BYTE s, int *control_frames)
{
    switch (s)
    {
    case FLAG:
        // Read 2nd Flag (End of UA)
        if (control_frames[3])
            received_control = TRUE;
        // Received normal Flag
        else
        {
            control_frames[0] = TRUE;
            control_frames[1] = control_frames[2] = control_frames[3] = FALSE;
        }
        break;
    case AF_REC:
        if (control_frames[0])
            control_frames[1] = TRUE;
        else
            control_frames[0] = control_frames[1] = control_frames[2] = control_frames[3] = FALSE;
        break;
    case DISC:
        if (control_frames[1])
            control_frames[2] = TRUE;
        else
            control_frames[0] = control_frames[1] = control_frames[2] = control_frames[3] = FALSE;
        break;
    case (AF_REC ^ DISC):
        if (control_frames[2])
            control_frames[3] = TRUE;
        else
            control_frames[0] = control_frames[1] = control_frames[2] = control_frames[3] = FALSE;
        break;
    default:
        // Restarting UA verification
        control_frames[0] = control_frames[1] = control_frames[2] = control_frames[3] = FALSE;
        break;
    }
}

/* This function reads the beginning of the frame
and informs the llread function what type of control field it has */
void llread_header_check(BYTE s, int control_frames[], BYTE *control_field)
{
    // BCC1 case
    if (control_frames[2] && (s == (AF_TRANS ^ *control_field) || s == (AF_REC ^ *control_field)))
        received_control = TRUE;
    else if (s == FLAG)
    {
        // Received normal Flag
        control_frames[0] = TRUE;
        control_frames[1] = control_frames[2] = FALSE;
    }
    else if (control_frames[0] && s == AF_TRANS)
    {
        // SET case (which coincidentally has the same number as AF_TRANS)
        /* Case where transmitor is stuck in llopen() because UA frame was lost and timed out */
        if (control_frames[1])
        {
            control_frames[2] = TRUE;
            *control_field = SET;
        }
        // Actual A case
        control_frames[1] = TRUE;
    }

    // Information frame control field
    else if (control_frames[1] && (s == DATA_CTRL_1 || s == DATA_CTRL_0 || s == DISC))
    {
        *control_field = s;
        control_frames[2] = TRUE;
    }
    else
        control_frames[0] = control_frames[1] = control_frames[2] = FALSE;
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
    write(app_layer.fd, data_packet, frame_size);
    flag = TRUE;
}

void timeout_disc()
{
    if (received_control)
        return;
    if (tries >= link_layer.numTransmissions)
    {
        failed_connection = TRUE;
        return;
    }
    tries++;
    set[0] = FLAG;
    set[1] = AF_TRANS;
    set[2] = DISC;
    set[3] = set[1] ^ set[2];
    set[4] = FLAG;
    write(app_layer.fd, set, 5);
    flag = TRUE;
}