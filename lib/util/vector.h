/*
    * vector.h - Template class for 3D vectors
    *
    * This class provides a simple implementation of a 3D vector with basic
    * operations such as setting and getting components, and dividing by a scalar.
    * Author: Giorgio Bella
    * version 1.0
    * License GNU General Public License Version 3 (GPL3)
    * University of Pisa Italy, g.bella2@studenti.unipi.it, bgiorgio0506@gmail.com
*/

#ifndef VECTOR_H
#define VECTOR_H

#define VEC_SIZE 3 // Standard size for 3D vectors (x, y, z)

template<typename T>
class Vector{
public:
    Vector();
    ~Vector();
    T vec[VEC_SIZE];
    void setX(T x) { this->vec[0] = x; }
    void setY(T y) { this->vec[1] = y; }
    void setZ(T z) { this->vec[2] = z; }
    T getX() const { return this->vec[0]; }
    T getY() const { return this->vec[1]; }
    T getZ() const { return this->vec[2]; }
    Vector<double> divideByScalar(T scalar) const {
        Vector<double> result;
        for (size_t i = 0; i < VEC_SIZE; i++)
        {
            result.vec[i] = static_cast<double>(vec[i]) / scalar;
        }
        return result;
    }
};
#endif