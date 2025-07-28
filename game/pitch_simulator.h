#pragma once

#include <math/vec.h>

namespace Simulator
{
    class CPitchSimulator
    {
    public:
        struct Descriptor
        {
            // Baseball parameters (SI units)
            float mfMass = 0.145f;                              // kg
            float mfRadius = 0.0366f;                           // m
            float mfAirDensity = 1.225f;                        // kg/m^3
            float mfGravity = 9.81f;                            // m/s^2 gravity

            // Pitch parameters
            float mfInitialSpeed = 44.7f; // 44.7f;                       // m/s (100 mph)
            float mfSpinRPM = 2400.0f;                          // rpm

            // Drag and lift coefficients
            float mfDragCoeff = 0.35f;                          // drag coefficient
            float mfSeamShiftedWakeCoeff = 0.05f;               // seam-shifted wake coefficient (small lateral force)
        
            float3 mInitialPosition = float3(0.0f, 2.2f, 0.0f);

            float3 mSpinAxis = float3(1.0f, 0.0f, 0.0f);
            float3 mInitialVelocity = float3(0.0f, -2.0f, 44.7f);
        };

    public:
        CPitchSimulator() = default;
        virtual ~CPitchSimulator() = default;

        void simulate(float fDTimeSeconds);

        inline float3 getPosition() { return mPosition; }
        inline float getTime() { return mfTime; }
        inline float4 getAxisAngle() { return mAxisAngle; }

        inline void setDesc(Descriptor const& desc) { mDesc = desc; }

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
    
    };
}   // Simulator