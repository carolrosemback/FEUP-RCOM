#include "statistics.h"


int errors_generated, max_errors_data, max_errors_header;

uint64_t time_passed(struct timespec start, struct timespec end)
{
    return 1000000000L * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
}

void initialize_errors()
{
    errors_generated = 0;
    max_errors_data = DATA_FRAMES_MAX_SIZE * ceil(FER_DATA / (float)100);
    max_errors_header = 4 * ceil(FER_DATA / (float)100);
}

void restart_errors()
{
    errors_generated = 0;
}

// causes an error in the data packet (header not included)
BYTE random_error_data(BYTE b)
{
    if (errors_generated <= max_errors_data && rand() % 2)
    {
        errors_generated++;
        return rand() % 256;
    }
    return b;
}

// causes an error in the header 
BYTE random_error_header(BYTE b)
{
    if (errors_generated <= max_errors_header && rand() % 2)
    {
        errors_generated++;
        return rand() % 256;
    }
    return b;
}