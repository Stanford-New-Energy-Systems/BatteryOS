#ifndef SCALE_HPP
#define SCALE_HPP 

enum class PolicyType : int {
    RESERVED = 0,
    TRANCHED = 1,  
    PROPORTIONAL = 2,
};

typedef struct Scale {
    double charge_proportion;
    double capacity_proportion;

    private:
        double ABS(const double &x) {
            if (x < 0)
                return x * -1;
            return x;
        }

    public:
        Scale(double capacity, double charge) {
            this->charge_proportion   = ABS(charge);
            this->capacity_proportion = ABS(capacity);
        } 

        Scale(double proportion=0.0) {
            this->charge_proportion   = ABS(proportion);
            this->capacity_proportion = ABS(proportion);
        }

        Scale operator+(const Scale &other) {
            double charge   = this->charge_proportion + other.charge_proportion;
            double capacity = this->capacity_proportion + other.capacity_proportion;
            return Scale(capacity, charge);
        }

        Scale& operator+=(const Scale& other) {
            this->charge_proportion += other.charge_proportion;
            this->capacity_proportion += other.capacity_proportion;
            return *this;
        }
        
} Scale;

#endif
