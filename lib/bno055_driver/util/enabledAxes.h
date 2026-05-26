#ifndef ENABLED_AXES_H
#define ENABLED_AXES_H

#include <Arduino.h>

class EnabledAxes {
            
        public:
            bool x = false;
            bool y = false;
            bool z = false;

            EnabledAxes enableX() {
                x = true;
                return *this;
            }

            EnabledAxes enableY() {
                y = true;
                return *this;
            };

            EnabledAxes enableZ() {
                z = true;
                return *this;
            };

            EnabledAxes disableX() {
                x = false;
                return *this;
            };

            EnabledAxes disableY() {
                y = false;
                return *this;
            };

            EnabledAxes disableZ() {
                z = false;
                return *this;
            };

            byte getEnabledAxes();
        };

#endif // ENABLED_AXES_H