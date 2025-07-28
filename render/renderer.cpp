#include <render/renderer.h>

#include <curl/curl.h>

#include <rapidjson/document.h>
#include <math/vec.h>
#include <math/mat4.h>
#include <loader/loader.h>
#include <assert.h>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif // __EMSCRIPTEN__

//#define TINYEXR_IMPLEMENTATION
//#include <tinyexr/tinyexr.h>
//#include <tinyexr/miniz.c>

#define STB_IMAGE_IMPLEMENTATION
#include <external/stb_image/stb_image.h>

#include <utils/LogPrint.h>

struct Vertex
{
    vec4        mPosition;
    vec4        mUV;
    vec4        mNormal;
};

struct DefaultUniformData
{
    int32_t miScreenWidth = 0;
    int32_t miScreenHeight = 0;
    int32_t miFrame = 0;
    uint32_t miNumMeshes = 0;

    float mfRand0 = 0.0f;
    float mfRand1 = 0.0f;
    float mfRand2 = 0.0f;
    float mfRand3 = 0.0f;

    float4x4 mViewProjectionMatrix;
    float4x4 mPrevViewProjectionMatrix;
    float4x4 mViewMatrix;
    float4x4 mProjectionMatrix;

    float4x4 mJitteredViewProjectionMatrix;
    float4x4 mPrevJitteredViewProjectionMatrix;

    float4 mCameraPosition;
    float4 mCameraLookDir;

    float4 mLightRadiance;
    float4 mLightDirection;
};

// Callback function to write data to file
size_t writeData(void* ptr, size_t size, size_t nmemb, void* pData) 
{
    size_t iTotalSize = size * nmemb;
    std::vector<char>* pBuffer = (std::vector<char>*)pData;
    uint32_t iPrevSize = (uint32_t)pBuffer->size();
    pBuffer->resize(pBuffer->size() + iTotalSize);
    char* pBufferEnd = pBuffer->data();
    pBufferEnd += iPrevSize;
    memcpy(pBufferEnd, ptr, iTotalSize);

    return iTotalSize;
}

// Function to download file from URL
void streamBinary(
    const std::string& url,
    std::vector<char>& acTriangleBuffer) 
{
    CURL* curl;
    CURLcode res;


    curl = curl_easy_init();
    if(curl)
    {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeData);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &acTriangleBuffer);
        res = curl_easy_perform(curl);

        curl_easy_cleanup(curl);
    }
}

namespace Render
{
    /*
    **
    */
    void CRenderer::setup(CreateDescriptor& desc)
    {
        mCreateDesc = desc;

        mpDevice = desc.mpDevice;
        wgpu::Device& device = *mpDevice;

        wgpu::BufferDescriptor bufferDesc = {};

        // default uniform buffer
        bufferDesc.size = sizeof(DefaultUniformData);
        bufferDesc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
        maBuffers["default-uniform-buffer"] = device.CreateBuffer(&bufferDesc);
        maBuffers["default-uniform-buffer"].SetLabel("Default Uniform Buffer");
        maBufferSizes["default-uniform-buffer"] = (uint32_t)bufferDesc.size;

        // full screen triangle
        Vertex aFullScreenTriangles[3];
        aFullScreenTriangles[0].mPosition = float4(-1.0f, 3.0f, 0.0f, 1.0f);
        aFullScreenTriangles[0].mNormal = float4(0.0f, 0.0f, 1.0f, 1.0f);
        aFullScreenTriangles[0].mUV = float4(0.0f, -1.0f, 0.0f, 0.0f);

        aFullScreenTriangles[1].mPosition = float4(-1.0f, -1.0f, 0.0f, 1.0f);
        aFullScreenTriangles[1].mNormal = float4(0.0f, 0.0f, 1.0f, 1.0f);
        aFullScreenTriangles[1].mUV = float4(0.0f, 1.0f, 0.0f, 0.0f);

        aFullScreenTriangles[2].mPosition = float4(3.0f, -1.0f, 0.0f, 1.0f);
        aFullScreenTriangles[2].mNormal = float4(0.0f, 0.0f, 1.0f, 1.0f);
        aFullScreenTriangles[2].mUV = float4(2.0f, 1.0f, 0.0f, 0.0f);

        bufferDesc.size = sizeof(Vertex) * 3;
        bufferDesc.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst; 
        maBuffers["full-screen-triangle-vertex-buffer"] = device.CreateBuffer(&bufferDesc);
        maBuffers["full-screen-triangle-vertex-buffer"].SetLabel("Full Screen Triangle Vertex Buffer");
        maBufferSizes["full-screen-triangle-vertex-buffer"] = (uint32_t)bufferDesc.size;
        device.GetQueue().WriteBuffer(
            maBuffers["full-screen-triangle-vertex-buffer"], 
            0, 
            aFullScreenTriangles, 
            3 * sizeof(Vertex));

        uint32_t aiTriangleIndices[] = {0, 1, 2};
        bufferDesc.size = sizeof(uint32_t) * 3;
        bufferDesc.usage = wgpu::BufferUsage::Index | wgpu::BufferUsage::CopyDst; 
        maBuffers["full-screen-triangle-index-buffer"] = device.CreateBuffer(&bufferDesc);
        maBuffers["full-screen-triangle-index-buffer"].SetLabel("Full Screen Triangle Index Buffer");
        maBufferSizes["full-screen-triangle-index-buffer"] = (uint32_t)bufferDesc.size;
        device.GetQueue().WriteBuffer(
            maBuffers["full-screen-triangle-index-buffer"], 
            0, 
            aiTriangleIndices, 
            3 * sizeof(uint32_t));

        bufferDesc.size = 256 * sizeof(float2);
        bufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
        maBuffers["blueNoiseBuffer"] = device.CreateBuffer(&bufferDesc);
        maBuffers["blueNoiseBuffer"].SetLabel("Blue Noise Buffer");
        maBufferSizes["blueNoiseBuffer"] = (uint32_t)bufferDesc.size;

        // blue noise texture
        {
            std::string fileName = "blue-noise.png";
            char* acImageData = nullptr;
            uint32_t iFileSize = Loader::loadFile(&acImageData, fileName);
            assert(iFileSize > 0);

            int32_t iImageWidth = 0, iImageHeight = 0, iNumComp = 0;
            stbi_uc* pImageData = stbi_load_from_memory((stbi_uc const*)acImageData, iFileSize, &iImageWidth, &iImageHeight, &iNumComp, 4);

            Loader::loadFileFree(acImageData);

            wgpu::TextureFormat aViewFormats[] = {wgpu::TextureFormat::RGBA8Unorm};
            wgpu::TextureDescriptor textureDesc = {};
            textureDesc.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding;
            textureDesc.dimension = wgpu::TextureDimension::e2D;
            textureDesc.format = wgpu::TextureFormat::RGBA8Unorm;
            textureDesc.mipLevelCount = 1;
            textureDesc.sampleCount = 1;
            textureDesc.size.depthOrArrayLayers = 1;
            textureDesc.size.width = iImageWidth;
            textureDesc.size.height = iImageHeight;
            textureDesc.viewFormatCount = 1;
            textureDesc.viewFormats = aViewFormats;
            maTextures["blueNoiseTexture"] = mpDevice->CreateTexture(&textureDesc);
            maTextures["blueNoiseTexture"].SetLabel("blueNoiseTexture");

#if defined(__EMSCRIPTEN__)
            wgpu::TextureDataLayout layout = {};
#else
            wgpu::TexelCopyBufferLayout layout = {};
#endif // __EMSCRIPTEN__
            layout.bytesPerRow = iImageWidth * 4 * sizeof(char);
            layout.offset = 0;
            layout.rowsPerImage = iImageHeight;
            wgpu::Extent3D extent = {};
            extent.depthOrArrayLayers = 1;
            extent.width = iImageWidth;
            extent.height = iImageHeight;

#if defined(__EMSCRIPTEN__)
            wgpu::ImageCopyTexture destination = {};
#else 
            wgpu::TexelCopyTextureInfo destination = {};
#endif // __EMSCRIPTEN__
            destination.aspect = wgpu::TextureAspect::All;
            destination.mipLevel = 0;
            destination.origin = {.x = 0, .y = 0, .z = 0};
            destination.texture = maTextures["blueNoiseTexture"];
            mpDevice->GetQueue().WriteTexture(
                &destination,
                pImageData,
                iImageWidth * iImageHeight * 4,
                &layout,
                &extent);

            mpDevice->GetQueue().WriteTexture(
                &destination,
                pImageData,
                iImageWidth * iImageHeight * 4,
                &layout,
                &extent);

#if defined(__EMSCRIPTEN__)
            free(acImageData);
#endif // __EMSCRIPTEN__
        }

        mpSampler = desc.mpSampler;
        
        miAtlasImageWidth = 8192;
        miAtlasImageHeight = 8192;
        createTextureAtlas();

        createRenderJobs(desc);

#if 0
        struct UniformData
        {
            uint32_t    miNumMeshes;
            float       mfExplodeMultipler;
        };

        UniformData uniformData;
        uniformData.miNumMeshes = (uint32_t)maMeshExtents.size();
        uniformData.mfExplodeMultipler = 1.0f;
        if(maRenderJobs.find("Mesh Culling Compute") != maRenderJobs.end())
        {
            device.GetQueue().WriteBuffer(
                maRenderJobs["Mesh Culling Compute"]->mUniformBuffers["meshCullingUniformBuffer"],
                0,
                &uniformData,
                sizeof(UniformData));
        }
#endif // #if 0
        
        bufferDesc = {};
        bufferDesc.mappedAtCreation = false;
        bufferDesc.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
        bufferDesc.size = 1024;
        mOutputImageBuffer = mpDevice->CreateBuffer(&bufferDesc);
        mOutputImageBuffer.SetLabel("Read Back Image Buffer");

        bufferDesc = {};
        bufferDesc.mappedAtCreation = false;
        bufferDesc.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
        bufferDesc.size = (1 << 16);
        maBuffers["Debug Read Back Buffer"] = mpDevice->CreateBuffer(&bufferDesc);
        maBuffers["Debug Read Back Buffer"].SetLabel("Debug Read Back Buffer");

        bufferDesc.mappedAtCreation = false;
        bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Indirect;
        bufferDesc.size = 4096;
        bufferDesc.label = "Test Indirect Draw Buffer";
        maBuffers["Test Indirect Draw Buffer"] = mpDevice->CreateBuffer(&bufferDesc);

        mpInstance = desc.mpInstance;
    }

