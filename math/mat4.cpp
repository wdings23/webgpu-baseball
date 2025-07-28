#include <stdint.h>
#include <math.h>

#ifdef _MSC_VER
#include <corecrt_math_defines.h>
#endif // MSC_VER

#include "mat4.h"
#include "quaternion.h"

#include <float.h>

/*
**
*/
mat4 invert(mat4 const& m)
{
    float inv[16], invOut[16], det;
    int i;
    
    inv[0] = m.mafEntries[5]  * m.mafEntries[10] * m.mafEntries[15] -
    m.mafEntries[5]  * m.mafEntries[11] * m.mafEntries[14] -
    m.mafEntries[9]  * m.mafEntries[6]  * m.mafEntries[15] +
    m.mafEntries[9]  * m.mafEntries[7]  * m.mafEntries[14] +
    m.mafEntries[13] * m.mafEntries[6]  * m.mafEntries[11] -
    m.mafEntries[13] * m.mafEntries[7]  * m.mafEntries[10];
    
    inv[4] = -m.mafEntries[4]  * m.mafEntries[10] * m.mafEntries[15] +
    m.mafEntries[4]  * m.mafEntries[11] * m.mafEntries[14] +
    m.mafEntries[8]  * m.mafEntries[6]  * m.mafEntries[15] -
    m.mafEntries[8]  * m.mafEntries[7]  * m.mafEntries[14] -
    m.mafEntries[12] * m.mafEntries[6]  * m.mafEntries[11] +
    m.mafEntries[12] * m.mafEntries[7]  * m.mafEntries[10];
    
    inv[8] = m.mafEntries[4]  * m.mafEntries[9] * m.mafEntries[15] -
    m.mafEntries[4]  * m.mafEntries[11] * m.mafEntries[13] -
    m.mafEntries[8]  * m.mafEntries[5] * m.mafEntries[15] +
    m.mafEntries[8]  * m.mafEntries[7] * m.mafEntries[13] +
    m.mafEntries[12] * m.mafEntries[5] * m.mafEntries[11] -
    m.mafEntries[12] * m.mafEntries[7] * m.mafEntries[9];
    
    inv[12] = -m.mafEntries[4]  * m.mafEntries[9] * m.mafEntries[14] +
    m.mafEntries[4]  * m.mafEntries[10] * m.mafEntries[13] +
    m.mafEntries[8]  * m.mafEntries[5] * m.mafEntries[14] -
    m.mafEntries[8]  * m.mafEntries[6] * m.mafEntries[13] -
    m.mafEntries[12] * m.mafEntries[5] * m.mafEntries[10] +
    m.mafEntries[12] * m.mafEntries[6] * m.mafEntries[9];
    
    inv[1] = -m.mafEntries[1]  * m.mafEntries[10] * m.mafEntries[15] +
    m.mafEntries[1]  * m.mafEntries[11] * m.mafEntries[14] +
    m.mafEntries[9]  * m.mafEntries[2] * m.mafEntries[15] -
    m.mafEntries[9]  * m.mafEntries[3] * m.mafEntries[14] -
    m.mafEntries[13] * m.mafEntries[2] * m.mafEntries[11] +
    m.mafEntries[13] * m.mafEntries[3] * m.mafEntries[10];
    
    inv[5] = m.mafEntries[0]  * m.mafEntries[10] * m.mafEntries[15] -
    m.mafEntries[0]  * m.mafEntries[11] * m.mafEntries[14] -
    m.mafEntries[8]  * m.mafEntries[2] * m.mafEntries[15] +
    m.mafEntries[8]  * m.mafEntries[3] * m.mafEntries[14] +
    m.mafEntries[12] * m.mafEntries[2] * m.mafEntries[11] -
    m.mafEntries[12] * m.mafEntries[3] * m.mafEntries[10];
    
    inv[9] = -m.mafEntries[0]  * m.mafEntries[9] * m.mafEntries[15] +
    m.mafEntries[0]  * m.mafEntries[11] * m.mafEntries[13] +
    m.mafEntries[8]  * m.mafEntries[1] * m.mafEntries[15] -
    m.mafEntries[8]  * m.mafEntries[3] * m.mafEntries[13] -
    m.mafEntries[12] * m.mafEntries[1] * m.mafEntries[11] +
    m.mafEntries[12] * m.mafEntries[3] * m.mafEntries[9];
    
    inv[13] = m.mafEntries[0]  * m.mafEntries[9] * m.mafEntries[14] -
    m.mafEntries[0]  * m.mafEntries[10] * m.mafEntries[13] -
    m.mafEntries[8]  * m.mafEntries[1] * m.mafEntries[14] +
    m.mafEntries[8]  * m.mafEntries[2] * m.mafEntries[13] +
    m.mafEntries[12] * m.mafEntries[1] * m.mafEntries[10] -
    m.mafEntries[12] * m.mafEntries[2] * m.mafEntries[9];
    
    inv[2] = m.mafEntries[1]  * m.mafEntries[6] * m.mafEntries[15] -
    m.mafEntries[1]  * m.mafEntries[7] * m.mafEntries[14] -
    m.mafEntries[5]  * m.mafEntries[2] * m.mafEntries[15] +
    m.mafEntries[5]  * m.mafEntries[3] * m.mafEntries[14] +
    m.mafEntries[13] * m.mafEntries[2] * m.mafEntries[7] -
    m.mafEntries[13] * m.mafEntries[3] * m.mafEntries[6];
    
    inv[6] = -m.mafEntries[0]  * m.mafEntries[6] * m.mafEntries[15] +
    m.mafEntries[0]  * m.mafEntries[7] * m.mafEntries[14] +
    m.mafEntries[4]  * m.mafEntries[2] * m.mafEntries[15] -
    m.mafEntries[4]  * m.mafEntries[3] * m.mafEntries[14] -
    m.mafEntries[12] * m.mafEntries[2] * m.mafEntries[7] +
    m.mafEntries[12] * m.mafEntries[3] * m.mafEntries[6];
    
    inv[10] = m.mafEntries[0]  * m.mafEntries[5] * m.mafEntries[15] -
    m.mafEntries[0]  * m.mafEntries[7] * m.mafEntries[13] -
    m.mafEntries[4]  * m.mafEntries[1] * m.mafEntries[15] +
    m.mafEntries[4]  * m.mafEntries[3] * m.mafEntries[13] +
    m.mafEntries[12] * m.mafEntries[1] * m.mafEntries[7] -
    m.mafEntries[12] * m.mafEntries[3] * m.mafEntries[5];
    
    inv[14] = -m.mafEntries[0]  * m.mafEntries[5] * m.mafEntries[14] +
    m.mafEntries[0]  * m.mafEntries[6] * m.mafEntries[13] +
    m.mafEntries[4]  * m.mafEntries[1] * m.mafEntries[14] -
    m.mafEntries[4]  * m.mafEntries[2] * m.mafEntries[13] -
    m.mafEntries[12] * m.mafEntries[1] * m.mafEntries[6] +
    m.mafEntries[12] * m.mafEntries[2] * m.mafEntries[5];
    
    inv[3] = -m.mafEntries[1] * m.mafEntries[6] * m.mafEntries[11] +
    m.mafEntries[1] * m.mafEntries[7] * m.mafEntries[10] +
    m.mafEntries[5] * m.mafEntries[2] * m.mafEntries[11] -
    m.mafEntries[5] * m.mafEntries[3] * m.mafEntries[10] -
    m.mafEntries[9] * m.mafEntries[2] * m.mafEntries[7] +
    m.mafEntries[9] * m.mafEntries[3] * m.mafEntries[6];
    
    inv[7] = m.mafEntries[0] * m.mafEntries[6] * m.mafEntries[11] -
    m.mafEntries[0] * m.mafEntries[7] * m.mafEntries[10] -
    m.mafEntries[4] * m.mafEntries[2] * m.mafEntries[11] +
    m.mafEntries[4] * m.mafEntries[3] * m.mafEntries[10] +
    m.mafEntries[8] * m.mafEntries[2] * m.mafEntries[7] -
    m.mafEntries[8] * m.mafEntries[3] * m.mafEntries[6];
    
    inv[11] = -m.mafEntries[0] * m.mafEntries[5] * m.mafEntries[11] +
    m.mafEntries[0] * m.mafEntries[7] * m.mafEntries[9] +
    m.mafEntries[4] * m.mafEntries[1] * m.mafEntries[11] -
    m.mafEntries[4] * m.mafEntries[3] * m.mafEntries[9] -
    m.mafEntries[8] * m.mafEntries[1] * m.mafEntries[7] +
    m.mafEntries[8] * m.mafEntries[3] * m.mafEntries[5];
    
    inv[15] = m.mafEntries[0] * m.mafEntries[5] * m.mafEntries[10] -
    m.mafEntries[0] * m.mafEntries[6] * m.mafEntries[9] -
    m.mafEntries[4] * m.mafEntries[1] * m.mafEntries[10] +
    m.mafEntries[4] * m.mafEntries[2] * m.mafEntries[9] +
    m.mafEntries[8] * m.mafEntries[1] * m.mafEntries[6] -
    m.mafEntries[8] * m.mafEntries[2] * m.mafEntries[5];
    
    det = m.mafEntries[0] * inv[0] + m.mafEntries[1] * inv[4] + m.mafEntries[2] * inv[8] + m.mafEntries[3] * inv[12];
    if(fabsf(det) <= 1.0e-5)
    {
        for(i = 0; i < 16; i++)
            invOut[i] = FLT_MAX;
    }
    else
    {
        det = 1.0f / det;

        for(i = 0; i < 16; i++)
            invOut[i] = inv[i] * det;
    }
    
    return mat4(invOut);
}

