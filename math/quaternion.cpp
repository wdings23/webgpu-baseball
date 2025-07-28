#include <math/quaternion.h>

#if defined(_MSC_VER)
#include <corecrt_math_defines.h>
#endif // _MSC_VER

/*
**
*/
mat4 quaternion::matrix()
{
    float fXSquared = x * x;
    float fYSquared = y * y;
    float fZSquared = z * z;
    
    float fXMulY = x * y;
    float fXMulZ = x * z;
    float fXMulW = x * w;
    
    float fYMulZ = y * z;
    float fYMulW = y * w;
    
    float fZMulW = z * w;
    
    
    float afVal[16];
    afVal[0] = 1.0f - 2.0f * fYSquared - 2.0f * fZSquared;
    afVal[1] = 2.0f * fXMulY - 2.0f * fZMulW;
    afVal[2] = 2.0f * fXMulZ + 2.0f * fYMulW;
    afVal[3] = 0.0f;
    
    afVal[4] = 2.0f * fXMulY + 2.0f * fZMulW;
    afVal[5] = 1.0f - 2.0f * fXSquared - 2.0f * fZSquared;
    afVal[6] = 2.0f * fYMulZ - 2.0f * fXMulW;
    afVal[7] = 0.0f;
    
    afVal[8] = 2.0f * fXMulZ - 2.0f * fYMulW;
    afVal[9] = 2.0f * fYMulZ + 2.0f * fXMulW;
    afVal[10] = 1.0f - 2.0f * fXSquared - 2.0f * fYSquared;
    afVal[11] = 0.0f;
    
    afVal[12] = afVal[13] = afVal[14] = 0.0f;
    afVal[15] = 1.0f;
    
    return mat4(afVal);
}

/*
**
*/
quaternion quaternion::fromMatrix(mat4 const& mat)
{
	quaternion ret;

	float trace = mat.mafEntries[0] + mat.mafEntries[5] + mat.mafEntries[10]; // I removed + 1.0f; see discussion with Ethan
	if(trace > 0) 
	{// I changed M_EPSILON to 0
		float s = 0.5f / sqrtf(trace + 1.0f);
		ret.w = 0.25f / s;
		ret.x = (mat.mafEntries[9] - mat.mafEntries[6]) * s;
		ret.y = (mat.mafEntries[2] - mat.mafEntries[8]) * s;
		ret.z = (mat.mafEntries[4] - mat.mafEntries[1]) * s;
	}
	else 
	{
		if(mat.mafEntries[0] > mat.mafEntries[5] && mat.mafEntries[0] > mat.mafEntries[10]) 
		{
			float s = 2.0f * sqrtf(1.0f + mat.mafEntries[0] - mat.mafEntries[5] - mat.mafEntries[10]);
			ret.w = (mat.mafEntries[9] - mat.mafEntries[6]) / s;
			ret.x = 0.25f * s;
			ret.y = (mat.mafEntries[1] + mat.mafEntries[4]) / s;
			ret.z = (mat.mafEntries[2] + mat.mafEntries[8]) / s;
		}
		else if(mat.mafEntries[5] > mat.mafEntries[10]) 
		{
			float s = 2.0f * sqrtf(1.0f + mat.mafEntries[5] - mat.mafEntries[0] - mat.mafEntries[10]);
			ret.w = (mat.mafEntries[2] - mat.mafEntries[8]) / s;
			ret.x = (mat.mafEntries[1] + mat.mafEntries[4]) / s;
			ret.y = 0.25f * s;
			ret.z = (mat.mafEntries[6] + mat.mafEntries[9]) / s;
		}
		else 
		{
			float s = 2.0f * sqrtf(1.0f + mat.mafEntries[10] - mat.mafEntries[0] - mat.mafEntries[5]);
			ret.w = (mat.mafEntries[4] - mat.mafEntries[1]) / s;
			ret.x = (mat.mafEntries[2] + mat.mafEntries[8]) / s;
			ret.y = (mat.mafEntries[6] + mat.mafEntries[9]) / s;
			ret.z = 0.25f * s;
		}
	}

	return ret;
}

/*
**
*/
vec3 quaternion::toEuler()
{
	vec3 ret;

	float sqw = w * w;
	float sqx = x * x;
	float sqy = y * y;
	float sqz = z * z;

	float unit = sqx + sqy + sqz + sqw; // if normalised is one, otherwise is correction factor
	float test = x * y + z * w;
	if(test > 0.499f * unit) { // singularity at north pole
		ret.y = 2.0f * atan2f(x, w);
		ret.x = (float)M_PI * 0.5f;
		ret.z = 0;
		return ret;
	}
	if(test < -0.499f * unit) { // singularity at south pole
		ret.y = -2.0f * atan2f(x, w);
		ret.x = (float)-M_PI * 0.5f;
		ret.z = 0;
		return ret;
	}
	ret.y = atan2f(2.0f * y * w - 2 * x * z, sqx - sqy - sqz + sqw);
	ret.x = asinf(2.0f * test / unit);
	ret.z = atan2f(2.0f * x * w - 2.0f * y * z, -sqx + sqy - sqz + sqw);

	return ret;
}

/*
**
*/
float quaternion::dot(quaternion const& q0, quaternion const& q1)
{
    return (q1.x * q0.x + q1.y * q0.y + q1.z * q0.z + q1.w * q0.w);
}

/*
**
*/
quaternion quaternion::normalize(quaternion const& q)
{
    float fLength = sqrtf(quaternion::dot(q, q));
    return quaternion(
        q.x / fLength,
        q.y / fLength,
        q.z / fLength,
        q.w / fLength
    );
}

/*
**
*/
quaternion quaternion::slerp(quaternion const& q0, quaternion const& q1, float t)
{
    // Compute the cosine of the angle between the two vectors.
    float dotProd = dot(q0, q1);

    quaternion q1Copy = q1;

    // If the dot product is negative, slerp won't take the shorter path.
    // Fix by reversing one quaternion.
    if(dotProd < 0.0f) {
        dotProd = -dotProd;
        q1Copy.w = -q1Copy.w;
        q1Copy.x = -q1Copy.x;
        q1Copy.y = -q1Copy.y;
        q1Copy.z = -q1Copy.z;
    }

    const float DOT_THRESHOLD = 0.9995f;
    if(dotProd > DOT_THRESHOLD) {
        // Very close - use linear interpolation
        quaternion result;
        result.w = q0.w + t * (q1Copy.w - q0.w);
        result.x = q0.x + t * (q1Copy.x - q0.x);
        result.y = q0.y + t * (q1Copy.y - q0.y);
        result.z = q0.z + t * (q1Copy.z - q0.z);
        normalize(result);
        return result;
    }

    // Since dot is in range [0, DOT_THRESHOLD], acos is safe
    float theta_0 = acosf(dotProd);        // angle between input quaternions
    float theta = theta_0 * t;                 // angle between q0 and result
    float sin_theta = sinf(theta);
    float sin_theta_0 = sinf(theta_0);

    float s0 = cosf(theta) - dotProd * sin_theta / sin_theta_0;
    float s1 = sin_theta / sin_theta_0;

    quaternion result;
    result.w = (s0 * q0.w) + (s1 * q1Copy.w);
    result.x = (s0 * q0.x) + (s1 * q1Copy.x);
    result.y = (s0 * q0.y) + (s1 * q1Copy.y);
    result.z = (s0 * q0.z) + (s1 * q1Copy.z);
    return result;
}