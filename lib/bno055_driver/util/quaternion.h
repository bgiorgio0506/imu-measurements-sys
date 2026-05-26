/*
    * quaternion.h - Definition of the Quaternion class for representing orientation
    * This class provides a simple implementation of a quaternion, which is used to represent
    * orientation in
    * 3D space. It includes methods for calculating roll, pitch, and yaw from the quaternion
    * components, as well as a method for dividing the quaternion by a scalar.
    * Author: Giorgio Bella
    * version 1.0
    * License GNU General Public License Version 3 (GPL3)
    * University of Pisa Italy, g.bella2@studenti.unipi.it, bgiorgio0506@gmail.com
    * Adapted from TeamSunride Arduino-BNO055
    */
#ifndef QUARTERION_H
#define QUARTERION_H

#include <cmath>

class Quaternion {
public:
    Quaternion(): orientation{0.0, 0.0, 0.0, 1.0} {};
    ~Quaternion() = default;
    struct EulerAngles {
        double roll;
        double pitch;
        double yaw;
    };

    double getRoll() {
    const double x = orientation[0];
    const double y = orientation[1];
    const double z = orientation[2];
    const double w = orientation[3];


    return std::atan2(
        2.0 * (w * x + y * z),
        1.0 - 2.0 * (x * x + y * y)
    );
}

double getPitch() {
    const double x = orientation[0];
    const double y = orientation[1];
    const double z = orientation[2];
    const double w = orientation[3];

    const double s = 2.0 * (w * y - z * x);

    // Safer against tiny floating-point drift outside [-1, 1]
    double clamped = s;
    if (clamped > 1.0) {
        clamped = 1.0;
    } else if (clamped < -1.0) {
        clamped = -1.0;
    }
    return std::asin(clamped);
}

double getYaw() {
    const double x = orientation[0];
    const double y = orientation[1];
    const double z = orientation[2];
    const double w = orientation[3];

    return std::atan2(
        2.0 * (w * z + x * y),
        1.0 - 2.0 * (y * y + z * z)
    );
}

double getX() { return orientation[0]; };
double getY() { return orientation[1]; };
double getZ() { return orientation[2]; };
double getW() { return orientation[3]; };

Quaternion divideByScalar(double scalar) const {
    Quaternion result;
    for (size_t i = 0; i < 4; i++)
    {
        result.orientation[i] = orientation[i] / scalar;
    }
    return result;
}
void setOrientation(double x, double y, double z, double w) {
    orientation[0] = x;
    orientation[1] = y;
    orientation[2] = z;
    orientation[3] = w;
}
void setX(double x) { orientation[0] = x; }
void setY(double y) { orientation[1] = y; }
void setZ(double z) { orientation[2] = z; }
void setW(double w) { orientation[3] = w; }


EulerAngles getEulerAngles() const {
    const double x = orientation[0];
    const double y = orientation[1];
    const double z = orientation[2];
    const double w = orientation[3];

    const double xx = x * x;
    const double yy = y * y;
    const double zz = z * z;

    const double wx = w * x;
    const double wy = w * y;
    const double wz = w * z;
    const double xy = x * y;
    const double xz = x * z;
    const double yz = y * z;

    EulerAngles e;

    e.roll = std::atan2(
        2.0 * (wx + yz),
        1.0 - 2.0 * (xx + yy)
    );

    const double pitchArg = 2.0 * (wy - xz);
    double pitchClamped = pitchArg;
    if (pitchClamped > 1.0) {
        pitchClamped = 1.0;
    } else if (pitchClamped < -1.0) {
        pitchClamped = -1.0;
    }
    e.pitch = std::asin(pitchClamped);

    e.yaw = std::atan2(
        2.0 * (wz + xy),
        1.0 - 2.0 * (yy + zz)
    );

    return e;
}
private:
    double orientation[4]; // Quaternion representation of orientation ( x, y, z, w)
};


#endif // QUARTERION_H