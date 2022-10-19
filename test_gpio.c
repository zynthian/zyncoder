
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <wiringPi.h>

int main() {
    int i;
    int pin_a=10;
    int n_reads=100000000;

	wiringPiSetup();
	pinMode(pin_a, INPUT);
	pullUpDnControl(pin_a, PUD_UP);

	struct timespec ts;
	uint64_t tsns1, tsns2;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	tsns1 = (uint64_t)ts.tv_sec * (uint64_t)1000000000 + (uint64_t)ts.tv_nsec;
	printf("Starting at time = %llu\n", tsns1);
    for (i=0; i<n_reads; i++) {
        digitalRead(pin_a);
    }
	clock_gettime(CLOCK_MONOTONIC, &ts);
	tsns2 = (int64_t)ts.tv_sec * (uint64_t)1000000000 + (uint64_t)ts.tv_nsec;
	printf("Finishing at time = %llu\n", tsns2);

	uint64_t ns_read = (tsns2 - tsns1) / n_reads;
	printf("GPIO read time (ns) = %llu\n", ns_read);
}
