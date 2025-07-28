#pragma once

#include <string>
#include <vector>

#include <render/renderer.h>
#include <game/joint.h>
#include <game/pitch_simulator.h>
#include <game/batted_ball_simulator.h>
#include <render/camera.h>

#include <chrono>
#include <vector>

enum GameState
{
    GAME_STATE_PITCH_WINDUP,
    GAME_STATE_PITCH_BALL_IN_FLIGHT,
    GAME_STATE_HIT_BALL_IN_FLIGHT,
};

class CApp
{
public:
    struct CreateInfo
    {
        wgpu::Device* mpDevice;
        Render::CRenderer* mpRenderer;
    };

    struct AnimFrameInfo
    {
        uint32_t        miJoint;
        float4x4        mTotalAnimMatrix;
        float4x4        mTotalAnimWithInverseBindMatrix;
    };

    struct AnimFrame
    {
        float           mfTime;
        uint32_t        miNodeIndex;
        float4          mRotation = float4(0.0f, 0.0f, 0.0f, 1.0f);
        float4          mTranslation = float4(0.0f, 0.0f, 0.0f, 1.0f);
        float4          mScaling = float4(1.0f, 1.0f, 1.0f, 1.0f);

        bool operator == (CApp::AnimFrame& check)
        {
            bool bRet = (
                check.mfTime == mfTime &&
                check.miNodeIndex == miNodeIndex &&
                check.mRotation == mRotation &&
                check.mTranslation == mTranslation &&
                check.mScaling == mScaling 
            );

            return bRet;
        }
    };


public:
    CApp() = default;
    virtual ~CApp() = default;

    void init(CreateInfo& createInfo);

    void loadMeshes(
        std::string const& dir);

    void loadAnimMeshes(
        std::string const& dir);

    void loadAnimation(
        std::string const& dir);

    void update();

    void verify0(
        std::vector<Joint> const& aJoints,
        std::vector<float4x4> const& aLocalBindMatrices,
        std::vector<float4x4> const& aGlobalInverseBindMatrices,
        std::vector<uint32_t> const& aiJointToArrayMapping,
        std::map<uint32_t, std::string> const& aJointMapping);

    void verify1(
        std::vector<std::vector<CApp::AnimFrame>>& aaAnimFrames,
        std::vector<float4x4> const& aLocalBindMatrices,
        std::vector<std::vector<float4x4>> const& aaDstInverseGlobalBindMatrices);

    
    std::vector<std::string>                            maStaticMeshModelNames;
    std::vector<std::string>                            maAnimMeshModelNames;

protected:
    void updateAnimations(float fTimeSeconds);
    void traverseJoint(
        std::vector<AnimFrameInfo>& aAnimFrames,
        std::vector<float4x4>& aAnimMatrices,
        std::vector<std::vector<AnimFrame>> const& aaAnimFrames,
        std::vector<Joint> const& aJoints,
        std::vector<float4x4> const& aLocalBindMatrices,
        std::vector<float4x4> const& aGlobalInverseBindMatrices,
        std::vector<uint32_t> const& aiJointArrayIndexMapping,
        std::map<uint32_t, std::string>& aJointMapping,
        Joint const& joint,
        float4x4 const& parentMatrix,
        float fTime,
        uint32_t iStack);

    void updateMeshModelTransforms();
    void updateBall(float fCurrTimeMilliSeconds);

    void getJointMatrices(
        float4x4& localBindMatrix,
        float4x4& parentTotalMatrix,
        float4x4& animMatrix,
        std::string const& jointName,
        std::string const& srcAnimName,
        uint32_t iAnimationIndex);

    struct ShadowUniformData
    {
        float4x4            maLightViewProjectionMatrices[3];
        float4x4            mDecalViewProjectionMatrix;
    };

    void updateLightViewMatrices(
        ShadowUniformData& shadowUniformData,
        CCamera const& camera);

    void loadAnimMeshDB();

    static void getVertexBufferNames(std::vector<std::string>& aVertexBufferNames);
    static void getIndexBufferNames(std::vector<std::string>& aIndexBufferNames);
    static void getIndexCounts(std::vector<uint32_t>& aiIndexCounts);
    static void getIndexRanges(std::vector<std::pair<uint32_t, uint32_t>>& aIndexRanges);
    static void getVertexRanges(std::vector<std::pair<uint32_t, uint32_t>>& aVertexRanges);

    static void getAnimVertexBufferNames(std::vector<std::string>& aVertexBufferNames);
    static void getAnimIndexBufferNames(std::vector<std::string>& aIndexBufferNames);
    static void getAnimIndexRanges(std::vector<std::pair<uint32_t, uint32_t>>& aIndexRanges);
    static void getAnimVertexRanges(std::vector<std::pair<uint32_t, uint32_t>>& aVertexRanges);

protected:
   
    struct MeshTriangleRange
    {
        uint32_t miStart;
        uint32_t miEnd;
    };

    struct MeshVertexRange
    {
        uint32_t miStart;
        uint32_t miEnd;
    };

    struct MeshExtent
    {
        float4  mMinPosition;
        float4  mMaxPosition;
    };

    struct ObjectInfo
    {
        uint32_t                            miAnimIndex = UINT32_MAX;
        uint32_t                            miStaticIndex = UINT32_MAX;
    };

