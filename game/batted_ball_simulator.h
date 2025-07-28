#pragma once

#include <math/vec.h>

namespace Simulator
{
    class CBattedBallSimulator
    {
    public:
        struct Descriptor
        {
            // Baseball parameters (SI units)
            float mfBallMass = 0.145f;                              // kg
            float mfBatMass = 0.94f;                            // kg

            float mfRadius = 0.0366f;                           // m
            float mfAirDensity = 1.225f;                        // kg/m^3
            float mfGravity = 9.81f;                            // m/s^2 gravity

            // Pitch parameters
            float mfInitialSpeed = 44.7f; // 44.7f;                       // m/s (100 mph)
            float mfSpinRPM = 2400.0f;                          // rpm

            // Drag and lift coefficients
            float mfDragCoeff = 0.35f;                          // drag coefficient
            float mfSeamShiftedWakeCoeff = 0.05f;               // seam-shifted wake coefficient (small lateral force)

            float mLiftCoeff = 0.2f;                 // Lift coefficient for Magnus
            float mRestitutionCoeff = 0.3f;                  // Coefficient of restitution
            float mFrictionCoeff = 0.3f;     // Tangential friction

            float3 mInitialPosition = float3(0.0f, 2.2f, 0.0f);

            float3 mSpinAxis = float3(1.0f, 0.0f, 0.0f);
            float3 mInitialVelocity = float3(0.0f, -2.0f, 44.7f);
        };
    public:
        CBattedBallSimulator() = default;
        virtual ~CBattedBallSimulator() = default;

        void simulate(float fDTimeSeconds);
        void computeExitParams(
            float& fExitSpeed,
            float& fLaunchAngle,
            float3& exitSpinVector,
            float3& exitBallVelocity,

            float fBatSpeed,
            float fBatAttackAngleDegree,
            float fBatHorizontalAngleDegree,
            float fVerticalOffset,
            float fHorizontalOffset);

        inline float3 getPosition() { return mPosition; }
        inline float getTime() { return mfTime; }
        inline float4 getAxisAngle() { return mAxisAngle; }

        inline void setDesc(Descriptor const& desc) { mDesc = desc; }
        inline uint32_t getNumBounces() { return miNumBounces; }
        inline float3 getVelocity() { return mVelocity; }

        bool hasStopped();

        void reset();
        
    protected:
        Descriptor              mDesc;

        float3                  mPosition;
        float3                  mVelocity;
        float4                  mAxisAngle;

        float                   mfTime;
        float                   mfBallArea;

        float                   mfLiftCoeff;
        float                   mfSpinFactor;
        float                   mfSpinRadiansPerSecond;

        uint32_t                miNumBounces;
        float3                  mCurrSpin;
        float                   mfCurrSpeed;
        float3                  mTangentialVelocity;
    };
}