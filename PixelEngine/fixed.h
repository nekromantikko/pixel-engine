#pragma once
#include <cmath>
#include <cstdint>
#include <iostream>

struct fixed32 // Q24.8
{
public:
    fixed32() : d(0) {}
    fixed32(const fixed32& a) : d(a.d) {}
    fixed32(double a) : d(fix(a)) {}
    inline fixed32& operator=(const fixed32& a) {
        d = a.d;
        return *this;
    }
    inline fixed32& operator=(double a)  {
        d = fix(a);
        return *this;
    }
    ////////////////////////////
    inline fixed32 operator+(fixed32 a) const {
        fixed32 b;
        b.d = d + a.d;
        return b;
    }
    inline fixed32 operator-(fixed32 a) const {
        fixed32 b;
        b.d = d - a.d;
        return b;
    }
    inline fixed32 operator*(fixed32 a) const {
        fixed32 b;
        b.d = to_double() * a.d;
        return b;
    }
    inline fixed32 operator/(fixed32 a) const {
        fixed32 b;
        b.d = fix(d / a.d);
        return b;
    }
    ////////////////////////////
    inline fixed32 operator+(double a) const {
        return *this + fixed32(a);
    }
    inline fixed32 operator-(double a) const {
        return *this - fixed32(a);
    }
    inline fixed32 operator*(double a) const {
        return *this * fixed32(a);
    }
    inline fixed32 operator/(double a) const {
        return *this / fixed32(a);
    }
    ////////////////////////////
    inline fixed32 operator+(float a) const {
        return *this + fixed32(a);
    }
    inline fixed32 operator-(float a) const {
        return *this - fixed32(a);
    }
    inline fixed32 operator*(float a) const {
        return *this * fixed32(a);
    }
    inline fixed32 operator/(float a) const {
        return *this / fixed32(a);
    }
    ////////////////////////////
    inline fixed32 operator+(int a) const {
        return *this + fixed32(a);
    }
    inline fixed32 operator-(int a) const {
        return *this - fixed32(a);
    }
    inline fixed32 operator*(int a) const {
        return *this * fixed32(a);
    }
    inline fixed32 operator/(int a) const {
        return *this / fixed32(a);
    }
    ////////////////////////////
    inline fixed32 operator+(unsigned int a) const {
        return *this + fixed32(a);
    }
    inline fixed32 operator-(unsigned int a) const {
        return *this - fixed32(a);
    }
    inline fixed32 operator*(unsigned int a) const {
        return *this * fixed32(a);
    }
    inline fixed32 operator/(unsigned int a) const {
        return *this / fixed32(a);
    }
    ////////////////////////////
    inline fixed32& operator+=(fixed32 a) {
        return *this = *this + a;
    }
    inline fixed32& operator-=(fixed32 a) {
        return *this = *this - a;
    }
    inline fixed32& operator*=(fixed32 a) {
        return *this = *this * a;
    }
    inline fixed32& operator/=(fixed32 a) {
        return *this = *this / a;
    }
    ///////////////////////////
    inline fixed32& operator+=(double a) {
        return *this = *this + fixed32(a);
    }
    inline fixed32& operator-=(double a) {
        return *this = *this - fixed32(a);
    }
    inline fixed32& operator*=(double a) {
        return *this = *this * fixed32(a);
    }
    inline fixed32& operator/=(double a) {
        return *this = *this / fixed32(a);
    }
    ///////////////////////////
    inline fixed32& operator+=(float a) {
        return *this = *this + fixed32(a);
    }
    inline fixed32& operator-=(float a) {
        return *this = *this - fixed32(a);
    }
    inline fixed32& operator*=(float a) {
        return *this = *this * fixed32(a);
    }
    inline fixed32& operator/=(float a) {
        return *this = *this / fixed32(a);
    }
    ///////////////////////////
    inline fixed32& operator+=(int a) {
        return *this = *this + fixed32(a);
    }
    inline fixed32& operator-=(int a) {
        return *this = *this - fixed32(a);
    }
    inline fixed32& operator*=(int a) {
        return *this = *this * fixed32(a);
    }
    inline fixed32& operator/=(int a) {
        return *this = *this / fixed32(a);
    }
    ///////////////////////////
    inline fixed32& operator+=(unsigned int a) {
        return *this = *this + fixed32(a);
    }
    inline fixed32& operator-=(unsigned int a) {
        return *this = *this - fixed32(a);
    }
    inline fixed32& operator*=(unsigned int a) {
        return *this = *this * fixed32(a);
    }
    inline fixed32& operator/=(unsigned int a) {
        return *this = *this / fixed32(a);
    }
    ///////////////////////////
    inline bool operator==(const fixed32& a) const {
        return d == a.d;
    }
    inline bool operator!=(const fixed32& a) const {
        return d != a.d;
    }
    inline bool operator<=(const fixed32& a) const {
        return d <= a.d;
    }
    inline bool operator>=(const fixed32& a) const {
        return d >= a.d;
    }
    inline bool operator>(const fixed32& a) const {
        return d > a.d;
    }
    inline bool operator<(const fixed32& a) const {
        return d < a.d;
    }
    ///////////////////////////
    inline bool operator==(double a) const {
        return d == fix(a);
    }
    inline bool operator!=(double a) const {
        return d != fix(a);
    }
    inline bool operator<=(double a) const {
        return d <= fix(a);
    }
    inline bool operator>=(double a) const {
        return d >= fix(a);
    }
    inline bool operator>(double a) const {
        return d > fix(a);
    }
    inline bool operator<(double a) const {
        return d < fix(a);
    }
    ///////////////////////////
    inline bool operator==(float a) const {
        return d == fix(a);
    }
    inline bool operator!=(float a) const {
        return d != fix(a);
    }
    inline bool operator<=(float a) const {
        return d <= fix(a);
    }
    inline bool operator>=(float a) const {
        return d >= fix(a);
    }
    inline bool operator>(float a) const {
        return d > fix(a);
    }
    inline bool operator<(float a) const {
        return d < fix(a);
    }
    ///////////////////////////
    inline bool operator==(int a) const {
        return d == fix(a);
    }
    inline bool operator!=(int a) const {
        return d != fix(a);
    }
    inline bool operator<=(int a) const {
        return d <= fix(a);
    }
    inline bool operator>=(int a) const {
        return d >= fix(a);
    }
    inline bool operator>(int a) const {
        return d > fix(a);
    }
    inline bool operator<(int a) const {
        return d < fix(a);
    }
    ///////////////////////////
    inline bool operator==(unsigned int a) const {
        return d == fix(a);
    }
    inline bool operator!=(unsigned int a) const {
        return d != fix(a);
    }
    inline bool operator<=(unsigned int a) const {
        return d <= fix(a);
    }
    inline bool operator>=(unsigned int a) const {
        return d >= fix(a);
    }
    inline bool operator>(unsigned int a) const {
        return d > fix(a);
    }
    inline bool operator<(unsigned int a) const {
        return d < fix(a);
    }
    ///////////////////////////
    inline bool operator==(bool a) const {
        return bool(*this) == a;
    }
    inline bool operator!=(bool  a) const {
        return bool(*this) != a;
    }
    ///////////////////////////
    inline static int32_t fix(double a) {
        return std::round(a * 0x100);
    }
    inline double to_double() const {
        return (double)d / 0x100;
    }
    //////////////////////////
    inline operator bool() const {
        return d != 0;
    }
    inline operator double() const {
        return to_double();
    }
    inline operator float() const {
        return (float)to_double();
    }
    inline operator int() const {
        return (int)to_double();
    }
    inline operator unsigned int() const {
        return (unsigned int)to_double();
    }
    //////////////////////////
    inline fixed32 operator-() const {
        fixed32 result = *this;
        result.d = -d;
        return result;
    }
    //////////////////////////
    friend fixed32 operator+(const double& a, const fixed32& b);
    friend fixed32 operator-(const double& a, const fixed32& b);
    friend fixed32 operator/(const double& a, const fixed32& b);
    friend fixed32 operator*(const double& a, const fixed32& b);
    friend double& operator+=(double& a, const fixed32& b);
    friend double& operator-=(double& a, const fixed32& b);
    friend double& operator/=(double& a, const fixed32& b);
    friend double& operator*=(double& a, const fixed32& b);
    friend bool operator==(const double& a, const fixed32& b);
    friend bool operator!=(const double& a, const fixed32& b);
    friend bool operator<=(const double& a, const fixed32& b);
    friend bool operator>=(const double& a, const fixed32& b);
    friend bool operator>(const double& a, const fixed32& b);
    friend bool operator<(const double& a, const fixed32& b);
    friend fixed32 operator+(const float& a, const fixed32& b);
    friend fixed32 operator-(const float& a, const fixed32& b);
    friend fixed32 operator/(const float& a, const fixed32& b);
    friend fixed32 operator*(const float& a, const fixed32& b);
    friend float& operator+=(float& a, const fixed32& b);
    friend float& operator-=(float& a, const fixed32& b);
    friend float& operator/=(float& a, const fixed32& b);
    friend float& operator*=(float& a, const fixed32& b);
    friend bool operator==(const float& a, const fixed32& b);
    friend bool operator!=(const float& a, const fixed32& b);
    friend bool operator<=(const float& a, const fixed32& b);
    friend bool operator>=(const float& a, const fixed32& b);
    friend bool operator>(const float& a, const fixed32& b);
    friend bool operator<(const float& a, const fixed32& b);
    friend fixed32 operator+(const int& a, const fixed32& b);
    friend fixed32 operator-(const int& a, const fixed32& b);
    friend fixed32 operator/(const int& a, const fixed32& b);
    friend fixed32 operator*(const int& a, const fixed32& b);
    friend int& operator+=(int& a, const fixed32& b);
    friend int& operator-=(int& a, const fixed32& b);
    friend int& operator/=(int& a, const fixed32& b);
    friend int& operator*=(int& a, const fixed32& b);
    friend bool operator==(const int& a, const fixed32& b);
    friend bool operator!=(const int& a, const fixed32& b);
    friend bool operator<=(const int& a, const fixed32& b);
    friend bool operator>=(const int& a, const fixed32& b);
    friend bool operator>(const int& a, const fixed32& b);
    friend bool operator<(const int& a, const fixed32& b);
    friend fixed32 operator+(const unsigned int& a, const fixed32& b);
    friend fixed32 operator-(const unsigned int& a, const fixed32& b);
    friend fixed32 operator/(const unsigned int& a, const fixed32& b);
    friend fixed32 operator*(const unsigned int& a, const fixed32& b);
    friend unsigned int& operator+=(unsigned int& a, const fixed32& b);
    friend unsigned int& operator-=(unsigned int& a, const fixed32& b);
    friend unsigned int& operator/=(unsigned int& a, const fixed32& b);
    friend unsigned int& operator*=(unsigned int& a, const fixed32& b);
    friend bool operator==(const unsigned int& a, const fixed32& b);
    friend bool operator!=(const unsigned int& a, const fixed32& b);
    friend bool operator<=(const unsigned int& a, const fixed32& b);
    friend bool operator>=(const unsigned int& a, const fixed32& b);
    friend bool operator>(const unsigned int& a, const fixed32& b);
    friend bool operator<(const unsigned int& a, const fixed32& b);
private:
    int32_t d;
};

