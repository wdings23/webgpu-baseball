#include <game/pitch_simulator.h>
#include <math.h>
#include <stdlib.h>

#include <utils/LogPrint.h>

namespace Simulator
{
    /*
    **
    */
    void CPitchSimulator::simulate(
        float fDTimeSeconds)
    {
        // Function to generate fluctuating Cl_ssw at a given time
        auto getFluctuatingClSSW = [this](float time)
        {
            // Base coefficient oscillates like a sine wave (simulating vortex shedding)
            float base = 0.5f * sinf(20.0f * time); // 20 Hz oscillation

            // Add small random noise between -0.01 and 0.01
            float noise = ((rand() % 2001) - 1000) / 100000.0f; // [-0.01, 0.01]

            // less spin => higher fluctuation
            float fFactor = (100.0f / mDesc.mfSpinRPM);
            noise *= fFactor;
            base *= fFactor;

            return base + noise;
        };


        float3 spinAxisNormalized = normalize(mDesc.mSpinAxis);
        if(mfTime <= 0.0f)
        {
            mPosition = mDesc.mInitialPosition;
            mVelocity = mDesc.mInitialVelocity;
            mAxisAngle = float4(1.0f, 0.0f, 0.0f, 0.0f);

            mfBallArea = 3.14159f * mDesc.mfRadius * mDesc.mfRadius; // m^2

            mfSpinRadiansPerSecond = (mDesc.mfSpinRPM * 2.0f * 3.14159f) / 60.0f;  // rad/s
            mfSpinFactor = mDesc.mfRadius * mfSpinRadiansPerSecond / mDesc.mfInitialSpeed; // S
            mfLiftCoeff = 1.6f * mfSpinFactor; // lift coefficient ~ 0.328
        }

        float fVelocityLength = length(mVelocity);
        if(fVelocityLength < 0.1f)
        {
            return;
        }

        float fVelocityLengthSquared = fVelocityLength * fVelocityLength;

        // Unit velocity vector
        float3 velocityNormalized = mVelocity / fVelocityLength;

        // Drag force magnitude
        float fDrag = 0.5f * mDesc.mfAirDensity * mDesc.mfDragCoeff * mfBallArea * fVelocityLengthSquared;

        // Drag acceleration vector (opposite velocity)
        float3 dragAcceleration = velocityNormalized * (-fDrag / mDesc.mfMass);

        // Magnus force magnitude (lift)
        float fMagnusForce = 0.5f * mDesc.mfAirDensity * mfLiftCoeff * mfBallArea * fVelocityLengthSquared;
        float fMagnusAcceleration = fMagnusForce / mDesc.mfMass;

        // Compute Magnus acceleration vector = (omega cross v) normalized * a_M
        // omega = spin vector (spin_x, spin_y, spin_z)
        // v = velocity vector (vx, vy, vz)
        
        // Cross product omega x v
        float3 magnusAcceleration = normalize(cross(spinAxisNormalized, mVelocity)) * fMagnusAcceleration;

        // Seam-Shifted Wake fluctuations for knuckle balls
        float fSeamShiftedWake = mDesc.mfSeamShiftedWakeCoeff;
        if(mDesc.mfSpinRPM <= 100.0f)
        {
            fSeamShiftedWake = getFluctuatingClSSW(mfTime);
        }

        // Seam-Shifted Wake force magnitude (lateral)
        float fSeamShiftedForce = 0.5f * mDesc.mfAirDensity * fSeamShiftedWake * mfBallArea * fVelocityLengthSquared;
        float fSeamShiftedAcceleration = fSeamShiftedForce / mDesc.mfMass;

        // Assume SSW acts along x (arm-side run)
        float3 seamShiftedWeight = float3(fSeamShiftedAcceleration, 0.0f, 0.0f);

        // Gravity acceleration
        float3 gravityAcceleration = float3(0.0f, -mDesc.mfGravity, 0.0f);
        
        // Total accelerations
        float3 totalAcceleration = dragAcceleration + magnusAcceleration + seamShiftedWeight + gravityAcceleration;

        // Update velocities
        mVelocity = mVelocity + totalAcceleration * fDTimeSeconds;

        // Update positions
        mPosition = mPosition + mVelocity * fDTimeSeconds;
        
        mAxisAngle.w += mfSpinRadiansPerSecond * fDTimeSeconds;
        mAxisAngle = float4(spinAxisNormalized.x, spinAxisNormalized.y, spinAxisNormalized.z, mAxisAngle.w);

        mfTime += fDTimeSeconds;
    }

    /*
    **
    */
    void CPitchSimulator::reset()
    {
        mfTime = 0.0f;
    }

}   // Simulator