    /*
    **
    */
    void CRenderer::draw(DrawUpdateDescriptor& desc)
    {
        DefaultUniformData defaultUniformData;
        defaultUniformData.mViewMatrix = *desc.mpViewMatrix;
        defaultUniformData.mProjectionMatrix = *desc.mpProjectionMatrix;
        defaultUniformData.mViewProjectionMatrix = *desc.mpViewProjectionMatrix;
        defaultUniformData.mPrevViewProjectionMatrix = *desc.mpPrevViewProjectionMatrix;
        defaultUniformData.mJitteredViewProjectionMatrix = *desc.mpViewProjectionMatrix;
        defaultUniformData.mPrevJitteredViewProjectionMatrix = *desc.mpPrevViewProjectionMatrix;
        defaultUniformData.miScreenWidth = (int32_t)mCreateDesc.miScreenWidth;
        defaultUniformData.miScreenHeight = (int32_t)mCreateDesc.miScreenHeight;
        defaultUniformData.miFrame = miFrame;
        defaultUniformData.mCameraPosition = float4(mCameraPosition, 1.0f);
        defaultUniformData.mCameraLookDir = float4(mCameraLookAt, 1.0f);

        float3 lightDirection = normalize(float3(1.0f, 1.0f, 0.0f));
        defaultUniformData.mLightDirection = float4(lightDirection);
        defaultUniformData.mLightRadiance = float4(2.0f, 2.0f, 2.0f, 1.0f);

        // update default uniform buffer
        mpDevice->GetQueue().WriteBuffer(
            maBuffers["default-uniform-buffer"],
            0,
            &defaultUniformData,
            sizeof(defaultUniformData)
        );

        std::vector<uint32_t> aiIndirectDrawData(128 * 5);

#if 0
#if defined(__EMSCRIPTEN__)
        WGPUBufferMapCallback callback = [](
            WGPUBufferMapAsyncStatus status,
            void* userdata)
            {
                bool* pbStarted = (bool*)userdata;
                *pbStarted = true;
            };

        uint32_t iBufferSizeToRead = 128 * 5 * (uint32_t)sizeof(uint32_t);
        bool bBufferMapped = false;
        wgpuBufferMapAsync(
            maBuffers["Debug Read Back Buffer"].Get(),
            WGPUMapMode_Read,
            0,
            iBufferSizeToRead,
            callback,
            &bBufferMapped
        );
        while(!bBufferMapped)
        {
            emscripten_sleep(1);
        }
        
        DEBUG_PRINTF("%s : %d !!! MAPPED !!!!\n", __FILE__, __LINE__);
        uint32_t const* piData = (uint32_t const*)maBuffers["Debug Read Back Buffer"].GetConstMappedRange(0, iBufferSizeToRead);
        memcpy(aiIndirectDrawData.data(), piData, iBufferSizeToRead);
        maBuffers["Debug Read Back Buffer"].Unmap();

        for(uint32_t i = 0; i < 64; i++)
        {
            DEBUG_PRINTF("%d (%d, %d, %d, %d, %d)\n",
                i,
                aiIndirectDrawData[i * 5],
                aiIndirectDrawData[i * 5 + 1],
                aiIndirectDrawData[i * 5 + 2],
                aiIndirectDrawData[i * 5 + 3],
                aiIndirectDrawData[i * 5 + 4]
            );
        }

#else 
        WGPUBufferMapCallbackInfo callBackInfo = {};
        callBackInfo.userdata1 = aiIndirectDrawData.data();
        callBackInfo.userdata2 = &maBuffers["Debug Read Back Buffer"];
        callBackInfo.mode = WGPUCallbackMode_WaitAnyOnly;
        callBackInfo.callback = [](
            WGPUMapAsyncStatus status, 
            struct WGPUStringView message, 
            WGPU_NULLABLE void* userdata1, 
            WGPU_NULLABLE void* userdata2)
        {
                uint32_t* piOutputData = (uint32_t*)userdata1;
                wgpu::Buffer* pBuffer = (wgpu::Buffer*)userdata2;

                uint32_t const* piData = (uint32_t const*)pBuffer->GetConstMappedRange(0, sizeof(uint32_t) * 128 * 5);
                memcpy(piOutputData, piData, sizeof(uint32_t) * 128 * 5);
                pBuffer->Unmap();
        };

        WGPUFuture future = wgpuBufferMapAsync(
            maBuffers["Debug Read Back Buffer"].Get(),
            WGPUMapMode_Read,
            0,
            128 * 5 * sizeof(uint32_t),
            callBackInfo
        );
        WGPUFutureWaitInfo futureWaitInfo = {};
        futureWaitInfo.future = future;
        wgpuInstanceWaitAny(mpInstance->Get(), 1, &futureWaitInfo, UINT64_MAX);
#endif // __EMSCRIPTEN__
#endif // #if 0

        // clear number of draw calls
        char acClearData[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        if(maRenderJobs.find("Mesh Culling Compute") != maRenderJobs.end())
        {
            mpDevice->GetQueue().WriteBuffer(
                maRenderJobs["Mesh Culling Compute"]->mOutputBufferAttachments["Draw Calls"],
                0,
                acClearData,
                sizeof(acClearData)
            );
        }

        if(miFrame > 2)
        {
            auto iter = std::find_if(
                maRenderJobs.begin(),
                maRenderJobs.end(),
                [](auto& renderJob)
                {
                    return (renderJob.first == "Sky Convolution Graphics");
                }
            );
            if(iter != maRenderJobs.end())
            {
                iter->second->mbEnabled = false;
            }

            iter = std::find_if(
                maRenderJobs.begin(),
                maRenderJobs.end(),
                [](auto& renderJob)
                {
                    return (renderJob.first == "Atmosphere Graphics");
                }
            );
            if(iter != maRenderJobs.end())
            {
                iter->second->mbEnabled = false;
            }
        }

        // add commands from the render jobs
        std::vector<wgpu::CommandBuffer> aCommandBuffer;
        for(auto const& renderJobName : maOrderedRenderJobs)
        {
            Render::CRenderJob* pRenderJob = maRenderJobs[renderJobName].get();
            if(pRenderJob->mbEnabled == false)
            {
                continue;
            }

            wgpu::CommandEncoderDescriptor commandEncoderDesc = {};
            wgpu::CommandEncoder commandEncoder = mpDevice->CreateCommandEncoder(&commandEncoderDesc);
            if(pRenderJob->mType == Render::JobType::Graphics)
            {
                uint32_t iOutputAttachmentWidth = pRenderJob->mOutputImageAttachments.begin()->second.GetWidth();
                uint32_t iOutputAttachmentHeight = pRenderJob->mOutputImageAttachments.begin()->second.GetHeight();

                wgpu::RenderPassDescriptor renderPassDesc = {};
                renderPassDesc.colorAttachmentCount = pRenderJob->maOutputAttachments.size();
                renderPassDesc.colorAttachments = pRenderJob->maOutputAttachments.data();
                renderPassDesc.depthStencilAttachment = &pRenderJob->mDepthStencilAttachment;
                wgpu::RenderPassEncoder renderPassEncoder = commandEncoder.BeginRenderPass(&renderPassDesc);
                std::string renderPassEncoderName = pRenderJob->mName + " Render Pass Encoder";
                renderPassEncoder.SetLabel(renderPassEncoderName.c_str());

                renderPassEncoder.PushDebugGroup(pRenderJob->mName.c_str());

                // bind broup, pipeline, index buffer, vertex buffer, scissor rect, viewport, and draw
                for(uint32_t iGroup = 0; iGroup < 2; iGroup++)
                {
                    renderPassEncoder.SetBindGroup(
                        iGroup,
                        pRenderJob->maBindGroups[iGroup]);
                }

                renderPassEncoder.SetPipeline(pRenderJob->mRenderPipeline);
                
                renderPassEncoder.SetScissorRect(
                    0,
                    0,
                    iOutputAttachmentWidth,
                    iOutputAttachmentHeight);
                renderPassEncoder.SetViewport(
                    0,
                    0,
                    (float)iOutputAttachmentWidth,
                    (float)iOutputAttachmentHeight,
                    0.0f,
                    1.0f);
                
                if(pRenderJob->mPassType == Render::PassType::DrawMeshes)
                {
                    if(maRenderJobs.find("Mesh Culling Compute") == maRenderJobs.end())
                    {
                        assert(mpfnGetVertexBufferNames != nullptr);
                        assert(mpfnGetIndexBufferNames != nullptr);
                        assert(mpfnIndexCounts != nullptr);

                        std::vector<std::string> aMeshVertexBufferNames;
                        (*mpfnGetVertexBufferNames)(
                            aMeshVertexBufferNames
                            );

                        std::vector<std::string> aMeshIndexBufferNames;
                        (*mpfnGetIndexBufferNames)(
                            aMeshIndexBufferNames
                            );

                        std::vector<uint32_t> aiMeshIndexCounts;
                        (*mpfnIndexCounts)(
                            aiMeshIndexCounts
                            );

                        std::vector<std::pair<uint32_t, uint32_t>> aiMeshIndexRanges;
                        (*mpfnGetIndexRanges)(
                            aiMeshIndexRanges
                        );

                        for(uint32_t iMesh = 0; iMesh < aMeshVertexBufferNames.size(); iMesh++)
                        {
                            // dynamic bind group for individual meshes
                            uint32_t iMeshUniformDataOffset = iMesh * 256;
                            renderPassEncoder.SetBindGroup(
                                2,
                                pRenderJob->maBindGroups[2],
                                1,
                                &iMeshUniformDataOffset
                            );

                            renderPassEncoder.SetVertexBuffer(
                                0,
                                maBuffers[aMeshVertexBufferNames[iMesh]]
                            );
                            renderPassEncoder.SetIndexBuffer(
                                maBuffers[aMeshIndexBufferNames[iMesh]],
                                wgpu::IndexFormat::Uint32
                            );
                            uint32_t iIndexCount = aiMeshIndexRanges[iMesh].second - aiMeshIndexRanges[iMesh].first;
                            renderPassEncoder.DrawIndexed(
                                iIndexCount,
                                1,
                                aiMeshIndexRanges[iMesh].first,
                                0,
                                iMesh
                            );
                        }
                    }
                    else
                    {
                        std::vector<std::string> aMeshVertexBufferNames;
                        (*mpfnGetVertexBufferNames)(
                            aMeshVertexBufferNames
                            );

                        std::vector<std::string> aMeshIndexBufferNames;
                        (*mpfnGetIndexBufferNames)(
                            aMeshIndexBufferNames
                        );

                        renderPassEncoder.SetVertexBuffer(
                            0,
                            maBuffers[aMeshVertexBufferNames[0]]
                        );
                        renderPassEncoder.SetIndexBuffer(
                            maBuffers[aMeshIndexBufferNames[0]],
                            wgpu::IndexFormat::Uint32
                        );

#if defined(__EMSCRIPTEN__) || !defined(_MSC_VER)
                        for(uint32_t iMesh = 0; iMesh < 128; iMesh++)
                        {
                            // dynamic bind group for individual meshes
                            uint32_t iOffset = iMesh * 256;
                            renderPassEncoder.SetBindGroup(
                                2,
                                pRenderJob->maBindGroups[2],
                                1,
                                &iOffset
                            );

                            renderPassEncoder.DrawIndexedIndirect(
                                maRenderJobs["Mesh Culling Compute"]->mOutputBufferAttachments["Draw Calls"],
                                iMesh * 5 * sizeof(uint32_t)
                            );
                        }

#else
                        for(uint32_t iMesh = 0; iMesh < 128; iMesh++)
                        {
                            // dynamic bind group for individual meshes
                            uint32_t iOffset = iMesh * 256;
                            renderPassEncoder.SetBindGroup(
                                2,
                                pRenderJob->maBindGroups[2],
                                1,
                                &iOffset);
                        }

                        renderPassEncoder.MultiDrawIndexedIndirect(
                            maRenderJobs["Mesh Culling Compute"]->mOutputBufferAttachments["Draw Calls"],
                            0,
                            128,
                            maRenderJobs["Mesh Culling Compute"]->mOutputBufferAttachments["Num Draw Calls"],
                            0
                        );
#endif // __EMSCRIPTEN__
                    }
                }
                else if(pRenderJob->mPassType == Render::PassType::FullTriangle)
                {
                    renderPassEncoder.SetIndexBuffer(
                        maBuffers["full-screen-triangle-index-buffer"],
                        wgpu::IndexFormat::Uint32
                    );
                    renderPassEncoder.SetVertexBuffer(
                        0,
                        maBuffers["full-screen-triangle-vertex-buffer"]
                    );

                    renderPassEncoder.Draw(3);
                }
                else if(pRenderJob->mPassType == Render::PassType::DrawAnimatedMesh)
                {
                    std::vector<std::string> aAnimVertexBufferNames;
                    std::vector<std::string> aAnimIndexBufferNames;
                    std::vector<std::pair<uint32_t, uint32_t>> aAnimIndexRanges;
                    std::vector<std::pair<uint32_t, uint32_t>> aAnimVertexRanges;

                    mpfnGetAnimVertexBufferNames(aAnimVertexBufferNames);
                    mpfnGetAnimIndexBufferNames(aAnimIndexBufferNames);
                    mpfnGetAnimIndexRanges(aAnimIndexRanges);
                    mpfnGetAnimVertexRanges(aAnimVertexRanges);

                    for(uint32_t iMesh = 0; iMesh < aAnimVertexBufferNames.size(); iMesh++)
                    {
                        uint32_t iOffset = iMesh * 256;

                        // dynamic bind group for individual meshes
                        renderPassEncoder.SetBindGroup(
                            2,
                            pRenderJob->maBindGroups[2],
                            1,
                            &iOffset);

                        std::string const& vertexBufferName = aAnimVertexBufferNames[iMesh];
                        std::string const& indexBufferName = aAnimIndexBufferNames[iMesh];

                        uint32_t iNumIndices = aAnimIndexRanges[iMesh].second - aAnimIndexRanges[iMesh].first;
                        uint32_t iIndexOffset = aAnimIndexRanges[iMesh].first;
                        uint32_t iNumVertices = aAnimVertexRanges[iMesh].second - aAnimVertexRanges[iMesh].first;
                        uint32_t iVertexOffset = aAnimVertexRanges[iMesh].first;


                        uint32_t iVertexBufferOffset = (pRenderJob->mpInputVertexBuffer != nullptr) ? iMesh * iNumVertices : 0;
                        renderPassEncoder.SetVertexBuffer(
                            0,
                            (pRenderJob->mpInputVertexBuffer != nullptr) ? *pRenderJob->mpInputVertexBuffer : maBuffers[vertexBufferName],
                            0
                        );

                        renderPassEncoder.SetIndexBuffer(
                            maBuffers[indexBufferName],
                            wgpu::IndexFormat::Uint32
                        );

                        renderPassEncoder.DrawIndexed(
                            iNumIndices,
                            1,
                            iIndexOffset,
                            iVertexOffset,
                            iMesh
                        );

                    }
                }

                renderPassEncoder.PopDebugGroup();
                renderPassEncoder.End();


            }
            else if(pRenderJob->mType == Render::JobType::Compute)
            {
                wgpu::ComputePassDescriptor computePassDesc = {};
                wgpu::ComputePassEncoder computePassEncoder = commandEncoder.BeginComputePass(&computePassDesc);
                std::string renderEncoderName = pRenderJob->mName + " Render Encoder";
                computePassEncoder.SetLabel(renderEncoderName.c_str());
                computePassEncoder.PushDebugGroup(pRenderJob->mName.c_str());

                // bind broup, pipeline, index buffer, vertex buffer, scissor rect, viewport, and draw
                for(uint32_t iGroup = 0; iGroup < (uint32_t)pRenderJob->maBindGroups.size(); iGroup++)
                {
                    computePassEncoder.SetBindGroup(
                        iGroup,
                        pRenderJob->maBindGroups[iGroup]);
                }
                computePassEncoder.SetPipeline(pRenderJob->mComputePipeline);
                computePassEncoder.DispatchWorkgroups(
                    pRenderJob->mDispatchSize.x,
                    pRenderJob->mDispatchSize.y,
                    pRenderJob->mDispatchSize.z);
                
                computePassEncoder.PopDebugGroup();
                computePassEncoder.End();
            }
            else if(pRenderJob->mType == Render::JobType::Copy)
            {
                commandEncoder.PushDebugGroup(pRenderJob->mName.c_str());
                for(auto const& keyValue : pRenderJob->mInputImageAttachments)
                {
#if defined(__EMSCRIPTEN__)
                    wgpu::ImageCopyTexture srcInfo = {};
                    srcInfo.texture = *keyValue.second;
                    srcInfo.aspect = wgpu::TextureAspect::All;
                    srcInfo.mipLevel = 0;
                    srcInfo.origin.x = 0;
                    srcInfo.origin.y = 0;
                    srcInfo.origin.z = 0;

                    wgpu::ImageCopyTexture dstInfo = {};
                    dstInfo.texture = pRenderJob->mOutputImageAttachments[keyValue.first];
                    dstInfo.aspect = wgpu::TextureAspect::All;
                    dstInfo.mipLevel = 0;
                    dstInfo.origin.x = 0;
                    dstInfo.origin.y = 0;
                    dstInfo.origin.z = 0;

#else 
                    wgpu::TexelCopyTextureInfo srcInfo = {};
                    srcInfo.texture = *keyValue.second;
                    srcInfo.aspect = wgpu::TextureAspect::All;
                    srcInfo.mipLevel = 0;
                    srcInfo.origin.x = 0;
                    srcInfo.origin.y = 0;
                    srcInfo.origin.z = 0;

                    wgpu::TexelCopyTextureInfo dstInfo = {};
                    dstInfo.texture = pRenderJob->mOutputImageAttachments[keyValue.first];
                    dstInfo.aspect = wgpu::TextureAspect::All;
                    dstInfo.mipLevel = 0;
                    dstInfo.origin.x = 0;
                    dstInfo.origin.y = 0;
                    dstInfo.origin.z = 0;
#endif // __EMSCRIPTEN__
                    
                    wgpu::Extent3D copySize = {};
                    copySize.depthOrArrayLayers = 1;
                    copySize.width = srcInfo.texture.GetWidth();
                    copySize.height = srcInfo.texture.GetHeight();
                    commandEncoder.CopyTextureToTexture(&srcInfo, &dstInfo, &copySize);
                }
                
                for(auto& keyValue : pRenderJob->mInputBufferAttachments)
                {
                    assert(pRenderJob->mOutputBufferAttachments[keyValue.first] != nullptr);
                    commandEncoder.CopyBufferToBuffer(
                        *keyValue.second,
                        0,
                        pRenderJob->mOutputBufferAttachments[keyValue.first],
                        0,
                        keyValue.second->GetSize()
                    );
                }
                commandEncoder.PopDebugGroup();
            }

            wgpu::CommandBuffer commandBuffer = commandEncoder.Finish();
            aCommandBuffer.push_back(commandBuffer);

        }   // for all render jobs

        // get selection info from shader via read back buffer
        if(mbWaitingForMeshSelection && maRenderJobs.find(mCaptureImageJobName) != maRenderJobs.end())
        {
            wgpu::CommandEncoderDescriptor commandEncoderDesc = {};
            wgpu::CommandEncoder commandEncoder = mpDevice->CreateCommandEncoder(&commandEncoderDesc);

            commandEncoder.CopyBufferToBuffer(
                maRenderJobs[mCaptureImageJobName]->mUniformBuffers[mCaptureUniformBufferName],
                0,
                mOutputImageBuffer,
                0,
                64
            );

            wgpu::CommandBuffer commandBuffer = commandEncoder.Finish();
            aCommandBuffer.push_back(commandBuffer);

            printf("copy selection buffer\n");
            mbSelectedBufferCopied = true;
        }

        // submit all the job commands
        mpDevice->GetQueue().Submit(
            (uint32_t)aCommandBuffer.size(), 
            aCommandBuffer.data());

        ++miFrame;

    }

    /*
    **
    */
    void CRenderer::createRenderJobs(CreateDescriptor& desc)
    {
        char* acFileContentBuffer = nullptr;
        uint32_t iDataSize = Loader::loadFile(
            &acFileContentBuffer,
            "render-jobs/" + desc.mRenderJobPipelineFilePath,
            true
        );
        assert(iDataSize > 0);

        Render::CRenderJob::CreateInfo createInfo = {};
        createInfo.miScreenWidth = desc.miScreenWidth;
        createInfo.miScreenHeight = desc.miScreenHeight;
        createInfo.mpfnGetBuffer = [](uint32_t& iBufferSize, std::string const& bufferName, void* pUserData)
        {
            Render::CRenderer* pRenderer = (Render::CRenderer*)pUserData;
            assert(pRenderer->maBuffers.find(bufferName) != pRenderer->maBuffers.end());
            
            iBufferSize = pRenderer->maBufferSizes[bufferName];
            return pRenderer->maBuffers[bufferName];
        };
        createInfo.mpUserData = this;

        createInfo.mpfnGetTexture = [](std::string const& textureName, void* pUserData)
        {
            Render::CRenderer* pRenderer = (Render::CRenderer*)pUserData;
            assert(pRenderer->maTextures.find(textureName) != pRenderer->maTextures.end());

            return pRenderer->maTextures[textureName];
        };

        rapidjson::Document doc;
        {
            doc.Parse(acFileContentBuffer);
            Loader::loadFileFree(acFileContentBuffer);
        }

        std::vector<std::string> aRenderJobNames;
        std::vector<std::string> aShaderModuleFilePath;

        auto const& jobs = doc["Jobs"].GetArray();
        for(auto const& job : jobs)
        {
            if(job.HasMember("Disable"))
            {
                if(std::string(job["Disable"].GetString()) == "True")
                {
                    continue;
                }
            }

            createInfo.mName = job["Name"].GetString();
            std::string jobType = job["Type"].GetString();
            createInfo.mJobType = Render::JobType::Graphics;
            if(jobType == "Compute")
            {
                createInfo.mJobType = Render::JobType::Compute;
            }
            else if(jobType == "Copy")
            {
                createInfo.mJobType = Render::JobType::Copy;
            }

            maOrderedRenderJobs.push_back(createInfo.mName);

            std::string passStr = job["PassType"].GetString();
            if(passStr == "Compute")
            {
                createInfo.mPassType = Render::PassType::Compute;
            }
            else if(passStr == "Draw Meshes")
            {
                createInfo.mPassType = Render::PassType::DrawMeshes;
            }
            else if(passStr == "Draw Animated Meshes")
            {
                createInfo.mPassType = Render::PassType::DrawAnimatedMesh;
            }
            else if(passStr == "Full Triangle")
            {
                createInfo.mPassType = Render::PassType::FullTriangle;
            }
            else if(passStr == "Copy")
            {
                createInfo.mPassType = Render::PassType::Copy;
            }
            else if(passStr == "Swap Chain")
            {
                createInfo.mPassType = Render::PassType::SwapChain;
            }
            else if(passStr == "Depth Prepass")
            {
                createInfo.mPassType = Render::PassType::DepthPrepass;
            }
            else if(passStr == "Copy")
            {
                createInfo.mPassType = Render::PassType::Copy;
            }

            createInfo.mpDevice = mpDevice;

            std::string pipelineFilePath = std::string("render-jobs/") + job["Pipeline"].GetString();
            createInfo.mPipelineFilePath = pipelineFilePath;

            createInfo.mpSampler = desc.mpSampler;

            aShaderModuleFilePath.push_back(pipelineFilePath);

            maRenderJobs[createInfo.mName] = std::make_unique<Render::CRenderJob>();
            maRenderJobs[createInfo.mName]->createWithOnlyOutputAttachments(createInfo);
            
            if(jobType == "Compute")
            {
                if(job.HasMember("Dispatch"))
                {
                    auto dispatchArray = job["Dispatch"].GetArray();
                    maRenderJobs[createInfo.mName]->mDispatchSize.x = dispatchArray[0].GetUint();
                    maRenderJobs[createInfo.mName]->mDispatchSize.y = dispatchArray[1].GetUint();
                    maRenderJobs[createInfo.mName]->mDispatchSize.z = dispatchArray[2].GetUint();
                }
            }

            aRenderJobNames.push_back(createInfo.mName);
        }

        std::vector<Render::CRenderJob*> apRenderJobs;
        for(auto const& renderJobName : aRenderJobNames)
        {
            apRenderJobs.push_back(maRenderJobs[renderJobName].get());
        }

        createInfo.mpDefaultUniformBuffer = &maBuffers["default-uniform-buffer"];
        createInfo.mpaRenderJobs = &apRenderJobs;
        uint32_t iIndex = 0;
        std::vector<std::vector<std::tuple<std::string, std::string, std::string, std::string>>> aaDeferredAttachments;
        for(auto const& renderJobName : aRenderJobNames)
        {
            createInfo.mName = renderJobName;
            createInfo.mJobType = maRenderJobs[renderJobName]->mType;
            createInfo.mPassType = maRenderJobs[renderJobName]->mPassType;
            createInfo.mPipelineFilePath = aShaderModuleFilePath[iIndex];

            if(maRenderJobs[renderJobName]->mType == Render::JobType::Copy)
            {
                maRenderJobs[renderJobName]->setCopyAttachments(createInfo);
            }
            else
            {
                //maRenderJobs[renderJobName]->createWithInputAttachmentsAndPipeline(createInfo);
                std::vector<std::tuple<std::string, std::string, std::string, std::string>> aDeferredAttachments;
                maRenderJobs[renderJobName]->createWithInputAttachments(
                    createInfo,
                    aDeferredAttachments
                );
                if(aDeferredAttachments.size() > 0)
                {
                    aaDeferredAttachments.push_back(aDeferredAttachments);
                }
            }
            ++iIndex;
        }

        // create
        iIndex = 0;
        for(auto const& renderJobName : aRenderJobNames)
        {
            createInfo.mName = renderJobName;
            createInfo.mJobType = maRenderJobs[renderJobName]->mType;
            createInfo.mPassType = maRenderJobs[renderJobName]->mPassType;
            createInfo.mPipelineFilePath = aShaderModuleFilePath[iIndex];

            if(createInfo.mJobType == Render::JobType::Copy)
            {
                maRenderJobs[renderJobName]->setCopyAttachments(createInfo);
            }
            iIndex += 1;
        }

        // link deferred input attachments
        {
            for(auto& aAttachmentInfo : aaDeferredAttachments)
            {
                for(auto& attachmentInfo : aAttachmentInfo)
                {
                    std::string const& attachmentName = std::get<0>(attachmentInfo);
                    std::string const& renderJobName = std::get<1>(attachmentInfo);
                    std::string const& parentRenderJobName = std::get<2>(attachmentInfo);
                    std::string const& type = std::get<3>(attachmentInfo);

                    if(type == "buffer")
                    {
                        assert(maRenderJobs[parentRenderJobName]->mOutputBufferAttachments.find(attachmentName) != maRenderJobs[parentRenderJobName]->mOutputBufferAttachments.end());
                        maRenderJobs[renderJobName]->mInputBufferAttachments[attachmentName] = &maRenderJobs[parentRenderJobName]->mOutputBufferAttachments[attachmentName];
                        assert(maRenderJobs[renderJobName]->mInputBufferAttachments[attachmentName]->Get() != nullptr);
                    }
                    else if(type == "texture")
                    {
                        assert(maRenderJobs[parentRenderJobName]->mOutputImageAttachments.find(attachmentName) != maRenderJobs[parentRenderJobName]->mOutputImageAttachments.end());
                        maRenderJobs[renderJobName]->mInputImageAttachments[attachmentName] = &maRenderJobs[parentRenderJobName]->mOutputImageAttachments[attachmentName];
                        assert(maRenderJobs[renderJobName]->mInputImageAttachments[attachmentName]->Get() != nullptr);
                    }
                }
            }
        }

        iIndex = 0;
        for(auto const& renderJobName : aRenderJobNames)
        {
            createInfo.mName = renderJobName;
            createInfo.mJobType = maRenderJobs[renderJobName]->mType;
            createInfo.mPassType = maRenderJobs[renderJobName]->mPassType;
            createInfo.mPipelineFilePath = aShaderModuleFilePath[iIndex];

            if(createInfo.mJobType != Render::JobType::Copy)
            {
                maRenderJobs[renderJobName]->createPipeline(createInfo);
            }

            ++iIndex;
        }

    }

    /*
    **
    */
    wgpu::Texture& CRenderer::getSwapChainTexture()
    {
        //wgpu::Texture& swapChainTexture = maRenderJobs["PBR Graphics"]->mOutputImageAttachments["PBR Output"];
        //wgpu::Texture& swapChainTexture = maRenderJobs["Composite Graphics"]->mOutputImageAttachments["Composite Output"];
        //wgpu::Texture& swapChainTexture = maRenderJobs["TAA Graphics"]->mOutputImageAttachments["TAA Output"];
        //wgpu::Texture& swapChainTexture = maRenderJobs["Mesh Selection Graphics"]->mOutputImageAttachments["Selection Output"];
        //assert(maRenderJobs.find("Mesh Selection Graphics") != maRenderJobs.end());
        //assert(maRenderJobs["Mesh Selection Graphics"]->mOutputImageAttachments.find("Selection Output") != maRenderJobs["Mesh Selection Graphics"]->mOutputImageAttachments.end());

        //wgpu::Texture& swapChainTexture = maRenderJobs["Skin Mesh Graphics"]->mOutputImageAttachments["Color Output"];
        //wgpu::Texture& swapChainTexture = maRenderJobs["Atmosphere Graphics"]->mOutputImageAttachments["Sky Output"];

        //wgpu::Texture& swapChainTexture = maRenderJobs["Spherical Harmonics Diffuse Graphics"]->mOutputImageAttachments["Diffuse SH Coefficients 0 Output"];
        //wgpu::Texture& swapChainTexture = maRenderJobs["Light View Skin Mesh Graphics"]->mOutputImageAttachments["Light View Clip Position Output"];
        //wgpu::Texture& swapChainTexture = maRenderJobs["Shadow Graphics"]->mOutputImageAttachments["Shadow Output"];

        //wgpu::Texture& swapChainTexture = maRenderJobs["Ambient Occlusion Graphics"]->mOutputImageAttachments["Ambient Occlusion Output"];
        //wgpu::Texture& swapChainTexture = maRenderJobs["Ambient Occlusion Graphics"]->mOutputImageAttachments["Indirect Lighting Output"];
        
        //wgpu::Texture& swapChainTexture = maRenderJobs["Light View Skin Mesh Graphics"]->mOutputImageAttachments["Light View World Position Output"];
        //wgpu::Texture& swapChainTexture = maRenderJobs["Spherical Harmonics Diffuse Graphics"]->mOutputImageAttachments["Decoded Output"];

        //wgpu::Texture& swapChainTexture = maRenderJobs["Bilateral Filter Indirect Lighting Graphics"]->mOutputImageAttachments["Bilateral Filtered Output"];
        //wgpu::Texture& swapChainTexture = maRenderJobs["Bilateral Filter Ambient Occlusion Graphics"]->mOutputImageAttachments["Bilateral Filtered Ambient Occlusion Output"];
        //wgpu::Texture& swapChainTexture = maRenderJobs["Bilateral Filter Shadow Graphics"]->mOutputImageAttachments["Bilateral Filtered Shadow Output"];

        //wgpu::Texture& swapChainTexture = maRenderJobs["Lighting Graphics"]->mOutputImageAttachments["Lighting Output"];

        //wgpu::Texture& swapChainTexture = maRenderJobs["Temporal Accumulation Graphics"]->mOutputImageAttachments["Temporal Accumulated Indirect Lighting Output"];
        //wgpu::Texture& swapChainTexture = maRenderJobs["Temporal Accumulation Graphics"]->mOutputImageAttachments["Temporal Accumulated Ambient Occlusion Output"];

        //wgpu::Texture & swapChainTexture = maRenderJobs["SSGI Graphics"]->mOutputImageAttachments["Indirect Diffuse Lighting Output"];

        //wgpu::Texture& swapChainTexture = maRenderJobs["Decode Indirect Diffuse And Ambient Occlusion Graphics"]->mOutputImageAttachments["Indirect Diffuse Lighting Output"];
        //wgpu::Texture& swapChainTexture = maRenderJobs["Decode Indirect Diffuse And Ambient Occlusion Graphics"]->mOutputImageAttachments["Ambient Occlusion Output"];

        //wgpu::Texture& swapChainTexture = maRenderJobs["Variance Bilateral Filter Indirect Lighting Graphics"]->mOutputImageAttachments["Filtered Indirect Diffuse Output"];
        wgpu::Texture& swapChainTexture = maRenderJobs["TAA Graphics"]->mOutputImageAttachments["TAA Output"];
        //wgpu::Texture& swapChainTexture = maRenderJobs["Final Composite Graphics"]->mOutputImageAttachments["Composited Output"];

        return swapChainTexture;
    }

    /*
    **
    */
    bool CRenderer::setBufferData(
        std::string const& jobName,
        std::string const& bufferName,
        void* pData,
        uint32_t iOffset,
        uint32_t iDataSize)
    {
        bool bRet = false;

        assert(maRenderJobs.find(jobName) != maRenderJobs.end());
        Render::CRenderJob* pRenderJob = maRenderJobs[jobName].get();
        if(pRenderJob->mUniformBuffers.find(bufferName) != pRenderJob->mUniformBuffers.end())
        {
            wgpu::Buffer uniformBuffer = pRenderJob->mUniformBuffers[bufferName];
            mpDevice->GetQueue().WriteBuffer(
                uniformBuffer,
                iOffset,
                pData,
                iDataSize
            );

            bRet = true;
        }

        return bRet;
    }

    /*
    **
    */
    bool CRenderer::setBufferData(
        std::string const& bufferName,
        void* pData,
        uint32_t iOffset,
        uint32_t iDataSize)
    {
        bool bRet = true;

        //assert(maBuffers.find(bufferName) != maBuffers.end());
        if(maBuffers.find(bufferName) == maBuffers.end())
        {
            DEBUG_PRINTF("!!! can\'t find buffer \"%s\"\n",
                bufferName.c_str()
            );
        }
        else 
        {
            mpDevice->GetQueue().WriteBuffer(
                maBuffers[bufferName],
                iOffset,
                pData,
                iDataSize
            );
        }

        return bRet;
    }

    /*
    **
    */
    void CRenderer::highLightSelectedMesh(int32_t iX, int32_t iY)
    {
        mCaptureImageName = "Selection Output";
        mCaptureImageJobName = "Mesh Selection Graphics";
        mCaptureUniformBufferName = "selectedMesh";

        mSelectedCoord = int2(iX, iY);
        mSelectMeshInfo.miMeshID = 0;
    }

    /*
    **
    */
    CRenderer::SelectMeshInfo const& CRenderer::getSelectionInfo()
    {
        return mSelectMeshInfo;
    }

    /*
    **
    */
    void CRenderer::createTextureAtlas()
    {
        // diffuse texture atlas
        wgpu::TextureFormat aViewFormats[] = {wgpu::TextureFormat::RGBA8Unorm};
        wgpu::TextureDescriptor textureDesc = {};
        textureDesc.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding;
        textureDesc.dimension = wgpu::TextureDimension::e2D;
        textureDesc.format = wgpu::TextureFormat::RGBA8Unorm;
        textureDesc.mipLevelCount = 1;
        textureDesc.sampleCount = 1;
        textureDesc.size.depthOrArrayLayers = 1;
        textureDesc.size.width = miAtlasImageWidth;
        textureDesc.size.height = miAtlasImageHeight;
        textureDesc.viewFormatCount = 1;
        textureDesc.viewFormats = aViewFormats;

        maTextures["totalDiffuseTextures"] = mpDevice->CreateTexture(&textureDesc);
        mDiffuseTextureAtlas = maTextures["totalDiffuseTextures"];

        wgpu::TextureViewDescriptor viewDesc = {};
        viewDesc.arrayLayerCount = 1;
        viewDesc.aspect = wgpu::TextureAspect::All;
        viewDesc.baseArrayLayer = 0;
        viewDesc.baseMipLevel = 0;
        viewDesc.dimension = wgpu::TextureViewDimension::e2D;
        viewDesc.format = wgpu::TextureFormat::RGBA8Unorm;
        viewDesc.label = "Diffuse Texture Atlas";
        viewDesc.mipLevelCount = 1;
#if !defined(__EMSCRIPTEN__)
        viewDesc.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding;
#endif // __EMSCRIPTEN__
        mDiffuseTextureAtlasView = mDiffuseTextureAtlas.CreateView(&viewDesc);

        wgpu::BufferDescriptor bufferDesc = {};
        bufferDesc.mappedAtCreation = false;
        bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Storage;
#if defined(__EMSCRIPTEN__)
        bufferDesc.size = std::max((uint32_t)sizeof(TextureAtlasInfo) * 512, 64u);
#else 
        bufferDesc.size = max((uint32_t)sizeof(TextureAtlasInfo) * 512, 64u);
#endif // __EMSCRIPTEN__
        maBuffers["diffuseTextureAtlasInfoBuffer"] = mpDevice->CreateBuffer(&bufferDesc);
        maBuffers["diffuseTextureAtlasInfoBuffer"].SetLabel("Diffuse Texture Atlas Info Buffer");
    }

    /*
    **
    */
    void CRenderer::loadTexturesIntoAtlas(
        std::string const& meshFilePath,
        std::string const& dir)
    {
        if(maTextures.find("totalDiffuseTextures") == maTextures.end())
        {
            createTextureAtlas();
        }

        std::vector<std::string> aDiffuseTextureNames;
        std::vector<std::string> aEmissiveTextureNames;
        std::vector<std::string> aSpecularTextureNames;
        std::vector<std::string> aNormalTextureNames;
        {
            char* acTextureNames = nullptr;
            uint32_t iSize = Loader::loadFile(&acTextureNames, meshFilePath + "-texture-names.tex");
            if(iSize > 0)
            {
                uint32_t iDiffuseSignature = ('D') | ('F' << 8) | ('S' << 16) | ('E' << 24);
                uint32_t iEmissiveSignature = ('E') | ('M' << 8) | ('S' << 16) | ('V' << 24);
                uint32_t iSpecularSignature = ('S') | ('P' << 8) | ('C' << 16) | ('L' << 24);
                uint32_t iNormalSignature = ('N') | ('R' << 8) | ('M' << 16) | ('L' << 24);
                uint32_t iMetalRoughessSignature = ('M') | ('L' << 8) | ('R' << 16) | ('H' << 24);

                uint32_t const* piData = (uint32_t const*)acTextureNames;
                char const* pcEnd = ((char const*)piData) + iSize;

                for(uint32_t iType = 0; iType < 4; iType++)
                {
                    uint32_t iSignature = *piData++;
                    uint32_t iNumTextures = *piData++;
                    char const* pcChar = (char const*)piData;
                    for(uint32_t i = 0; i < iNumTextures; i++)
                    {
                        std::vector<char> acName;
                        while(*pcChar != '\0')
                        {
                            acName.push_back(*pcChar++);
                        }
                        acName.push_back(*pcChar++);

                        std::string convertedName = std::string(acName.data());
                        auto iter = convertedName.rfind("/");
                        if(iter == std::string::npos)
                        {
                            iter = convertedName.rfind("\\");
                        }

                        std::string baseName = convertedName;
                        if(iter != std::string::npos)
                        {
                            baseName = convertedName.substr(iter);
                        }
                        iter = baseName.rfind(".");
                        std::string noExtension = baseName.substr(0, iter);
                        std::string oldFileExtension = baseName.substr(iter);

                        if(oldFileExtension != ".jpeg" && oldFileExtension != ".png" && oldFileExtension != ".jpg")
                        {
                            noExtension += ".png";
                        }
                        else
                        {
                            noExtension += oldFileExtension;
                        }

                        if(iSignature == iDiffuseSignature)
                        {
                            aDiffuseTextureNames.push_back(noExtension);
                        }
                        else if(iSignature == iEmissiveSignature)
                        {
                            aEmissiveTextureNames.push_back(noExtension);
                        }
                        else if(iSignature == iSpecularSignature)
                        {
                            aSpecularTextureNames.push_back(noExtension);
                        }
                        else if(iSignature == iNormalSignature)
                        {
                            aNormalTextureNames.push_back(noExtension);
                        }
                    }
                    piData = (uint32_t const*)pcChar;
                    if(pcChar == pcEnd)
                    {
                        break;
                    }

                }   // for texture type

                Loader::loadFileFree(acTextureNames);

                int32_t iAtlasIndex = 0;
                int32_t iX = 0, iY = 0;
                int32_t iLargestHeight = 0;

                if(maTextureAtlasInfo.size() > 0)
                {
                    iX = maTextureAtlasInfo[maTextureAtlasInfo.size() - 1].miTextureCoord.x + maTextureAtlasInfo[maTextureAtlasInfo.size() - 1].miImageWidth;
                    iY = maTextureAtlasInfo[maTextureAtlasInfo.size() - 1].miTextureCoord.y;
                }

                auto copyToAtlas = [&](
                    int32_t& iX,
                    int32_t& iY,
                    int32_t iAtlasImageWidth,
                    int32_t iAtlasImageHeight,
                    std::string const& textureName,
                    wgpu::Texture& textureAtlas,
                    int32_t& iLargestHeight)
                    {
                        std::string parsedTextureName = dir + "/" + textureName;
                        char* acTextureImageData = nullptr;
                        uint32_t iSize = Loader::loadFile(&acTextureImageData, parsedTextureName);
                        assert(iSize > 0);
                        int32_t iImageWidth = 0, iImageHeight = 0, iImageComp = 0;
                        stbi_uc* pImageData = stbi_load_from_memory(
                            (stbi_uc const*)acTextureImageData,
                            (int32_t)iSize,
                            &iImageWidth,
                            &iImageHeight,
                            &iImageComp,
                            4
                        );
                        Loader::loadFileFree(acTextureImageData);

                        if(pImageData)
                        {
#if defined(__EMSCRIPTEN__)
                            iLargestHeight = std::max(iLargestHeight, iImageHeight);
#else
                            iLargestHeight = max(iLargestHeight, iImageHeight);
#endif // __EMSCRIPTEN__
                            if(iX + iImageWidth >= iAtlasImageWidth)
                            {
                                iX = 0;
                                iY += iLargestHeight;
                                iLargestHeight = 0;
                            }

#if defined(__EMSCRIPTEN__)
                            wgpu::TextureDataLayout layout = {};
#else
                            wgpu::TexelCopyBufferLayout layout = {};
#endif // __EMSCRIPTEN__
                            layout.bytesPerRow = iImageWidth * 4 * sizeof(char);
                            layout.offset = 0;
                            layout.rowsPerImage = iImageHeight;
                            wgpu::Extent3D extent = {};
                            extent.depthOrArrayLayers = 1;
                            extent.width = iImageWidth;
                            extent.height = iImageHeight;

#if defined(__EMSCRIPTEN__)
                            wgpu::ImageCopyTexture destination = {};
#else 
                            wgpu::TexelCopyTextureInfo destination = {};
#endif // __EMSCRIPTEN__
                            destination.aspect = wgpu::TextureAspect::All;
                            destination.mipLevel = 0;
                            destination.origin = {.x = (uint32_t)iX, .y = (uint32_t)iY, .z = 0};
                            destination.texture = textureAtlas;
                            mpDevice->GetQueue().WriteTexture(
                                &destination,
                                pImageData,
                                iImageWidth * iImageHeight * 4,
                                &layout,
                                &extent);

                            TextureAtlasInfo info = {};
                            info.miTextureCoord = uint2(iX, iY);
                            info.miTextureID = iAtlasIndex;
                            info.mUV = float2(float(iX) / float(iAtlasImageWidth), float(iY) / float(iAtlasImageHeight));
                            info.miImageWidth = iImageWidth;
                            info.miImageHeight = iImageHeight;
                            maTextureAtlasInfo.push_back(info);

                            iX += iImageWidth;

                            stbi_image_free(pImageData);

#if defined(__EMSCRIPTEN__)
                            free(acTextureImageData);
#endif // __EMSCRIPTEN__
                        }
                        else
                        {
                            DEBUG_PRINTF("!!! Can\'t load \"%s\"\n", parsedTextureName.c_str());
                        }
                    };


                for(auto const& diffuseTextureName : aDiffuseTextureNames)
                {
                    copyToAtlas(iX, iY, miAtlasImageWidth, miAtlasImageHeight, diffuseTextureName, mDiffuseTextureAtlas, iLargestHeight);

                    ++iAtlasIndex;
                }

            }

        }   // textures

        mpDevice->GetQueue().WriteBuffer(
            maBuffers["diffuseTextureAtlasInfoBuffer"],
            0,
            maTextureAtlasInfo.data(),
            sizeof(TextureAtlasInfo)* (uint32_t)maTextureAtlasInfo.size()
        );
    }

}   // Render
