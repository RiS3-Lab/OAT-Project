#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include "lib/util.h"

void pinMode(int pin, int mode) {
	printf("%s (int pin = %d, int mode = %d)\n", __func__, pin, mode);
	return;
}

int digitalRead(int pin) {
	int val;
	printf("%s (int pin = %d)\n", __func__, pin);
	scanf("%d", &val);
	return val;
}

void digitalWrite(int pin, int value) {
//	printf("%s (int pin = %d, int value = %d)\n", __func__, pin, value);
	return;
}

void Serial_begin(int baud) {
	printf("%s (int baud = %d)\n", __func__, baud);
	return;
}

int Serial_available() {
	char c;

	c = getchar();

	printf("%s() c:%c\n", __func__, c);

	if (c == 'y')
		return 1;
	else
		return 0;
}

int Serial_read() {
	char c;

	c = getchar();

	return (int)c;
}

int Serial_write(char* output, int len) {
	printf("%s (char *output = %s, int len = %d)\n", __func__, output, len);
	return 0;
}

int analogRead(int pin) {
	int val;
	printf("read from pin %d\n", pin);
	scanf("%d", &val);
	return val;
}

unsigned long millis() {
	struct timeval start;

	gettimeofday(&start, NULL);

	return start.tv_sec * 1000 + start.tv_usec/1000;

}

unsigned long usecs() {
	struct timeval start;

	gettimeofday(&start, NULL);

	return start.tv_sec * 1000 * 1000 + start.tv_usec;

}

void delayMicroseconds(float usecs) {
	usleep((long)usecs);
}

int toUInt(char* input, int len) {
	int val;
	val = atoi(input);
	return val;
}
