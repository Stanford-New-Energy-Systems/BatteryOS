#ifndef CURRENT_REGULATOR_HPP
#define CURRENT_REGULATOR_HPP

#include <string>

class CurrentRegulator {
public: 
    CurrentRegulator() {}
    virtual ~CurrentRegulator() {}
    virtual void set_current_Amps(double current_A) = 0;
    virtual double get_current_Amps() = 0;
};

#endif // ! CURRENT_REGULATOR_HPP
