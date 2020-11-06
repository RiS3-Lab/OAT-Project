#include <time.h>
#include <sys/time.h>
#include "lib/util.h"

unsigned long usecs() {
	struct timeval start;

	gettimeofday(&start, NULL);

	return start.tv_sec * 1000 * 1000 + start.tv_usec;

}
