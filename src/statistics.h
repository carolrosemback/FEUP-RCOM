#ifndef STATISTICS_H
#define STATISTICS_H

#include <stdint.h> /* for uint64 definition */
#include <stdlib.h>
#include <unistd.h>
#include <time.h> /* for clock_gettime */
#include "datalink.h"
#include "math.h"
#include "app.h"

#define FER_HEADER 0 /* error % on the header */
#define FER_DATA 0   /* error % on the data */


uint64_t time_passed(struct timespec, struct timespec);

void init_errors();
// chooses if we will cause an errors in the frame
void entering_frame();
void exited_frame();
// causes an error in the header
BYTE random_error_header(BYTE );
// causes an error in the data frame (does not affect header)
BYTE random_error_data(BYTE );
void print_error_stats();

#endif