    struct AnimMeshGPUBuffers
    {
        wgpu::Buffer    mVertexBuffer;
        wgpu::Buffer    mIndexBuffer;
        wgpu::Buffer    mJointInfluenceIndices;
        wgpu::Buffer    mJointInfluenceWeights;
        wgpu::Buffer    mJointInveseGlobalBindMatrices;
        wgpu::Buffer    mJointGlobalBindMatrices;
        wgpu::Buffer    mJointTotalGlobalAnimationMatrices;
    };

    struct AnimMeshFileInfo
    {
        std::string             mFileName;
        uint32_t                miBaseMeshModel;
        std::vector<std::string>    maMeshInstanceNames;
    };

    struct MatchVertexRangeToMeshInstance
    {
        uint32_t        miStart;
        uint32_t        miEnd;
        uint32_t        miMeshModelIndex;
        uint32_t        miMatrixStartIndex;
        uint32_t        miMatrixEndIndex;
    };

    Simulator::CPitchSimulator              mPitchSimulator;
    Simulator::CBattedBallSimulator         mBattedBallSimulator;
    GameState                               mGameState;
    GameState                               mPrevGameState;

    std::vector<MeshTriangleRange>          maMeshTriangleRanges;
    std::vector<MeshVertexRange>            maMeshVertexRanges;
    std::vector<MeshExtent>                 maMeshExtents;
    MeshExtent                              mTotalMeshExtent;

    std::map<std::string, wgpu::Buffer>     maBuffers;
    std::map<std::string, uint32_t>         maBufferSizes;

    CreateInfo                              mCreateInfo;

    float3                                              mBallPosition;
    float4                                              mBallAxisAngle;

    float3                                              mBatPosition;
    float4                                              mBatAxisAngle;
    float4x4                                            mBatMatrix;

    std::map<std::string, uint32_t>                     maMeshModelInfoDB;
    std::vector<ObjectInfo>                             maMeshModelInfo;
    std::vector<float4x4>                               maAnimMeshModelMatrices;
    std::vector<float4x4>                               maStaticMeshModelMatrices;

    
    std::vector<AnimMeshFileInfo>                       maAnimFileInfo;

    std::map<std::string, AnimMeshGPUBuffers>           maAnimMeshGPUBuffers;

    std::vector<std::vector<Joint>>                     maaJoints;
    std::vector<std::vector<float4x4>>                  maaLocalBindMatrices;
    std::vector<std::vector<float4x4>>                  maaGlobalInverseBindMatrices;
    std::vector<std::vector<uint32_t>>                  maaiJointToArrayMapping;
    std::vector<std::map<uint32_t, std::string>>        maaJointMapping;
    std::vector<std::vector<CApp::AnimFrame>>           maaAnimFrames;
    std::vector<std::vector<float4x4>>                  maaDstLocalBindMatrices;
    std::vector<std::vector<float4x4>>                  maaDstInverseGlobalBindMatrices;

    std::map<std::string, std::vector<AnimFrameInfo>>   maaCurrAnimFrameInfo;
    std::map<std::string, std::vector<float4x4>>        maaCurrLocalAnimMatrices;
    

    std::map<std::string, std::vector<std::vector<CApp::AnimFrame>>>        maaTotalAnimFrames;

    std::chrono::time_point<std::chrono::high_resolution_clock>     mLastTime;
    float                                               mfTimeMilliSeconds;

    std::map<std::string, float>                        mafAnimTimeMilliSeconds;

    float                                               mfStartBallSimulationMilliSeconds;

    std::vector<std::string>                            maVertexBufferNames;
    std::vector<std::string>                            maIndexBufferNames;

    std::vector<std::string>                            maAnimMeshVertexBufferNames;
    std::vector<std::string>                            maAnimMeshIndexBufferNames;
    std::vector<MeshTriangleRange>                      maAnimMeshTriangleRanges;
    std::vector<MeshVertexRange>                        maAnimMeshVertexRanges;

    struct AnimationNameInfo
    {
        std::string                 mSrcAnimationName;
        std::string                 mDatabaseName;
        float                       mfAnimSpeed;
        uint32_t                    miAnimMeshIndex;
        std::string                 mBaseModel;
    };

    std::vector<AnimationNameInfo>                            maAnimationNameInfo;
    std::vector<Render::CRenderer::TextureAtlasInfo>          maAnimMeshTextureAtlasInfo;
    std::vector<uint32_t>                                     maiAnimMeshInstanceMapping;

    CCamera                                                   maLightViewCameras[3];

    wgpu::Texture                                             mBallIndicatorTexture;

    std::vector<float3>                                       maPlayerLocalPositions;

    std::vector<float3>                                       maBallTrailPositions;
    uint32_t                                                  miNumTrailingBalls;

    std::vector<uint32_t>                                     maiVertexBufferStartIndices;
    std::vector<uint32_t>                                     maiIndexBufferStartIndices;
    std::vector<uint32_t>                                     maiJointMatrixStartIndices;

    std::vector<float4x4>                                     maTotalGlobalAnimationMatrices;
    std::vector<std::string>                                  maTotalAnimMeshVertexBufferNames;
    std::vector<std::string>                                  maTotalAnimMeshIndexBufferNames;
    std::vector<MatchVertexRangeToMeshInstance>               maMatchVertexRangeToMeshInstance;

    std::vector<float4x4> maBatGlobalTransforms;
    std::vector<float> mafBatGlobalFrameTimes;
};