#include "math.h"

/* 数学常量 */
static const double PI      = 3.14159265358979323846;
static const double PI_2    = 1.57079632679489661923;
static const double PI_4    = 0.78539816339744830962;
static const double LOG10E  = 0.43429448190325182765;
static const double E       = 2.71828182845904523536;

/* 绝对值 */
double fabs(double x)
{
    return x < 0.0 ? -x : x;
}

/* 平方根：牛顿迭代法 */
double sqrt(double x)
{
    if (x < 0.0) return 0.0;
    if (x == 0.0 || x == 1.0) return x;

    double guess = x;
    if (x > 1.0) guess = x * 0.5;
    else guess = 1.0;

    for (int i = 0; i < 64; i++) {
        double next = 0.5 * (guess + x / guess);
        if (fabs(next - guess) < 1e-15) break;
        guess = next;
    }
    return guess;
}

/* 将角度归约到 [-pi, pi] */
static double reduce_pi(double x)
{
    if (x == 0.0) return 0.0;
    double two_pi = 2.0 * PI;
    /* 先用 fmod 粗归约 */
    x = x - two_pi * (long long)(x / two_pi);
    /* 精调到 [-pi, pi] */
    while (x > PI)  x -= two_pi;
    while (x < -PI) x += two_pi;
    return x;
}

/* 正弦：泰勒级数展开 */
double sin(double x)
{
    x = reduce_pi(x);
    double term = x;
    double sum  = x;
    double x2   = x * x;
    for (int n = 1; n <= 20; n++) {
        term *= -x2 / ((2.0 * n) * (2.0 * n + 1.0));
        sum += term;
        if (fabs(term) < 1e-16) break;
    }
    return sum;
}

/* 余弦：利用 cos(x) = sin(x + pi/2) */
double cos(double x)
{
    return sin(x + PI_2);
}

/* 正切：sin(x)/cos(x) */
double tan(double x)
{
    double c = cos(x);
    if (fabs(c) < 1e-16) {
        /* cos 接近 0，返回无穷大 */
        return (sin(x) >= 0.0) ? 1.0 / 0.0 : -1.0 / 0.0;
    }
    return sin(x) / c;
}

/* 反正弦：泰勒级数展开，|x| <= 1 */
double asin(double x)
{
    if (x < -1.0) return -PI_2;
    if (x > 1.0)  return PI_2;
    if (x == 1.0)  return PI_2;
    if (x == -1.0) return -PI_2;

    double absx = fabs(x);
    /* 对 |x| 接近 1 的情况，用恒等式提高精度 */
    if (absx > 0.9) {
        double s = sqrt(1.0 - x * x);
        return (x >= 0.0) ? PI_2 - asin(s) : -PI_2 + asin(s);
    }

    double term = x;
    double sum  = x;
    double x2   = x * x;
    for (int n = 1; n <= 40; n++) {
        term *= x2 * (2.0 * n - 1.0) * (2.0 * n - 1.0)
                / ((2.0 * n) * (2.0 * n + 1.0));
        sum += term;
        if (fabs(term) < 1e-16) break;
    }
    return sum;
}

/* 反余弦：pi/2 - asin(x) */
double acos(double x)
{
    return PI_2 - asin(x);
}

/* 反正切：泰勒级数 */
double atan(double x)
{
    if (x > 1.0)  return PI_2 - atan(1.0 / x);
    if (x < -1.0) return -PI_2 - atan(1.0 / x);
    if (x == 1.0)  return PI_4;
    if (x == -1.0) return -PI_4;

    double term = x;
    double sum  = x;
    double x2   = x * x;
    for (int n = 1; n <= 40; n++) {
        term *= -x2;
        sum += term / (2.0 * n + 1.0);
        if (fabs(term) < 1e-16) break;
    }
    return sum;
}

/* atan2：根据象限调用 atan */
double atan2(double y, double x)
{
    if (x > 0.0) return atan(y / x);
    if (x < 0.0) {
        if (y >= 0.0) return atan(y / x) + PI;
        return atan(y / x) - PI;
    }
    /* x == 0 */
    if (y > 0.0)  return PI_2;
    if (y < 0.0)  return -PI_2;
    return 0.0;
}

/* 双曲正弦：(e^x - e^(-x)) / 2 */
double sinh(double x)
{
    double ex = exp(x);
    double emx = exp(-x);
    return 0.5 * (ex - emx);
}

/* 双曲余弦：(e^x + e^(-x)) / 2 */
double cosh(double x)
{
    double ex = exp(x);
    double emx = exp(-x);
    return 0.5 * (ex + emx);
}

/* 双曲正切：sinh(x) / cosh(x) */
double tanh(double x)
{
    double ex = exp(2.0 * x);
    return (ex - 1.0) / (ex + 1.0);
}

/* 指数：泰勒级数 */
double exp(double x)
{
    if (x == 0.0) return 1.0;
    /* 处理大数值，先分解为 e^x = 2^k * e^r */
    long long k = (long long)(x / 0.69314718055994530942); /* ln(2) */
    double r = x - k * 0.69314718055994530942;

    double term = 1.0;
    double sum  = 1.0;
    for (int n = 1; n <= 40; n++) {
        term *= r / n;
        sum += term;
        if (fabs(term) < 1e-16) break;
    }
    /* 乘以 2^k */
    for (long long i = 0; i < k; i++) sum *= 2.0;
    for (long long i = 0; i > k; i--) sum *= 0.5;
    return sum;
}

/* 自然对数：牛顿迭代法 */
double log(double x)
{
    if (x <= 0.0) return -1.0 / 0.0;
    if (x == 1.0) return 0.0;

    /* 归约到 [1, 2) 区间：x = 2^k * m，其中 m 在 [1, 2) */
    long long k = 0;
    double m = x;
    while (m >= 2.0) { m *= 0.5; k++; }
    while (m < 1.0)  { m *= 2.0; k--; }

    /* ln(m) 用牛顿迭代求解 e^y = m，即 y = y + 2*(m - e^y)/(m + e^y) */
    double y = m - 1.0; /* 初始猜测 */
    for (int i = 0; i < 32; i++) {
        double ey = exp(y);
        double next = y + 2.0 * (m - ey) / (m + ey);
        if (fabs(next - y) < 1e-15) break;
        y = next;
    }
    /* ln(x) = ln(m) + k * ln(2) */
    return y + k * 0.69314718055994530942;
}

/* 常用对数 */
double log10(double x)
{
    return log(x) * LOG10E;
}

/* 幂：exp(y * log(x)) */
double pow(double x, double y)
{
    if (y == 0.0) return 1.0;
    if (x == 0.0) return 0.0;
    if (x < 0.0) return 0.0; /* 实数范围内负数底数未定义 */
    return exp(y * log(x));
}

/* 向上取整 */
double ceil(double x)
{
    long long i = (long long)x;
    if (x > 0.0 && x != (double)i) return (double)(i + 1);
    return (double)i;
}

/* 向下取整 */
double floor(double x)
{
    long long i = (long long)x;
    if (x < 0.0 && x != (double)i) return (double)(i - 1);
    return (double)i;
}

/* 浮点取模：x - trunc(x/y)*y */
double fmod(double x, double y)
{
    if (y == 0.0) return 0.0;
    double q = x / y;
    /* C 的整数转换本身就是向零截断（trunc） */
    long long n = (long long)q;
    return x - (double)n * y;
}