extern fixed32 operator+(const double& a, const fixed32& b);
extern fixed32 operator-(const double& a, const fixed32& b);
extern fixed32 operator/(const double& a, const fixed32& b);
extern fixed32 operator*(const double& a, const fixed32& b);
extern double& operator+=(double& a, const fixed32& b);
extern double& operator-=(double& a, const fixed32& b);
extern double& operator/=(double& a, const fixed32& b);
extern double& operator*=(double& a, const fixed32& b);
extern bool operator==(const double& a, const fixed32& b);
extern bool operator!=(const double& a, const fixed32& b);
extern bool operator<=(const double& a, const fixed32& b);
extern bool operator>=(const double& a, const fixed32& b);
extern bool operator>(const double& a, const fixed32& b);
extern bool operator<(const double& a, const fixed32& b);

extern fixed32 operator+(const float& a, const fixed32& b);
extern fixed32 operator-(const float& a, const fixed32& b);
extern fixed32 operator/(const float& a, const fixed32& b);
extern fixed32 operator*(const float& a, const fixed32& b);
extern float& operator+=(float& a, const fixed32& b);
extern float& operator-=(float& a, const fixed32& b);
extern float& operator/=(float& a, const fixed32& b);
extern float& operator*=(float& a, const fixed32& b);
extern bool operator==(const float& a, const fixed32& b);
extern bool operator!=(const float& a, const fixed32& b);
extern bool operator<=(const float& a, const fixed32& b);
extern bool operator>=(const float& a, const fixed32& b);
extern bool operator>(const float& a, const fixed32& b);
extern bool operator<(const float& a, const fixed32& b);

