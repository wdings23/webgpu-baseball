#include <game/batted_ball_simulator.h>
#include <math.h>
#include <stdlib.h>

#include <utils/LogPrint.h>

#define PI 3.14159f

namespace Simulator
{
    /*
    **
    */
    void CBattedBallSimulator::simulate(
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


        if(miNumBounces <= 0)
        {
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
            float3 dragAcceleration = velocityNormalized * (-fDrag / mDesc.mfBallMass);

            // Magnus force magnitude (lift)
            float fMagnusForce = 0.5f * mDesc.mfAirDensity * mfLiftCoeff * mfBallArea * fVelocityLengthSquared;
            float fMagnusAcceleration = fMagnusForce / mDesc.mfBallMass;

            // Compute Magnus acceleration vector = (omega cross v) normalized * a_M
            // omega = spin vector (spin_x, spin_y, spin_z)
            // v = velocity vector (vx, vy, vz)

            // Cross product omega x v
            float3 magnusAcceleration = normalize(cross(spinAxisNormalized, mVelocity)) * fMagnusAcceleration;

            // Seam-Shifted Wake fluctuations for knuckle balls
            float fSeamShiftedWake = mDesc.mfSeamShiftedWakeCoeff;
            //if(mDesc.mfSpinRPM <= 100.0f)
            //{
            //    fSeamShiftedWake = getFluctuatingClSSW(mfTime);
            //}

            // Seam-Shifted Wake force magnitude (lateral)
            float fSeamShiftedForce = 0.5f * mDesc.mfAirDensity * fSeamShiftedWake * mfBallArea * fVelocityLengthSquared;
            float fSeamShiftedAcceleration = fSeamShiftedForce / mDesc.mfBallMass;

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

            if(mPosition.y <= mDesc.mfRadius)
            {
                mCurrSpin = float3(0.0f, 20.0f, 0.0f);
                ++miNumBounces;
            }
        }
        else
        {
            // Apply gravity
            mVelocity.y -= mDesc.mfGravity * fDTimeSeconds;
            
            // Update position
            mPosition += mVelocity * fDTimeSeconds;

            float fInertia = 0.4f * mDesc.mfBallMass * mDesc.mfRadius * mDesc.mfRadius;

            // Collision with ground
            if(mPosition.y <= mDesc.mfRadius) 
            {
                mPosition.y = mDesc.mfRadius;

                // Bounce vertically
                mVelocity.y = -mVelocity.y * mDesc.mRestitutionCoeff;

                // Tangential velocity at point of contact (bottom of ball)
                float3 contactVelocity = mVelocity + cross(mDesc.mSpinAxis, float3(0.0f, -mDesc.mfRadius, 0.0f));  // velocity at contact point
                mTangentialVelocity = float3(contactVelocity.x, 0.0f, contactVelocity.z);  // horizontal component

                if(length(mTangentialVelocity) > 1e-4)
                {
                    float3 frictionDirection = normalize(mTangentialVelocity) * -1.0f;
                    float fFrictionForce = mDesc.mFrictionCoeff * mDesc.mfBallMass * mDesc.mfGravity;
                    float3 impulse = frictionDirection * (fFrictionForce * fDTimeSeconds);

                    // Linear velocity update
                    mVelocity += impulse / mDesc.mfBallMass;

                    // Angular velocity update (torque = r × F)
                    float3 torque = cross(float3(0.0f, -mDesc.mfRadius, 0.0f), impulse);
                    float3 angularAcceleration = torque / fInertia;
                    mCurrSpin += angularAcceleration;

                    //DEBUG_PRINTF("velocity (%.4f, %.4f, %.4f) tangential velocity (%.4f, %.4f, %.4f) torque (%.4f, %.4f, %.4f)\n",
                    //    mVelocity.x, mVelocity.y, mVelocity.z,
                    //    mTangentialVelocity.x, mTangentialVelocity.y, mTangentialVelocity.z,
                    //    torque.x, torque.y, torque.z
                    //);

                    mfSpinRadiansPerSecond = length(mCurrSpin);
                    mAxisAngle = float4(torque.x, torque.y, torque.z, mfSpinRadiansPerSecond * fDTimeSeconds);

                    miNumBounces += 1;
                }
            }
        
            //DEBUG_PRINTF("tangential velocity (%.4f, %.4f, %.4f) spin (%.4f, %.4f, %.4f) bounces %d\n", 
            //    mTangentialVelocity.x, mTangentialVelocity.y, mTangentialVelocity.z,
            //    mCurrSpin.x, mCurrSpin.y, mCurrSpin.z,
            //    miNumBounces);
        }

        mfTime += fDTimeSeconds;
    }

    /*
    **
    */
    bool CBattedBallSimulator::hasStopped()
    {
        // Thresholds
        const float v_thresh = 0.1f;  // m/s

        return fabs(mPosition.y - mDesc.mfRadius) < 1e-3f &&
            fabs(mVelocity.y) < v_thresh &&
            length(mTangentialVelocity) < v_thresh;
    }

    /*
    **
    */
    void CBattedBallSimulator::reset()
    {
        mfTime = 0.0f;
        miNumBounces = 0;
    }



    /*
    **
    */
    void CBattedBallSimulator::computeExitParams(
        float& fExitSpeed,
        float& fLaunchAngle,
        float3& exitSpinVector,
        float3& exitBallVelocity,

        float fBatSpeed,
        float fBatAttackAngleDegree,
        float fBatHorizontalAngleDegree,
        float fVerticalOffset,
        float fHorizontalOffset)
    {
        float fAttackRadians = fBatAttackAngleDegree * PI / 180.0f;
        float fHorizontalRadians = fBatHorizontalAngleDegree * PI / 180.0f;

        float3 batDirection = float3(
            cosf(fAttackRadians) * sinf(fHorizontalRadians),
            sinf(fAttackRadians),
            cosf(fAttackRadians) * cosf(fHorizontalRadians)
        );
        float3 batVelocity = batDirection * fBatSpeed;

        float3 contactOffset = {fHorizontalOffset, fVerticalOffset, 0.0};

        float fReducedMass = (mDesc.mfBatMass * mDesc.mfBallMass) / (mDesc.mfBatMass + mDesc.mfBallMass);
        fExitSpeed = (1 + mDesc.mRestitutionCoeff) * fReducedMass / mDesc.mfBallMass * fBatSpeed;
        exitBallVelocity = normalize(batVelocity) * fExitSpeed;

        // Spin generation from offset
        exitSpinVector = float3(
            -mDesc.mFrictionCoeff * contactOffset.y * fBatSpeed / mDesc.mfRadius,  // sidespin
            mDesc.mFrictionCoeff * contactOffset.x * fBatSpeed / mDesc.mfRadius,   // backspin
            0.0f
        );

        if(exitSpinVector.x == 0.0f && exitSpinVector.y == 0.0f)
        {
            exitSpinVector.y = 1.0f;
        }

        float fSpinScale = 1000.0f;     // rad/second to RPM
        exitSpinVector = exitSpinVector * (fSpinScale / (2.0f * PI));  // to RPM

        fLaunchAngle = atan2f(exitBallVelocity.y, sqrtf(exitBallVelocity.x * exitBallVelocity.x + exitBallVelocity.z * exitBallVelocity.z)) * 180.0f / PI;
    }

}   // Simulator
