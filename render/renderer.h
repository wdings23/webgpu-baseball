#pragma once

#include <render/render_job.h>
#include <webgpu/webgpu_cpp.h>
#include <string>
#include <map>

#include <math/mat4.h>

namespace Render
{
    class CRenderer
    {
    public:
        struct CreateDescriptor
        {
            wgpu::Instance* mpInstance;
            wgpu::Device* mpDevice;
            uint32_t miScreenWidth;
            uint32_t miScreenHeight;
            std::string mMeshFilePath;
            std::string mRenderJobPipelineFilePath;
            wgpu::Sampler* mpSampler;

        };

        struct DrawUpdateDescriptor
        {
            float4x4 const* mpViewMatrix;
            float4x4 const* mpProjectionMatrix;
            float4x4 const* mpViewProjectionMatrix;
            float4x4 const* mpPrevViewProjectionMatrix;

            float3 const* mpCameraPosition;
            float3 const* mpCameraLookAt;
        };

        struct SelectMeshInfo
        {
            int32_t miMeshID = -1;
            int32_t miSelectionCoordX;
            int32_t miSelectionCoordY;
            int32_t miPadding;
            float4 mMinPosition;
            float4 mMaxPosition;
        };

        struct TextureAtlasInfo
        {
            uint2               miTextureCoord;
            float2              mUV;
            uint32_t            miTextureID;
            uint32_t            miImageWidth;
            uint32_t            miImageHeight;
            uint32_t            miPadding0;
        };

    public:
        CRenderer() = default;
        virtual ~CRenderer() = default;

        void setup(CreateDescriptor& desc);
        void draw(DrawUpdateDescriptor& desc);

        wgpu::Texture& getSwapChainTexture();

        bool setBufferData(
            std::string const& jobName,
            std::string const& bufferName,
            void* pData,
            uint32_t iOffset,
            uint32_t iDataSize);

        bool setBufferData(
            std::string const& bufferName,
            void* pData,
            uint32_t iOffset,
            uint32_t iDataSize);

        void highLightSelectedMesh(int32_t iX, int32_t iY);

        void loadTexturesIntoAtlas(
            std::string const& meshFilePath, 
            std::string const& dir);

        inline void setExplosionMultiplier(float fMult)
        {
            mfExplosionMult = fMult;
            mbUpdateUniform = true;
        }

        SelectMeshInfo const& getSelectionInfo();

        inline uint32_t getNumMeshes()
        {
            return (uint32_t)maMeshTriangleRanges.size();
        }

        inline void setVisibilityFlags(uint32_t* piVisibilityFlags)
        {
            maiVisibilityFlags = piVisibilityFlags;
        }

        inline uint32_t getFrameIndex()
        {
            return miFrame;
        }

        inline void setCameraPositionAndLookAt(
            float3 const& cameraPosition,
            float3 const& cameraLookAt
        )
        {
            mCameraPosition = cameraPosition;
            mCameraLookAt = cameraLookAt;
        }

        inline void registerBuffer(std::string const& name, wgpu::Buffer& buffer)
        {
            maBuffers[name] = buffer;
        }

        inline void registerTexture(std::string const& name, wgpu::Texture& texture)
        {
            maTextures[name] = texture;
        }

        inline wgpu::Buffer& getBuffer(std::string const& name)
        {
            return maBuffers[name];
        }

        inline void setGetVertexBufferNames(void(*pfn)(std::vector<std::string>&))
        {
            mpfnGetVertexBufferNames = pfn;
        }

        inline void setGetIndexBufferNames(void(*pfn)(std::vector<std::string>&))
        {
            mpfnGetIndexBufferNames = pfn;
        }

        inline void setGetIndexCounts(void(*pfn)(std::vector<uint32_t>&))
        {
            mpfnIndexCounts = pfn;
        }

        inline void setGetIndexRanges(void(*pfn)(std::vector<std::pair<uint32_t, uint32_t>>&))
        {
            mpfnGetIndexRanges = pfn;
        }

        inline void setGetVertexRanges(void(*pfn)(std::vector<std::pair<uint32_t, uint32_t>>&))
        {
            mpfnGetVertexRanges = pfn;
        }

