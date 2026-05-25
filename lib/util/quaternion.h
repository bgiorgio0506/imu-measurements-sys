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

#include <Math.h>

class Quaternion {
public:
    Quaternion();
    ~Quaternion();
    double getRoll(){
        return atan2(2*((orientation[3]*orientation[0]) + (orientation[2]*orientation[1])), 1-2*(pow(orientation[0], 2) + pow(orientation[1], 2)));
    }; // Roll in radians
    double getPitch(){
        return asin(2*((orientation[3]*orientation[1]) - (orientation[0]*orientation[2])));
    }; // Pitch in radians
    double getYaw(){
        return atan2(2*((orientation[3]*orientation[2]) + (orientation[0]*orientation[1])), 1-2*(pow(orientation[1], 2) + pow(orientation[2], 2)));
    };  // Yaw in radians
    
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
private:
    double orientation[4]; // Quaternion representation of orientation ( x, y, z, w)
};


#endif // QUARTERION_H