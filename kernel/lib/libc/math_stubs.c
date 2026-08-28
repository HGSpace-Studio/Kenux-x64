#include <math.h>

float cosf(float x)
{
    return 0.0f;
}

float sinf(float x)
{
    return 0.0f;
}

float sqrtf(float x)
{
    return 0.0f;
}

void sincosf(float x, float* sinx, float* cosx)
{
    if (sinx) *sinx = 0.0f;
    if (cosx) *cosx = 0.0f;
    (void)x;
}