        inline void setGetAnimVertexBufferNames(void(*pfn)(std::vector<std::string>&))
        {
            mpfnGetAnimVertexBufferNames = pfn;
        }

        inline void setGetAnimIndexBufferNames(void(*pfn)(std::vector<std::string>&))
        {
            mpfnGetAnimIndexBufferNames = pfn;
        }

        inline void setGetAnimIndexRanges(void(*pfn)(std::vector<std::pair<uint32_t, uint32_t>>&))
        {
            mpfnGetAnimIndexRanges = pfn;
        }

        inline void setGetAnimVertexRanges(void(*pfn)(std::vector<std::pair<uint32_t, uint32_t>>&))
        {
            mpfnGetAnimVertexRanges = pfn;
        }

        inline std::vector<TextureAtlasInfo>& getTextureAtlasInfo() { return maTextureAtlasInfo; }

    public:
        struct MeshExtent
        {
            float4  mMinPosition;
            float4  mMaxPosition;
        };

        MeshExtent                              mTotalMeshExtent;

    protected:
        void createRenderJobs(CreateDescriptor& desc);
        void createTextureAtlas();

    protected:
        
        CreateDescriptor                        mCreateDesc;

        wgpu::Device* mpDevice;

        // TODO: move buffers output renderer
        std::map<std::string, wgpu::Buffer>     maBuffers;
        std::map<std::string, uint32_t>         maBufferSizes;
        std::map<std::string, std::unique_ptr<Render::CRenderJob>>   maRenderJobs;
        std::vector<std::string> maOrderedRenderJobs;

        uint32_t                                miFrame = 0;

        struct MeshTriangleRange
        {
            uint32_t miStart;
            uint32_t miEnd;
        };
        

        std::vector<MeshTriangleRange>          maMeshTriangleRanges;
        std::vector<MeshExtent>                 maMeshExtents;

        wgpu::Instance*                         mpInstance;

        wgpu::Sampler*                          mpSampler;

        std::vector<TextureAtlasInfo>           maTextureAtlasInfo;
        int32_t                                 miAtlasImageWidth;
        int32_t                                 miAtlasImageHeight;

    protected:
        std::string                             mCaptureImageName = "";
        std::string                             mCaptureImageJobName = "";
        std::string                             mCaptureUniformBufferName = "";
        int2                                    mSelectedCoord = int2(-1, -1);
        wgpu::Buffer                            mOutputImageBuffer;
        
        wgpu::Texture                           mDiffuseTextureAtlas;
        wgpu::TextureView                       mDiffuseTextureAtlasView;

        std::map<std::string, wgpu::Texture>              maTextures;
        std::map<std::string, wgpu::TextureView>          maTextureViews;

        SelectMeshInfo                          mSelectMeshInfo;
        
        float                                   mfExplosionMult = 1.0f;
        bool                                    mbWaitingForMeshSelection = false;
        bool                                    mbUpdateUniform = false;
    
        uint32_t                                miStartCaptureFrame = 0;
        bool                                    mbSelectedBufferCopied = false;


        uint32_t*                               maiVisibilityFlags = nullptr;

        float3                                  mCameraPosition;
        float3                                  mCameraLookAt;

        void (*mpfnGetVertexBufferNames)(std::vector<std::string>&) = nullptr;
        void (*mpfnGetIndexBufferNames)(std::vector<std::string>&) = nullptr;
        void (*mpfnIndexCounts)(std::vector<uint32_t>&) = nullptr;
        void (*mpfnGetIndexRanges)(std::vector<std::pair<uint32_t, uint32_t>>&) = nullptr;
        void (*mpfnGetVertexRanges)(std::vector<std::pair<uint32_t, uint32_t>>&) = nullptr;

        void (*mpfnGetAnimVertexBufferNames)(std::vector<std::string>&) = nullptr;
        void (*mpfnGetAnimIndexBufferNames)(std::vector<std::string>&) = nullptr;
        void (*mpfnGetAnimIndexRanges)(std::vector<std::pair<uint32_t, uint32_t>>&) = nullptr;
        void (*mpfnGetAnimVertexRanges)(std::vector<std::pair<uint32_t, uint32_t>>&) = nullptr;
    };

}   // Render