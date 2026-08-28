#ifndef ULTRASONIC_H
#define ULTRASONIC_H

#include <stdbool.h>

void ultrasonic_init(void);
bool ultrasonic_read(float *distance_cm);

#endif /* ULTRASONIC_H */
