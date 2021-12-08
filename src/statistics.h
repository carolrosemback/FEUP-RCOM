#ifndef STATISTICS_H
#define STATISTICS_H

#include <stdint.h> /* for uint64 definition */
#include <unistd.h>
#include <time.h> /* for clock_gettime */

#define FER_HEADER 20 /* error % on the header */
#define FER_DATA 20   /* error % on the data */


uint64_t time_passed(struct timespec, struct timespec);

#endif