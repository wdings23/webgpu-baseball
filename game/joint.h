#pragma once

#include <math/quaternion.h>

struct Joint
{
    uint32_t miIndex;
    uint32_t miNumChildren;
    uint32_t maiChildren[32];
    uint32_t miParent;

    quaternion  mRotation;
    float3      mTranslation;
    float3      mScaling;

    bool operator == (Joint const& j)
    {
        bool bEqual = (
            miIndex == j.miIndex &&
            miNumChildren == j.miNumChildren &&
            maiChildren[0] == j.maiChildren[0] &&
            miParent == j.miParent &&
            mRotation == j.mRotation &&
            mTranslation == j.mTranslation &&
            mScaling == j.mScaling
         );
        
        return bEqual;
    }
};