/*
**
*/
void mul(mat4* pResult, mat4 const& m0, mat4 const& m1)
{
    for(uint32_t i = 0; i < 4; i++)
    {
        for(uint32_t j = 0; j < 4; j++)
        {
            float fResult = 0.0f;
            for(uint32_t k = 0; k < 4; k++)
            {
                uint32_t iIndex0 = (i << 2) + k;
                uint32_t iIndex1 = (k << 2) + j;
                fResult += (m0.mafEntries[iIndex0] * m1.mafEntries[iIndex1]);
            }
            
            pResult->mafEntries[(i << 2) + j] = fResult;
        }
    }
}

/*
**
*/
void mul(mat4& result, mat4 const& m0, mat4 const& m1)
{
    for(uint32_t i = 0; i < 4; i++)
    {
        for(uint32_t j = 0; j < 4; j++)
        {
            float fResult = 0.0f;
            for(uint32_t k = 0; k < 4; k++)
            {
                uint32_t iIndex0 = (i << 2) + k;
                uint32_t iIndex1 = (k << 2) + j;
                fResult += (m0.mafEntries[iIndex0] * m1.mafEntries[iIndex1]);
            }

            result.mafEntries[(i << 2) + j] = fResult;
        }
    }
}

/*
**
*/
vec4 mul(float4 const& v, mat4 const& m)
{
    return m * v;
}