extern fixed32 operator+(const int& a, const fixed32& b);
extern fixed32 operator-(const int& a, const fixed32& b);
extern fixed32 operator/(const int& a, const fixed32& b);
extern fixed32 operator*(const int& a, const fixed32& b);
extern int& operator+=(int& a, const fixed32& b);
extern int& operator-=(int& a, const fixed32& b);
extern int& operator/=(int& a, const fixed32& b);
extern int& operator*=(int& a, const fixed32& b);
extern bool operator==(const int& a, const fixed32& b);
extern bool operator!=(const int& a, const fixed32& b);
extern bool operator<=(const int& a, const fixed32& b);
extern bool operator>=(const int& a, const fixed32& b);
extern bool operator>(const int& a, const fixed32& b);
extern bool operator<(const int& a, const fixed32& b);

extern fixed32 operator+(const unsigned int& a, const fixed32& b);
extern fixed32 operator-(const unsigned int& a, const fixed32& b);
extern fixed32 operator/(const unsigned int& a, const fixed32& b);
extern fixed32 operator*(const unsigned int& a, const fixed32& b);
extern unsigned int& operator+=(unsigned int& a, const fixed32& b);
extern unsigned int& operator-=(unsigned int& a, const fixed32& b);
extern unsigned int& operator/=(unsigned int& a, const fixed32& b);
extern unsigned int& operator*=(unsigned int& a, const fixed32& b);
extern bool operator==(const unsigned int& a, const fixed32& b);
extern bool operator!=(const unsigned int& a, const fixed32& b);
extern bool operator<=(const unsigned int& a, const fixed32& b);
extern bool operator>=(const unsigned int& a, const fixed32& b);
extern bool operator>(const unsigned int& a, const fixed32& b);
extern bool operator<(const unsigned int& a, const fixed32& b);


extern std::ostream& operator<< (std::ostream& stream, const fixed32& fixed);
extern std::istream& operator>> (std::istream& stream, fixed32& fixed);