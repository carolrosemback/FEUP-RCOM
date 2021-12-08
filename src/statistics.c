#include "statistics.h"


uint64_t time_passed(struct timespec start, struct timespec end){
    return 1000000000L * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
}

