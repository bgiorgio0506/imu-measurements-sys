#ifndef GYRO_H
#define GYRO_H

#include <Arduino.h>

class GyroHRAxes {
    protected:
        struct GyroHRAxisSetting {
            // default values from datasheet:
            byte duration = 0b00011001;
            byte threshold = 0b01;
            byte hysteresis = 0b0;
        };

    public:

        GyroHRAxisSetting x;
        GyroHRAxisSetting y;
        GyroHRAxisSetting z;

        /**
        * Output a buffer of bytes to write to registers 18-1D
        * @param output
        */
        void get(byte *output) const {
            *output = (0b00011111 & x.threshold) | ((0b11 & x.hysteresis) << 5); output++;
            *output = x.duration; output++;

            *output = (0b00011111 & y.threshold) | ((0b11 & y.hysteresis) << 5); output++;
            *output = y.duration; output++;

            *output = (0b00011111 & z.threshold) | ((0b11 & z.hysteresis) << 5); output++;
            *output = z.duration; output++;
        }
};
#endif // GYRO_H