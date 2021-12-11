#include "statistics.h"

long pos_data = -1, pos_header = -1, curpos_data = 0, curpos_header = 0, frames_with_errors_data = 0, frames_with_errors_header = 0, total_frames = 0;

uint64_t time_passed(struct timespec start, struct timespec end)
{
    return 1000000000L * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
}

void init_errors()
{
    srand(time(NULL));
}

// chooses if we will cause an errors in the frame
void entering_frame()
{
    total_frames++;
    pos_data = -1;
    pos_header = -1;
    if (1 + rand() % 100 <= FER_DATA)
    {
        frames_with_errors_data++;
        pos_data = rand() % ((DATA_FRAMES_MAX_SIZE - 6) / 2); // Range na maioria dos casos (sem escape bytes)
    }
    if (1 + rand() % 100 <= FER_HEADER)
    {
        frames_with_errors_header++;
        pos_header = rand() % 3; // A, C or BCC1
    }
}

void exited_frame()
{
    pos_data = pos_header = -1;
    curpos_data = curpos_header = 0;
}

// causes an error in the header
BYTE random_error_header(BYTE b)
{
    if (curpos_header == pos_header)
    {
        curpos_header = -1;
        return rand() % 256;
    }
    curpos_header++;
    return b;
}

// causes an error in the data frame (does not affect header)
BYTE random_error_data(BYTE b)
{
    if (curpos_data == pos_data)
    {
        curpos_data = -1;
        return rand() % 256;
    }
    curpos_data++;
    return b;
}

void print_error_stats()
{
    printf("corrupted %ld data and %ld header from a total of %ld frames\n", frames_with_errors_data, frames_with_errors_header, total_frames);
}