/*
**
*/
mat4 translate(float fX, float fY, float fZ)
{
    float afVal[16] =
    {
        1.0f, 0.0f, 0.0f, fX,
        0.0f, 1.0f, 0.0f, fY,
        0.0f, 0.0f, 1.0f, fZ,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    
    return mat4(afVal);
}

/*
**
*/
mat4 translate(vec4 const& position)
{
    float afVal[16] =
    {
        1.0f, 0.0f, 0.0f, position.x,
        0.0f, 1.0f, 0.0f, position.y,
        0.0f, 0.0f, 1.0f, position.z,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    
    return mat4(afVal);
}

/*
**
*/
mat4 transpose(mat4 const& m)
{
    float afVal[16] =
    {
        m.mafEntries[0], m.mafEntries[4], m.mafEntries[8], m.mafEntries[12],
        m.mafEntries[1], m.mafEntries[5], m.mafEntries[9], m.mafEntries[13],
        m.mafEntries[2], m.mafEntries[6], m.mafEntries[10], m.mafEntries[14],
        m.mafEntries[3], m.mafEntries[7], m.mafEntries[11], m.mafEntries[15],
    };
    
    return mat4(afVal);
}

/*
**
*/
mat4 mat4::operator * (mat4 const& m) const
{
    float afResults[16];
    
    for(uint32_t i = 0; i < 4; i++)
    {
        for(uint32_t j = 0; j < 4; j++)
        {
            uint32_t iIndex = (i << 2) + j;
            afResults[iIndex] = 0.0f;
            for(uint32_t k = 0; k < 4; k++)
            {
                uint32_t iIndex0 = (i << 2) + k;
                uint32_t iIndex1 = (k << 2) + j;
                afResults[iIndex] += (mafEntries[iIndex0] * m.mafEntries[iIndex1]);
            }
        }
    }
    
    return mat4(afResults);
}

/*
**
*/
mat4 mat4::operator + (mat4 const& m) const
{
    float afResults[16];

    for(uint32_t i = 0; i < 16; i++)
    {
        afResults[i] = mafEntries[i] + m.mafEntries[i];
    }

    return mat4(afResults);
}

/*
**
*/
void mat4::operator += (mat4 const& m)
{
    for(uint32_t i = 0; i < 16; i++)
    {
        mafEntries[i] += m.mafEntries[i];
    }
}

/*
**
*/
mat4 perspectiveProjection(float fFOV,
                           uint32_t iWidth,
                           uint32_t iHeight,
                           float fFar,
                           float fNear)
{
    float fFD = 1.0f / tanf(fFOV * 0.5f);
    float fAspect = (float)iWidth / (float)iHeight;
    float fOneOverAspect = 1.0f / fAspect;
    float fOneOverFarMinusNear = 1.0f / (fFar - fNear);
    
    float afVal[16];
    memset(afVal, 0, sizeof(afVal));
    afVal[0] = -fFD * fOneOverAspect;
    afVal[5] = -fFD;
    afVal[10] = -(fFar + fNear) * fOneOverFarMinusNear;
    afVal[14] = -1.0f;
    afVal[11] = -2.0f * fFar * fNear * fOneOverFarMinusNear;
    afVal[15] = 0.0f;
    
#if defined(__APPLE__) || defined(TARGET_IOS)
    afVal[10] = -fFar * fOneOverFarMinusNear;
    afVal[11] = -fFar * fNear * fOneOverFarMinusNear;
#else
#if !defined(GLES_RENDER)
	afVal[5] *= -1.0f;
#endif // GLES_RENDER

#endif // __APPLE__
    
    return mat4(afVal);
}

/*
**
*/
mat4 perspectiveProjectionNegOnePosOne(
    float fFOV,
    uint32_t iWidth,
    uint32_t iHeight,
    float fFar,
    float fNear)
{
    float fFD = 1.0f / tanf(fFOV * 0.5f);
    float fAspect = (float)iWidth / (float)iHeight;
    float fOneOverAspect = 1.0f / fAspect;
    float fOneOverFarMinusNear = 1.0f / (fFar - fNear);

    float afVal[16];
    memset(afVal, 0, sizeof(afVal));
    afVal[0] = fFD * fOneOverAspect;
    afVal[5] = fFD;
    afVal[10] = -(fFar + fNear) * fOneOverFarMinusNear;
    afVal[11] = -1.0f;
    afVal[14] = -2.0f * fFar * fNear * fOneOverFarMinusNear;
    afVal[15] = 0.0f;

    return mat4(afVal);
}

/*
**
*/
mat4 perspectiveProjection2(
    float fFOV,
    uint32_t iWidth,
    uint32_t iHeight,
    float fFar,
    float fNear)
{
    float fFD = 1.0f / tanf(fFOV * 0.5f);
    float fAspect = (float)iWidth / (float)iHeight;
    float fOneOverAspect = 1.0f / fAspect;
    float fOneOverFarMinusNear = 1.0f / (fFar - fNear);

    float afVal[16];
    memset(afVal, 0, sizeof(afVal));
    afVal[0] = -fFD * fOneOverAspect;
    afVal[5] = fFD;
    afVal[10] = -fFar * fOneOverFarMinusNear;
    afVal[11] = fFar * fNear * fOneOverFarMinusNear;
    afVal[14] = -1.0f;
    afVal[15] = 0.0f;

    return mat4(afVal);
}

/*
**
*/
mat4 orthographicProjection(float fLeft,
                            float fRight,
                            float fTop,
                            float fBottom,
                            float fFar,
                            float fNear,
                            bool bInvertY)
{
    float fWidth = fRight - fLeft;
    float fHeight = fTop - fBottom;

    float fFarMinusNear = fFar - fNear;

    float afVal[16];
    memset(afVal, 0, sizeof(afVal));

    afVal[0] = -2.0f / fWidth;
    afVal[3] = -(fRight + fLeft) / (fRight - fLeft);
    afVal[5] = 2.0f / fHeight;
    afVal[7] = -(fTop + fBottom) / (fTop - fBottom);

    afVal[10] = -1.0f / fFarMinusNear;
    afVal[11] = -fNear / fFarMinusNear;

    afVal[15] = 1.0f;

    if(bInvertY)
    {
        afVal[5] = -afVal[5];
    }

    afVal[0] = -afVal[0];

    return mat4(afVal);
}

/*
**
*/
mat4 makeViewMatrix(vec3 const& eyePos, vec3 const& lookAt, vec3 const& up)
{
    vec3 dir = lookAt - eyePos;
    dir = normalize(dir);
    
    vec3 tangent = normalize(cross(up, dir));
    vec3 binormal = normalize(cross(dir, tangent));
    
    float afValue[16] =
    {
        tangent.x, tangent.y, tangent.z, 0.0f,
        binormal.x, binormal.y, binormal.z, 0.0f,
        -dir.x, -dir.y, -dir.z, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    
    mat4 xform(afValue);
    
    mat4 translation;
    translation.mafEntries[3] = -eyePos.x;
    translation.mafEntries[7] = -eyePos.y;
    translation.mafEntries[11] = -eyePos.z;
    
    return (xform * translation);
}

/*
**
*/
mat4 makeViewMatrix2(vec3 const& eyePos, vec3 const& lookAt, vec3 const& up)
{
    vec3 dir = lookAt - eyePos;
    dir = normalize(dir);

    vec3 tangent = normalize(cross(up, dir));
    vec3 binormal = normalize(cross(dir, tangent));

    float afValue[16] =
    {
        tangent.x, tangent.y, tangent.z, 0.0f,
        binormal.x, binormal.y, binormal.z, 0.0f,
        dir.x, dir.y, dir.z, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };

    mat4 xform(afValue);

    mat4 translation;
    translation.mafEntries[3] = -eyePos.x;
    translation.mafEntries[7] = -eyePos.y;
    translation.mafEntries[11] = -eyePos.z;

    //return (translation * xform);
    return (xform * translation);
}

/*
**
*/
mat4 rotateMatrixX(float fAngle)
{
    float afVal[16] =
    {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, cosf(fAngle), -sinf(fAngle), 0.0f,
        0.0f, sinf(fAngle), cosf(fAngle), 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    
    return mat4(afVal);
}

/*
**
*/
mat4 rotateMatrixY(float fAngle)
{
    float afVal[16] =
    {
        cosf(fAngle), 0.0f, sinf(fAngle), 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        -sinf(fAngle), 0.0f, cosf(fAngle), 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    
    return mat4(afVal);
}

/*
**
*/
mat4 rotateMatrixZ(float fAngle)
{
    float afVal[16] =
    {
        cosf(fAngle), -sinf(fAngle), 0.0f, 0.0f,
        sinf(fAngle), cosf(fAngle), 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    
    return mat4(afVal);
}

/*
**
*/
mat4 scale(float fX, float fY, float fZ)
{
    float afVal[16] =
    {
        fX, 0.0f, 0.0f, 0.0f,
        0.0f, fY, 0.0f, 0.0f,
        0.0f, 0.0f, fZ, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    
    return mat4(afVal);
}

/*
**
*/
mat4 scale(vec4 const& scale)
{
    float afVal[16] =
    {
        scale.x, 0.0f, 0.0f, 0.0f,
        0.0f, scale.y, 0.0f, 0.0f,
        0.0f, 0.0f, scale.z, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    
    return mat4(afVal);
}

/*
**
*/
bool mat4::operator == (mat4 const& m) const
{
    bool bRet = true;
    for(uint32_t i = 0; i < 16; i++)
    {
        float fDiff = fabsf(m.mafEntries[i] - mafEntries[i]);
        if(fDiff > 0.0001f)
        {
            bRet = false;
            break;
        }
    }
    
    return bRet;
}

/*
**
*/
vec3 extractEulerAngles(mat4 const& m)
{
	vec3 ret;

	float const fTwoPI = 2.0f * (float)M_PI;

	float fSY = sqrtf(m.mafEntries[0] * m.mafEntries[0] + m.mafEntries[4] * m.mafEntries[4]);   // (0,0), (1,0)

	if(fSY < 1e-6)
	{
		ret.x = atan2f(-m.mafEntries[6], m.mafEntries[5]);     // (1,2) (1,1)
		ret.y = atan2f(-m.mafEntries[8], fSY);      // (2,0)
		ret.z = 0.0f;
}
	else
	{
		ret.x = atan2f(m.mafEntries[9], m.mafEntries[10]);    // (2,1) (2,2)
		ret.y = atan2f(-m.mafEntries[8], fSY);     // (2,0)
		ret.z = atan2f(m.mafEntries[4], m.mafEntries[0]);     // (1,0) (0,0)
	}

	return ret;
}

/*
**
*/
bool mat4::identical(mat4 const& m, float fTolerance) const
{
	bool bRet = true;
	for(uint32_t i = 0; i < 16; i++)
	{
		float fDiff = fabsf(mafEntries[i] - m.mafEntries[i]);
		if(fDiff > fTolerance)
		{
			bRet = false;
			break;
		}
	}

	return bRet;
}

/*
**
*/
mat4 makeFromAngleAxis(vec3 const& axis, float fAngle)
{
    float fCosAngle = cosf(fAngle);
    float fSinAngle = sinf(fAngle);
    float fT = 1.0f - fCosAngle;

    mat4 m;
    m.mafEntries[0] = fT * axis.x * axis.x + fCosAngle;
    m.mafEntries[5] = fT * axis.y * axis.y + fCosAngle;
    m.mafEntries[10] = fT * axis.z * axis.z + fCosAngle;

    float fTemp0 = axis.x * axis.y * fT;
    float fTemp1 = axis.z * fSinAngle;

    m.mafEntries[4] = fTemp0 + fTemp1;
    m.mafEntries[1] = fTemp0 - fTemp1;

    fTemp0 = axis.x * axis.z * fT;
    fTemp1 = axis.y * fSinAngle;

    m.mafEntries[8] = fTemp0 - fTemp1;
    m.mafEntries[2] = fTemp0 + fTemp1;

    fTemp0 = axis.y * axis.z * fT;
    fTemp1 = axis.x * fSinAngle;

    m.mafEntries[9] = fTemp0 + fTemp1;
    m.mafEntries[6] = fTemp0 - fTemp1;
    

    return m;
}

/*
**
*/
mat4 makeRotation(
    vec3 const& src,
    vec3 const& dst)
{
    vec3 v = cross(src, dst);
    float s = length(v);
    float c = dot(src, dst);

    if(s == 0)
    {
        // Vectors are parallel or opposite
        if(c > 0)
        {
            // No rotation
            return float4x4();
        }
        else
        {
            // 180 degree rotation about any orthogonal axis
            float3 axis = (src.x != 0.0f || src.y != 0.0f) ? normalize(float3(-src.y, src.x, 0.0f)) : normalize(float3(0.0f, -src.z, src.y));
            float x = axis.x, y = axis.y, z = axis.z;

            float3 row0 = float3(2.0f * x * x - 1.0f, 2.0f * x * y, 2.0f * x * z);
            float3 row1 = float3(2.0f * x * y, 2.0f * y * y - 1.0f, 2.0f * y * z);
            float3 row2 = float3(2.0f * x * z, 2.0f * y * z, 2.0f * z * z - 1.0f);
            float4x4 ret = float4x4(row0, row1, row2);

            return ret;
        }
    }

    float3 k = v * (1.0f / s);
    float3 row0 = float3(0.0f, -k.z, k.y);
    float3 row1 = float3(k.z, 0.0f, -k.x);
    float3 row2 = float3(-k.y, k.x, 0.0f);
    float4x4 K(row0, row1, row2);

    float4x4 I;

    // Rodrigues rotation formula
    float one_minus_c = 1 - c;

    // Compute K^2
    float4x4 K2;
    for(int i = 0; i < 3; ++i)
    {
        for(int j = 0; j < 3; ++j)
        {
            uint32_t iIndex = i * 4 + j;
            K2.mafEntries[iIndex] = 0.0f;

            for(int k_ = 0; k_ < 3; ++k_)
            {
                K2.mafEntries[iIndex] += K.mafEntries[i * 4 + k_] * K.mafEntries[k_ * 4 + j];
            }
        }
    }
    // R = I + s*K + (1 - c)*K^2
    float4x4 R;
    for(int i = 0; i < 3; ++i)
    {
        for(int j = 0; j < 3; ++j)
        {
            uint32_t iIndex = i * 4 + j;
            R.mafEntries[iIndex] = I.mafEntries[iIndex] + s * K.mafEntries[iIndex] + one_minus_c * K2.mafEntries[iIndex];
        }
    }

    return R;
}

/*
**
*/
vec4 extractAxisAngle(mat4 const& m)
{
    // Compute the trace
    float fTrace = m.mafEntries[0] + m.mafEntries[5] + m.mafEntries[10];

    // Compute angle (in radians)
    float fAngle = acosf(clamp((fTrace - 1.0f) / 2.0f, -1.0f, 1.0f));

    float3 axis = float3(0.0f, 0.0f, 0.0f);

    if(fabs(fAngle) < 1.0e-6f)
    {
        // Angle is ~0: identity rotation — axis arbitrary
        axis = float3(1.0f, 0.0f, 0.0f);
    }
    else if(fabs(fAngle - 3.14159f) <= 1.0e-3f)
    {
        for(uint32_t i = 0; i < 3; ++i)
        {
            if(m.mafEntries[i * 5] > m.mafEntries[((i + 1) % 3) * 5] && m.mafEntries[i * 5] > m.mafEntries[((i + 2) % 3) * 5])
            {
                float denom = sqrtf(1.0f + m.mafEntries[i * 5] - m.mafEntries[((i + 1) % 3) * 5] - m.mafEntries[((i + 2) % 3) * 5]);
                if(i == 0)
                {
                    axis.x = denom / 2.0f;
                    axis.y = (m.mafEntries[1] + m.mafEntries[4]) / (2.0f * denom);
                    axis.z = (m.mafEntries[2] + m.mafEntries[8]) / (2.0f * denom);

                    break;
                }
                else if(i == 1)
                {
                    axis.y = denom / 2.0f;
                    axis.z = (m.mafEntries[6] + m.mafEntries[9]) / (2.0f * denom);
                    axis.x = (m.mafEntries[4] + m.mafEntries[1]) / (2.0f * denom);

                    break;
                }
                else if(i == 2)
                {
                    axis.z = denom / 2.0f;
                    axis.x = (m.mafEntries[8] + m.mafEntries[2]) / (2.0f * denom);
                    axis.y = (m.mafEntries[9] + m.mafEntries[6]) / (2.0f * denom);

                    break;
                }

                axis = normalize(axis);
            }
        }
    }
    else
    {
        float fOneOverDenom = 1.0f / (2.0f * sinf(fAngle));

        // Compute rotation axis
        axis.x = (m.mafEntries[9] - m.mafEntries[6]) * fOneOverDenom;
        axis.y = (m.mafEntries[2] - m.mafEntries[8]) * fOneOverDenom;
        axis.z = (m.mafEntries[4] - m.mafEntries[1]) * fOneOverDenom;
    }

    return float4(axis.x, axis.y, axis.z, fAngle);
}

/*
**
*/
vec4 extractQuaternion(mat4 const& m)
{
    float4 q;
    float fTrace = m.mafEntries[0] + m.mafEntries[5] + m.mafEntries[10];

    if(fTrace > 0)
    {
        float S = sqrtf(fTrace + 1.0f) * 2.0f; // S=4*w
        q.w = 0.25f * S;
        q.x = (m.mafEntries[9] - m.mafEntries[6]) / S;
        q.y = (m.mafEntries[2] - m.mafEntries[8]) / S;
        q.z = (m.mafEntries[4] - m.mafEntries[1]) / S;
    }
    else if(m.mafEntries[0] > m.mafEntries[5] && m.mafEntries[0] > m.mafEntries[10])
    {
        float S = sqrtf(1.0f + m.mafEntries[0] - m.mafEntries[5] - m.mafEntries[10]) * 2.0f; // S=4*x
        q.w = (m.mafEntries[9] - m.mafEntries[6]) / S;
        q.x = 0.25f * S;
        q.y = (m.mafEntries[1] + m.mafEntries[4]) / S;
        q.z = (m.mafEntries[2] + m.mafEntries[8]) / S;
    }
    else if(m.mafEntries[5] > m.mafEntries[10])
    {
        float S = sqrtf(1.0f + m.mafEntries[5] - m.mafEntries[0] - m.mafEntries[10]) * 2.0f; // S=4*y
        q.w = (m.mafEntries[2] - m.mafEntries[8]) / S;
        q.x = (m.mafEntries[1] + m.mafEntries[4]) / S;
        q.y = 0.25f * S;
        q.z = (m.mafEntries[6] + m.mafEntries[9]) / S;
    }
    else
    {
        float S = sqrtf(1.0f + m.mafEntries[10] - m.mafEntries[0] - m.mafEntries[5]) * 2.0f; // S=4*z
        q.w = (m.mafEntries[4] - m.mafEntries[1]) / S;
        q.x = (m.mafEntries[2] + m.mafEntries[8]) / S;
        q.y = (m.mafEntries[6] + m.mafEntries[9]) / S;
        q.z = 0.25f * S;
    }

    return q;
}

/*
**
*/
vec3 extractScale(mat4 const& m)
{
    float fScaleX = length(float3(m.mafEntries[0], m.mafEntries[4], m.mafEntries[8]));
    float fScaleY = length(float3(m.mafEntries[1], m.mafEntries[5], m.mafEntries[9]));
    float fScaleZ = length(float3(m.mafEntries[2], m.mafEntries[6], m.mafEntries[10]));

    return float3(fScaleX, fScaleY, fScaleZ);
}
