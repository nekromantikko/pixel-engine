#include "fixed.h"

fixed32 operator+(const double& a, const fixed32& b) {
    fixed32 c;
    c.d = fixed32::fix(a + b.to_double());
    return c;
}

fixed32 operator-(const double& a, const fixed32& b) {
    fixed32 c;
    c.d = fixed32::fix(a - b.to_double());
    return c;
}

fixed32 operator/(const double& a, const fixed32& b) {
    fixed32 c;
    c.d = fixed32::fix(a / b.to_double());
    return c;
}

fixed32 operator*(const double& a, const fixed32& b) {
    fixed32 c;
    c.d = fixed32::fix(a * b.to_double());
    return c;
}

double& operator+=(double& a, const fixed32& b) {
    a += (double)b;
    return a;
}
double& operator-=(double& a, const fixed32& b) {
    a -= (double)b;
    return a;
}
double& operator/=(double& a, const fixed32& b) {
    a /= (double)b;
    return a;
}
double& operator*=(double& a, const fixed32& b) {
    a *= (double)b;
    return a;
}

bool operator==(const double& a, const fixed32& b) {
    return fixed32(a) == b;
}

bool operator!=(const double& a, const fixed32& b) {
    return fixed32(a) != b;
}

bool operator<=(const double& a, const fixed32& b) {
    return fixed32(a) <= b;
}

bool operator>=(const double& a, const fixed32& b) {
    return fixed32(a) >= b;
}

bool operator>(const double& a, const fixed32& b) {
    return fixed32(a) > b;
}

bool operator<(const double& a, const fixed32& b) {
    return fixed32(a) < b;
}

/////////////////////////////////////////////

fixed32 operator+(const float& a, const fixed32& b) {
    fixed32 c;
    c.d = fixed32::fix(a + b.to_double());
    return c;
}

fixed32 operator-(const float& a, const fixed32& b) {
    fixed32 c;
    c.d = fixed32::fix(a - b.to_double());
    return c;
}

fixed32 operator/(const float& a, const fixed32& b) {
    fixed32 c;
    c.d = fixed32::fix(a / b.to_double());
    return c;
}

fixed32 operator*(const float& a, const fixed32& b) {
    fixed32 c;
    c.d = fixed32::fix(a * b.to_double());
    return c;
}

float& operator+=(float& a, const fixed32& b) {
    a += (float)b;
    return a;
}
float& operator-=(float& a, const fixed32& b) {
    a -= (float)b;
    return a;
}
float& operator/=(float& a, const fixed32& b) {
    a /= (float)b;
    return a;
}
float& operator*=(float& a, const fixed32& b) {
    a *= (float)b;
    return a;
}

bool operator==(const float& a, const fixed32& b) {
    return fixed32(a) == b;
}

bool operator!=(const float& a, const fixed32& b) {
    return fixed32(a) != b;
}

bool operator<=(const float& a, const fixed32& b) {
    return fixed32(a) <= b;
}

bool operator>=(const float& a, const fixed32& b) {
    return fixed32(a) >= b;
}

bool operator>(const float& a, const fixed32& b) {
    return fixed32(a) > b;
}

bool operator<(const float& a, const fixed32& b) {
    return fixed32(a) < b;
}

/////////////////////////////////////

fixed32 operator+(const int& a, const fixed32& b) {
    fixed32 c;
    c.d = fixed32::fix(a + b.to_double());
    return c;
}

fixed32 operator-(const int& a, const fixed32& b) {
    fixed32 c;
    c.d = fixed32::fix(a - b.to_double());
    return c;
}

fixed32 operator/(const int& a, const fixed32& b) {
    fixed32 c;
    c.d = fixed32::fix(a / b.to_double());
    return c;
}

fixed32 operator*(const int& a, const fixed32& b) {
    fixed32 c;
    c.d = fixed32::fix(a * b.to_double());
    return c;
}

int& operator+=(int& a, const fixed32& b) {
    a += (int)b;
    return a;
}
int& operator-=(int& a, const fixed32& b) {
    a -= (int)b;
    return a;
}
int& operator/=(int& a, const fixed32& b) {
    a /= (int)b;
    return a;
}
int& operator*=(int& a, const fixed32& b) {
    a *= (int)b;
    return a;
}

bool operator==(const int& a, const fixed32& b) {
    return fixed32(a) == b;
}

bool operator!=(const int& a, const fixed32& b) {
    return fixed32(a) != b;
}

bool operator<=(const int& a, const fixed32& b) {
    return fixed32(a) <= b;
}

bool operator>=(const int& a, const fixed32& b) {
    return fixed32(a) >= b;
}

bool operator>(const int& a, const fixed32& b) {
    return fixed32(a) > b;
}

bool operator<(const int& a, const fixed32& b) {
    return fixed32(a) < b;
}

///////////////////////////////////////////

fixed32 operator+(const unsigned int& a, const fixed32& b) {
    fixed32 c;
    c.d = fixed32::fix(a + b.to_double());
    return c;
}

fixed32 operator-(const unsigned int& a, const fixed32& b) {
    fixed32 c;
    c.d = fixed32::fix(a - b.to_double());
    return c;
}

fixed32 operator/(const unsigned int& a, const fixed32& b) {
    fixed32 c;
    c.d = fixed32::fix(a / b.to_double());
    return c;
}

fixed32 operator*(const unsigned int& a, const fixed32& b) {
    fixed32 c;
    c.d = fixed32::fix(a * b.to_double());
    return c;
}

unsigned int& operator+=(unsigned int& a, const fixed32& b) {
    a += (unsigned int)b;
    return a;
}
unsigned int& operator-=(unsigned int& a, const fixed32& b) {
    a -= (unsigned int)b;
    return a;
}
unsigned int& operator/=(unsigned int& a, const fixed32& b) {
    a /= (unsigned int)b;
    return a;
}
unsigned int& operator*=(unsigned int& a, const fixed32& b) {
    a *= (unsigned int)b;
    return a;
}

bool operator==(const unsigned int& a, const fixed32& b) {
    return fixed32(a) == b;
}

bool operator!=(const unsigned int& a, const fixed32& b) {
    return fixed32(a) != b;
}

bool operator<=(const unsigned int& a, const fixed32& b) {
    return fixed32(a) <= b;
}

bool operator>=(const unsigned int& a, const fixed32& b) {
    return fixed32(a) >= b;
}

bool operator>(const unsigned int& a, const fixed32& b) {
    return fixed32(a) > b;
}

bool operator<(const unsigned int& a, const fixed32& b) {
    return fixed32(a) < b;
}

///////////////////////////////////////////

std::ostream& operator<< (std::ostream& stream, const fixed32& fixed) {
    return stream << fixed.to_double();
}

std::istream& operator>> (std::istream& stream, fixed32& fixed) {
    double temp;
    stream >> temp;
    fixed = fixed32(temp);

    return stream;
}



