#include <game/app.h>
#include <math/vec.h>
#include <math/mat4.h>

#include <render/renderer.h>
#include <loader/loader.h>
#include <render/Vertex.h>

#include <utils/LogPrint.h>

#include <external/stb_image/stb_image.h>
#include <external/rapidjson/document.h>

#include <assert.h>

struct StaticMeshModelUniform
{
    float4x4        mModelMatrix;
    float4x4        mLightViewProjectionMatrix;
    float4          mTextureAtlasUV;
    float4          mExtraInfo;
};

struct AnimMeshModelUniform
{
    float4x4        mModelMatrix;
    float4x4        mLightViewProjectionMatrix;
    float4          mTextureAtlasUV;
    float4          mExtraInfo;
};

extern CCamera gCamera;

/*
**
*/
void CApp::init(CreateInfo& createInfo)
{
    mCreateInfo = createInfo;

    mCreateInfo.mpRenderer->setGetVertexBufferNames(getVertexBufferNames);
    mCreateInfo.mpRenderer->setGetIndexBufferNames(getIndexBufferNames);
    mCreateInfo.mpRenderer->setGetIndexCounts(getIndexCounts);
    mCreateInfo.mpRenderer->setGetIndexRanges(getIndexRanges);
    mCreateInfo.mpRenderer->setGetAnimVertexBufferNames(getAnimVertexBufferNames);
    mCreateInfo.mpRenderer->setGetAnimIndexBufferNames(getAnimIndexBufferNames);
    mCreateInfo.mpRenderer->setGetAnimIndexRanges(getAnimIndexRanges);
    mCreateInfo.mpRenderer->setGetAnimVertexRanges(getAnimVertexRanges);
    mCreateInfo.mpRenderer->setGetVertexRanges(getVertexRanges);

    maMeshModelInfo.resize(4);

    maMeshModelInfo[0] = {};
    maMeshModelInfo[1] = {};
    maMeshModelInfo[2] = {};
    maMeshModelInfo[3] = {};

    maMeshModelInfo[0].miAnimIndex = 0;
    maMeshModelInfo[1].miAnimIndex = 1;
    maMeshModelInfo[2].miStaticIndex = 0;
    maMeshModelInfo[3].miStaticIndex = 1;

    maMeshModelInfoDB["pitcher"] = 0;
    maMeshModelInfoDB["batter"] = 1;
    maMeshModelInfoDB["ball"] = 2;
    maMeshModelInfoDB["bat"] = 3;

    maAnimMeshModelMatrices.resize(128);
    maStaticMeshModelMatrices.resize(128);

    mPitchSimulator.reset();

    mGameState = GAME_STATE_PITCH_WINDUP;

    maAnimationNameInfo.resize(2);
    maAnimationNameInfo[0].mSrcAnimationName = "spider-man-bind-new-rig-pitching-2";
    maAnimationNameInfo[0].mDatabaseName = "pitcher";
    maAnimationNameInfo[0].mfAnimSpeed = 8.0f;
    maAnimationNameInfo[1].mSrcAnimationName = "spider-man-bind-new-rig-ik-batting-with-bat";
    maAnimationNameInfo[1].mDatabaseName = "batter";
    maAnimationNameInfo[1].mfAnimSpeed = 2.0f;

    srand((uint32_t)time(0));

    maLightViewCameras[0].setProjectionType(ProjectionType::PROJECTION_ORTHOGRAPHIC);
    maLightViewCameras[0].setLookAt(float3(0.0f, 0.0f, -9.0f));
    maLightViewCameras[0].setPosition(float3(0.0f, 0.0f, -9.0f) + normalize(float3(0.2f, 1.0f, 0.0f) * 20.0f));

    CameraUpdateInfo cameraUpdateInfo = {};
    cameraUpdateInfo.mfFar = 30.0f;
    cameraUpdateInfo.mfNear = -30.0f;
    cameraUpdateInfo.mfViewWidth = 20.0f;
    cameraUpdateInfo.mfViewHeight = 20.0f;
    cameraUpdateInfo.mUp = float3(0.0f, 1.0f, 0.0f);
    maLightViewCameras[0].update(cameraUpdateInfo);

    wgpu::BufferDescriptor bufferDesc = {};
    bufferDesc.label = "lightingUniformBuffer";
    bufferDesc.mappedAtCreation = false;
    bufferDesc.size = 1024;
    bufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
    maBuffers["lightingUniformBuffer"] = mCreateInfo.mpDevice->CreateBuffer(&bufferDesc);
    mCreateInfo.mpRenderer->registerBuffer(
        "lightingUniformBuffer",
        maBuffers["lightingUniformBuffer"]
    );

    maBallTrailPositions.resize(256);
    miNumTrailingBalls = 0;

    //std::vector<char> acFileContentBuffer;
    char* acFileContentBuffer = nullptr;
    uint32_t iDataSize = Loader::loadFile(
        &acFileContentBuffer, 
        "spider-man-bind-new-rig-ik-batting-with-bat-bat-global-transform-matrices.bin", 
        false);
    assert(iDataSize > 0);
    uint32_t const* piData = (uint32_t const*)acFileContentBuffer;
    uint32_t iNumFrames = *piData++;
    float4x4 const* pfFloat4x4 = (float4x4 const*)piData;
    maBatGlobalTransforms.resize(iNumFrames);
    mafBatGlobalFrameTimes.resize(iNumFrames);
    memcpy(maBatGlobalTransforms.data(), pfFloat4x4, sizeof(float4x4) * iNumFrames);
    pfFloat4x4 += iNumFrames;
    float const* pfData = (float const*)pfFloat4x4;
    memcpy(mafBatGlobalFrameTimes.data(), pfData, sizeof(float) * iNumFrames);

    for(auto& fFrameTime : mafBatGlobalFrameTimes)
    {
        fFrameTime /= maAnimationNameInfo[1].mfAnimSpeed;
    }

    Loader::loadFileFree(acFileContentBuffer);
}

/*
**
*/
void CApp::getVertexBufferNames(std::vector<std::string>& aVertexBufferNames)
{
    extern CApp gApp;
    aVertexBufferNames = gApp.maVertexBufferNames;
}

/*
**
*/
void CApp::getIndexBufferNames(std::vector<std::string>& aIndexBufferNames)
{
    extern CApp gApp;
    aIndexBufferNames = gApp.maIndexBufferNames;
}

/*
**
*/
void CApp::getIndexCounts(std::vector<uint32_t>& aiIndexCounts)
{
    extern CApp gApp;

    uint32_t iIndexCount = gApp.maBufferSizes["baseball-index-buffer"] / sizeof(uint32_t);
    aiIndexCounts.push_back(iIndexCount);
}

/*
**
*/
void CApp::getIndexRanges(std::vector<std::pair<uint32_t, uint32_t>>& aIndexRanges)
{
    extern CApp gApp;
    for(auto const& triangleRange : gApp.maMeshTriangleRanges)
    {
        aIndexRanges.push_back(std::make_pair(triangleRange.miStart, triangleRange.miEnd));
    }
}

/*
**
*/
void CApp::getVertexRanges(std::vector<std::pair<uint32_t, uint32_t>>& aVertexRanges)
{
    extern CApp gApp;
    for(auto const& triangleRange : gApp.maMeshVertexRanges)
    {
        aVertexRanges.push_back(std::make_pair(triangleRange.miStart, triangleRange.miEnd));
    }
}

/*
**
*/
void CApp::getAnimVertexBufferNames(std::vector<std::string>& aVertexBufferNames)
{
    extern CApp gApp;
    //aVertexBufferNames = gApp.maAnimMeshVertexBufferNames;
    aVertexBufferNames = gApp.maTotalAnimMeshVertexBufferNames;
}

/*
**
*/
void CApp::getAnimIndexBufferNames(std::vector<std::string>& aIndexBufferNames)
{
    extern CApp gApp;
    //aIndexBufferNames = gApp.maAnimMeshIndexBufferNames;
    aIndexBufferNames = gApp.maTotalAnimMeshIndexBufferNames;
}

/*
**
*/
void CApp::getAnimIndexRanges(std::vector<std::pair<uint32_t, uint32_t>>& aIndexRanges)
{
    extern CApp gApp;
    for(auto const& triangleRange : gApp.maAnimMeshTriangleRanges)
    {
        aIndexRanges.push_back(std::make_pair(triangleRange.miStart, triangleRange.miEnd));
    }
}

/*
**
*/
void CApp::getAnimVertexRanges(std::vector<std::pair<uint32_t, uint32_t>>& aVertexRanges)
{
    extern CApp gApp;
    //for(auto const& vertexRange : gApp.maAnimMeshVertexRanges)
    //{
    //    aVertexRanges.push_back(std::make_pair(vertexRange.miStart, vertexRange.miEnd));
    //}
    for(auto const& matchVertexRangeToMeshInstance : gApp.maMatchVertexRangeToMeshInstance)
    {
        aVertexRanges.push_back(std::make_pair(matchVertexRangeToMeshInstance.miStart, matchVertexRangeToMeshInstance.miEnd));
    }
}


/*
**
*/
void CApp::loadMeshes(
    std::string const& dir)
{

    //std::string meshModelName = "baseball-ground-bat";
    std::string meshModelName = "baseball-bat-stadium-2";

    char* acTriangleBuffer = nullptr;
    uint64_t iSize = Loader::loadFile(&acTriangleBuffer, meshModelName + "-triangles.bin");
    DEBUG_PRINTF("acTriangleBuffer = 0x%llX size: %lld\n", (uint64_t)acTriangleBuffer, iSize);
    uint32_t const* piData = (uint32_t const*)acTriangleBuffer;

    maStaticMeshModelNames.push_back(meshModelName);

    uint32_t iNumMeshes = *piData++;
    uint32_t iNumTotalVertices = *piData++;
    uint32_t iNumTotalTriangles = *piData++;
    uint32_t iVertexSize = *piData++;
    uint32_t iTriangleStartOffset = *piData++;

    DEBUG_PRINTF("num meshes: %d\n", iNumMeshes);
    DEBUG_PRINTF("num total vertices: %d\n", iNumTotalVertices);

    // triangle ranges for all the meshes
    maMeshTriangleRanges.resize(iNumMeshes);
    memcpy(maMeshTriangleRanges.data(), piData, sizeof(MeshTriangleRange) * iNumMeshes);
    piData += (2 * iNumMeshes);

    // the total mesh extent is at the very end of the list
    MeshExtent const* pMeshExtent = (MeshExtent const*)piData;
    maMeshExtents.resize(iNumMeshes + 1);
    memcpy(maMeshExtents.data(), pMeshExtent, sizeof(MeshExtent) * (iNumMeshes + 1));
    pMeshExtent += (iNumMeshes + 1);
    mTotalMeshExtent = maMeshExtents.back();

    // all the mesh vertices
    std::vector<Vertex> aTotalMeshVertices(iNumTotalVertices);
    Vertex const* pVertices = (Vertex const*)pMeshExtent;
    memcpy(aTotalMeshVertices.data(), pVertices, iNumTotalVertices * sizeof(Vertex));
    pVertices += iNumTotalVertices;

    // all the triangle indices
    std::vector<uint32_t> aiTotalMeshTriangleIndices(iNumTotalTriangles * 3);
    piData = (uint32_t const*)pVertices;
    memcpy(aiTotalMeshTriangleIndices.data(), piData, iNumTotalTriangles * 3 * sizeof(uint32_t));

    Loader::loadFileFree(acTriangleBuffer);

    wgpu::BufferDescriptor bufferDesc = {};

    std::string vertexBufferName = meshModelName + "-vertex-buffer";
    std::string indexBufferName = meshModelName + "-index-buffer";

    for(uint32_t i = 0; i < iNumMeshes; i++)
    {
        maVertexBufferNames.push_back(vertexBufferName);
        maIndexBufferNames.push_back(indexBufferName);
    }

    bufferDesc.size = iNumTotalVertices * sizeof(Vertex);
    bufferDesc.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
    maBuffers[vertexBufferName] = mCreateInfo.mpDevice->CreateBuffer(&bufferDesc);
    maBuffers[vertexBufferName].SetLabel(vertexBufferName.c_str());
    maBufferSizes[vertexBufferName] = (uint32_t)bufferDesc.size;

    bufferDesc.size = aiTotalMeshTriangleIndices.size() * sizeof(uint32_t);
    bufferDesc.usage = wgpu::BufferUsage::Index | wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
    maBuffers[indexBufferName] = mCreateInfo.mpDevice->CreateBuffer(&bufferDesc);
    maBuffers[indexBufferName].SetLabel(indexBufferName.c_str());
    maBufferSizes[indexBufferName] = (uint32_t)bufferDesc.size;

    bufferDesc.size = iNumTotalVertices * sizeof(Vertex);
    bufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
    maBuffers["meshTriangleIndexRanges"] = mCreateInfo.mpDevice->CreateBuffer(&bufferDesc);
    maBuffers["meshTriangleIndexRanges"].SetLabel("Mesh Triangle Ranges");
    maBufferSizes["meshTriangleIndexRanges"] = (uint32_t)bufferDesc.size;

    bufferDesc.size = (iNumMeshes + 1) * sizeof(MeshExtent);
    bufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
    maBuffers["meshExtents"] = mCreateInfo.mpDevice->CreateBuffer(&bufferDesc);
    maBuffers["meshExtents"].SetLabel("Train Mesh Extents");
    maBufferSizes["meshExtents"] = (uint32_t)bufferDesc.size;

    mCreateInfo.mpDevice->GetQueue().WriteBuffer(maBuffers[vertexBufferName], 0, aTotalMeshVertices.data(), iNumTotalVertices * sizeof(Vertex));
    mCreateInfo.mpDevice->GetQueue().WriteBuffer(maBuffers[indexBufferName], 0, aiTotalMeshTriangleIndices.data(), aiTotalMeshTriangleIndices.size() * sizeof(uint32_t));
    mCreateInfo.mpDevice->GetQueue().WriteBuffer(maBuffers["meshTriangleIndexRanges"], 0, maMeshTriangleRanges.data(), maMeshTriangleRanges.size() * sizeof(MeshTriangleRange));
    mCreateInfo.mpDevice->GetQueue().WriteBuffer(maBuffers["meshExtents"], 0, maMeshExtents.data(), maMeshExtents.size() * sizeof(MeshExtent));

    mCreateInfo.mpRenderer->registerBuffer(
        vertexBufferName,
        maBuffers[vertexBufferName]
    );
    mCreateInfo.mpRenderer->registerBuffer(
        indexBufferName,
        maBuffers[indexBufferName]
    );
    mCreateInfo.mpRenderer->registerBuffer(
        "meshTriangleIndexRanges",
        maBuffers["meshTriangleIndexRanges"]
    );
    mCreateInfo.mpRenderer->registerBuffer(
        "meshExtents",
        maBuffers["meshExtents"]
    );
    

    char* acMaterialID = nullptr;
    bufferDesc.size = Loader::loadFile(&acMaterialID, meshModelName + ".mid");
    bufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
    maBuffers["meshMaterialIDs"] = mCreateInfo.mpDevice->CreateBuffer(&bufferDesc);
    maBuffers["meshMaterialIDs"].SetLabel("Mesh Material IDs");
    maBufferSizes["meshMaterialIDs"] = (uint32_t)bufferDesc.size;

    mCreateInfo.mpDevice->GetQueue().WriteBuffer(
        maBuffers["meshMaterialIDs"],
        0,
        acMaterialID,
        bufferDesc.size);
    Loader::loadFileFree(acMaterialID);
    char* acMaterials = nullptr;
    bufferDesc.size = Loader::loadFile(&acMaterials, meshModelName + ".mat");
    printf("mesh material size: %d\n", (uint32_t)bufferDesc.size);


    bufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
    maBuffers["meshMaterials"] = mCreateInfo.mpDevice->CreateBuffer(&bufferDesc);
    maBuffers["meshMaterials"].SetLabel("Mesh Materials");
    maBufferSizes["meshMaterials"] = (uint32_t)bufferDesc.size;

    mCreateInfo.mpDevice->GetQueue().WriteBuffer(
        maBuffers["meshMaterials"],
        0,
        acMaterials,
        bufferDesc.size
    );

    Loader::loadFileFree(acMaterials);

    mCreateInfo.mpRenderer->registerBuffer(
        "meshMaterials",
        maBuffers["meshMaterials"]
    );
    mCreateInfo.mpRenderer->registerBuffer(
        "meshMaterialIDs",
        maBuffers["meshMaterialIDs"]
    );

    bufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
    bufferDesc.size = 50 * sizeof(ObjectInfo);
    maBuffers["meshModelInfo"] = mCreateInfo.mpDevice->CreateBuffer(&bufferDesc);
    maBuffers["meshModelInfo"].SetLabel("meshModelInfo");
    maBufferSizes["meshModelInfo"] = (uint32_t)bufferDesc.size;

    bufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
    bufferDesc.size = maAnimMeshModelMatrices.size() * sizeof(float4x4);
    maBuffers["animMeshModelMatrices"] = mCreateInfo.mpDevice->CreateBuffer(&bufferDesc);
    maBuffers["animMeshModelMatrices"].SetLabel("animMeshModelMatrices");
    maBufferSizes["animMeshModelMatrices"] = (uint32_t)bufferDesc.size;

    bufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
    bufferDesc.size = maStaticMeshModelMatrices.size() * sizeof(float4x4);
    maBuffers["staticMeshModelMatrices"] = mCreateInfo.mpDevice->CreateBuffer(&bufferDesc);
    maBuffers["staticMeshModelMatrices"].SetLabel("staticMeshModelMatrices");
    maBufferSizes["staticMeshModelMatrices"] = (uint32_t)bufferDesc.size;

    mCreateInfo.mpDevice->GetQueue().WriteBuffer(
        maBuffers["meshModelInfo"],
        0,
        maMeshModelInfo.data(),
        maMeshModelInfo.size() * sizeof(ObjectInfo)
    );

    mCreateInfo.mpDevice->GetQueue().WriteBuffer(
        maBuffers["animMeshModelMatrices"],
        0,
        maAnimMeshModelMatrices.data(),
        maAnimMeshModelMatrices.size() * sizeof(float4x4)
    );

    mCreateInfo.mpDevice->GetQueue().WriteBuffer(
        maBuffers["staticMeshModelMatrices"],
        0,
        maStaticMeshModelMatrices.data(),
        maStaticMeshModelMatrices.size() * sizeof(float4x4)
    );

    mCreateInfo.mpRenderer->registerBuffer(
        "meshModelInfo",
        maBuffers["meshModelInfo"]
    );
    mCreateInfo.mpRenderer->registerBuffer(
        "animMeshModelMatrices",
        maBuffers["animMeshModelMatrices"]
    );
    mCreateInfo.mpRenderer->registerBuffer(
        "staticMeshModelMatrices",
        maBuffers["staticMeshModelMatrices"]
    );

    std::string uniformBufferName = "staticMeshModelUniforms";
    bufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
    bufferDesc.size = 256 * 256;        // 256 is the min binding alignment
    maBuffers[uniformBufferName] = mCreateInfo.mpDevice->CreateBuffer(&bufferDesc);
    maBuffers[uniformBufferName].SetLabel(uniformBufferName.c_str());
    maBufferSizes[uniformBufferName] = (uint32_t)bufferDesc.size;
    mCreateInfo.mpRenderer->registerBuffer(uniformBufferName, maBuffers[uniformBufferName]);

    bufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
    bufferDesc.size = 256;
    maBuffers["shadowUniformBuffer"] = mCreateInfo.mpDevice->CreateBuffer(&bufferDesc);
    maBuffers["shadowUniformBuffer"].SetLabel("shadowUniformBuffer");
    maBufferSizes["shadowUniformBuffer"] = (uint32_t)bufferDesc.size;
    mCreateInfo.mpRenderer->registerBuffer("shadowUniformBuffer", maBuffers["shadowUniformBuffer"]);
    mCreateInfo.mpDevice->GetQueue().WriteBuffer(
        maBuffers["shadowUniformBuffer"],
        0,
        &maLightViewCameras[0].getViewProjectionMatrix(),
        sizeof(float4x4)
    );

    // static mesh model uniforms
    {
        std::vector<char> acBuffer(256 * 256);
        char* pcData = (char*)acBuffer.data();
#if 0
        for(uint32_t i = 0; i < 256; i++)
        {
            float4x4* pMatrix = (float4x4*)pcData;
            *pMatrix++ = float4x4();
            *pMatrix++ = maLightViewCameras[0].getViewProjectionMatrix();
            float4* pFloat4 = (float4*)pMatrix;
            *pFloat4++ = float4(0.0f, 0.0f, 0.0f, 0.0f);
            *pFloat4++ = float4((float)i, 0.0f, 0.0f, 0.0f);

            pcData += 256;
        }
#endif // 3if 0
        for(uint32_t i = 0; i < 256; i++)
        {
            uint32_t* piData = (uint32_t*)pcData;
            *piData++ = i;

            pcData += 256;
        }

        bufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
        bufferDesc.size = 256 * 256;        // 256 is the min binding alignment
        mCreateInfo.mpDevice->GetQueue().WriteBuffer(
            maBuffers["staticMeshModelUniforms"],
            0,
            acBuffer.data(),
            acBuffer.size()
        );

    }

    // mesh culling uniform and visibility flags
    {
        uint32_t aiUniformBufferData[] = {(uint32_t)iNumMeshes, 0};
        bufferDesc.size = 256;
        bufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
        maBuffers["meshCullingUniformBuffer"] = mCreateInfo.mpDevice->CreateBuffer(&bufferDesc);
        maBuffers["meshCullingUniformBuffer"].SetLabel("Mesh Culling Uniform");
        maBufferSizes["meshCullingUniformBuffer"] = (uint32_t)bufferDesc.size;
        mCreateInfo.mpDevice->GetQueue().WriteBuffer(
            maBuffers["meshCullingUniformBuffer"],
            0,
            aiUniformBufferData,
            sizeof(aiUniformBufferData)
        );
        mCreateInfo.mpRenderer->registerBuffer("meshCullingUniformBuffer", maBuffers["meshCullingUniformBuffer"]);

        bufferDesc.size = iNumMeshes * sizeof(uint32_t);
        bufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
        maBuffers["culledFlags"] = mCreateInfo.mpDevice->CreateBuffer(&bufferDesc);
        maBuffers["culledFlags"].SetLabel("Mesh Culled Flags");
        maBufferSizes["culledFlags"] = (uint32_t)bufferDesc.size;
        mCreateInfo.mpRenderer->registerBuffer("culledFlags", maBuffers["culledFlags"]);
    }

    {
        
        char* acTextureImageData = nullptr;
        uint32_t iFileSize = Loader::loadFile(
            &acTextureImageData,
            "target-decal.png",
            true
        );
        int32_t iImageWidth = 0, iImageHeight = 0, iImageComp = 0;
        stbi_uc* pImageData = stbi_load_from_memory(
            (stbi_uc const*)acTextureImageData,
            (int32_t)iFileSize,
            &iImageWidth,
            &iImageHeight,
            &iImageComp,
            4
        );

        Loader::loadFileFree(acTextureImageData);

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
        mBallIndicatorTexture = mCreateInfo.mpDevice->CreateTexture(
            &textureDesc
        );

#if defined(__EMSCRIPTEN__)
        wgpu::TextureDataLayout layout = {};
        wgpu::ImageCopyTexture destination = {};
#else
        wgpu::TexelCopyBufferLayout layout = {};
        wgpu::TexelCopyTextureInfo destination = {};
#endif // __EMSCRIPTEN__

        layout.bytesPerRow = iImageWidth * 4 * sizeof(char);
        layout.offset = 0;
        layout.rowsPerImage = iImageHeight;
        wgpu::Extent3D extent = {};
        extent.depthOrArrayLayers = 1;
        extent.width = iImageWidth;
        extent.height = iImageHeight;
        destination.aspect = wgpu::TextureAspect::All;
        destination.mipLevel = 0;
        destination.origin = {.x = 0, .y = 0, .z = 0};
        destination.texture = mBallIndicatorTexture;
        mCreateInfo.mpDevice->GetQueue().WriteTexture(
            &destination,
            pImageData,
            iImageWidth * iImageHeight * 4,
            &layout,
            &extent);

        stbi_image_free(pImageData);

        mCreateInfo.mpRenderer->registerTexture("decalTexture", mBallIndicatorTexture);

    }

    // mesh instance to mesh model mapping
    {
        struct ModelInstanceMap
        {
            uint32_t                miMeshInstance;
            uint32_t                miModel;
            uint32_t                miPadding0;
            uint32_t                miPadding1;
        };
        std::vector<ModelInstanceMap> aModelInstanceMap(256);
        for(uint32_t i = 0; i < iNumMeshes; i++)
        {
            aModelInstanceMap[i].miMeshInstance = i;
            aModelInstanceMap[i].miModel = i;
        }

        // ball instances
        for(uint32_t i = iNumMeshes; i < 256; i++)
        {
            aModelInstanceMap[i].miMeshInstance = i;
            aModelInstanceMap[i].miModel = 0;
        }

        bufferDesc.size = 256 * sizeof(ModelInstanceMap);
        bufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
        maBuffers["meshInstanceModelMapping"] = mCreateInfo.mpDevice->CreateBuffer(&bufferDesc);
        maBuffers["meshInstanceModelMapping"].SetLabel("Mesh Instance Model Mapping");
        maBufferSizes["meshInstanceModelMapping"] = (uint32_t)bufferDesc.size;
        mCreateInfo.mpRenderer->registerBuffer("meshInstanceModelMapping", maBuffers["meshInstanceModelMapping"]);
        mCreateInfo.mpDevice->GetQueue().WriteBuffer(
            maBuffers["meshInstanceModelMapping"],
            0,
            aModelInstanceMap.data(),
            sizeof(ModelInstanceMap) * aModelInstanceMap.size()
        );
    }

}



/*
**
*/
void CApp::loadAnimMeshes(std::string const& dir)
{
    std::string meshName;
    {
        rapidjson::Document doc;
        char* acFileContentBuffer = nullptr;
        Loader::loadFile(&acFileContentBuffer, "anim_mesh_assets.json", true);
        doc.Parse(acFileContentBuffer);

        // animation file info
        auto const& animModelInstances = doc["Animated Mesh"].GetArray();
        uint32_t iAnimMeshIndex = 0;
        for(auto& animInstance : animModelInstances)
        {
            std::string instanceName = animInstance["Name"].GetString();
            std::string fileName = animInstance["File Name"].GetString();

            auto iter = std::find_if(
                maAnimFileInfo.begin(),
                maAnimFileInfo.end(),
                [fileName](auto& animFileInfo)
                {
                    return fileName == animFileInfo.mFileName;
                }
            );
            if(iter == maAnimFileInfo.end())
            {
                AnimMeshFileInfo animMeshFileInfo;
                animMeshFileInfo.mFileName = fileName;
                animMeshFileInfo.miBaseMeshModel = (uint32_t)maAnimFileInfo.size();
                animMeshFileInfo.maMeshInstanceNames.push_back(instanceName);
                maAnimFileInfo.push_back(animMeshFileInfo);
            }
            else
            {
                iter->maMeshInstanceNames.push_back(instanceName);
            }
        }

        Loader::loadFileFree(acFileContentBuffer);

        loadAnimMeshDB();
    }

    wgpu::BufferDescriptor bufferDesc = {};

    std::string uniformBufferName = "animMeshModelUniforms";
    bufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
    bufferDesc.size = 256 * 256;            // 256 is the min binding alignment
    maBuffers[uniformBufferName] = mCreateInfo.mpDevice->CreateBuffer(&bufferDesc);
    maBuffers[uniformBufferName].SetLabel(uniformBufferName.c_str());
    maBufferSizes[uniformBufferName] = (uint32_t)bufferDesc.size;
    mCreateInfo.mpRenderer->registerBuffer(uniformBufferName, maBuffers[uniformBufferName]);

    uniformBufferName = "skinMeshUniformBuffer";
    bufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
    bufferDesc.size = 256;
    maBuffers[uniformBufferName] = mCreateInfo.mpDevice->CreateBuffer(&bufferDesc);
    maBuffers[uniformBufferName].SetLabel(uniformBufferName.c_str());
    maBufferSizes[uniformBufferName] = (uint32_t)bufferDesc.size;
    mCreateInfo.mpRenderer->registerBuffer(uniformBufferName, maBuffers[uniformBufferName]);

    // skin mesh uniform
    {
        std::vector<char> acSkinMeshUniformBuffer(256);
        char* pcData = acSkinMeshUniformBuffer.data();
        memset(pcData, 0, sizeof(char)* acSkinMeshUniformBuffer.size());
        uint32_t* piData = (uint32_t*)pcData;
        *piData = 2;
        piData += 4;
        pcData = (char *)piData;
        uint32_t iLastNumVertices = 0;
        for(uint32_t iMesh = 0; iMesh < 2; iMesh++)
        {
            piData = (uint32_t*)pcData;
            //*piData = (uint32_t)aTotalVertices.size() + iLastNumVertices;
            *piData = (uint32_t)maAnimMeshVertexRanges[0].miEnd + iLastNumVertices;
            iLastNumVertices = *piData;
            piData += 4;
            pcData = (char*)piData;
        }

        mCreateInfo.mpDevice->GetQueue().WriteBuffer(
            maBuffers[uniformBufferName],
            0,
            acSkinMeshUniformBuffer.data(),
            acSkinMeshUniformBuffer.size() * sizeof(char)
        );
    }

    maPlayerLocalPositions.resize(2);

    {
        uniformBufferName = "skinMeshVertexRanges";
        bufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
        bufferDesc.size = 256;
        maBuffers[uniformBufferName] = mCreateInfo.mpDevice->CreateBuffer(&bufferDesc);
        maBuffers[uniformBufferName].SetLabel(uniformBufferName.c_str());
        maBufferSizes[uniformBufferName] = (uint32_t)bufferDesc.size;
        mCreateInfo.mpRenderer->registerBuffer(uniformBufferName, maBuffers[uniformBufferName]);

        mCreateInfo.mpDevice->GetQueue().WriteBuffer(
            maBuffers[uniformBufferName],
            0,
            maAnimMeshVertexRanges.data(),
            maAnimMeshVertexRanges.size() * sizeof(MeshVertexRange)
        );

    }

}

/*
**
*/
void CApp::loadAnimation(
    std::string const& dir)
{
    std::vector<float4x4> aTotalInverseGlobalBindMatrices;

    uint32_t iAnimation = 0;
    for(auto& animNameInfo : maAnimationNameInfo)
    {
        std::string srcMatchingName = animNameInfo.mSrcAnimationName;
        auto iter = std::find_if(
            maAnimFileInfo.begin(),
            maAnimFileInfo.end(),
            [animNameInfo](auto const& animFileInfo)
            {
                auto iter = std::find_if(
                    animFileInfo.maMeshInstanceNames.begin(),
                    animFileInfo.maMeshInstanceNames.end(),
                    [animNameInfo](auto& meshInstanceName)
                    {
                        return meshInstanceName == animNameInfo.mDatabaseName;
                    }
                );
                
                return (iter != animFileInfo.maMeshInstanceNames.end());
            }
        );
        assert(iter != maAnimFileInfo.end());
        std::string const& baseFileName = iter->mFileName;
        auto end = baseFileName.rfind(".wad");
        std::string baseName = baseFileName.substr(0, end);
        animNameInfo.mBaseModel = baseName;

        // animation frame file
        std::string matchingAnimFrameFilePath = baseName + "-" + srcMatchingName + "-matching-animation-frames.anm";
        
        char* acFileContent = nullptr;
        Loader::loadFile(&acFileContent, matchingAnimFrameFilePath, true);
        uint32_t const* piData = (uint32_t const*)acFileContent;

        // animation frame data
        uint32_t iTotalAnimFrames = *piData++;
        maaAnimFrames.resize(iTotalAnimFrames);
        maaTotalAnimFrames[srcMatchingName].resize(iTotalAnimFrames);
        for(uint32_t i = 0; i < iTotalAnimFrames; i++)
        {
            uint32_t iNumMatchingJointFrame = *piData++;
            AnimFrame const* pAnimFrame = (AnimFrame const*)piData;

            maaAnimFrames[i].resize(iNumMatchingJointFrame);
            memcpy(maaAnimFrames[i].data(), pAnimFrame, sizeof(AnimFrame) * iNumMatchingJointFrame);

            maaTotalAnimFrames[srcMatchingName][i].resize(iNumMatchingJointFrame);
            memcpy(
                maaTotalAnimFrames[srcMatchingName][i].data(),
                pAnimFrame,
                sizeof(AnimFrame) * iNumMatchingJointFrame
            );

            pAnimFrame += iNumMatchingJointFrame;
            piData = (uint32_t const*)pAnimFrame;
        }

        float fAnimSpeedScale = 1.0f / animNameInfo.mfAnimSpeed;
        for(uint32_t i = 0; i < maaTotalAnimFrames[srcMatchingName].size(); i++)
        {
            for(uint32_t j = 0; j < maaTotalAnimFrames[srcMatchingName][i].size(); j++)
            {
                maaTotalAnimFrames[srcMatchingName][i][j].mfTime *= fAnimSpeedScale;
            }
        }

        Loader::loadFileFree(acFileContent);

        // joint local bind matrices
        std::string localBindMatrixFilePath = baseName + "-local-bind-matrices.bin";

        acFileContent = nullptr;
        Loader::loadFile(&acFileContent, localBindMatrixFilePath, true);
        piData = (uint32_t const*)acFileContent;

        uint32_t iNumJoints = *piData++;
        std::vector<float4x4> aLocalBindMatrices(iNumJoints);
        memcpy(
            aLocalBindMatrices.data(),
            piData,
            sizeof(float4x4) * iNumJoints);
        maaDstLocalBindMatrices.push_back(aLocalBindMatrices);

        // joint inverse global bind matrices
        std::string globalInverseBindFilePath = baseName + "-inverse-global-bind-matrices.bin";
        Loader::loadFileFree(acFileContent);
        acFileContent = nullptr;
        Loader::loadFile(&acFileContent, globalInverseBindFilePath, true);
        piData = (uint32_t const*)acFileContent;

        uint32_t iNumMatrices = *piData++;
        std::vector<float4x4> aInverseMatrices(iNumMatrices);
        memcpy(aInverseMatrices.data(), piData, sizeof(float4x4) * iNumMatrices);
        maaDstInverseGlobalBindMatrices.push_back(aInverseMatrices);

        aTotalInverseGlobalBindMatrices.insert(
            aTotalInverseGlobalBindMatrices.end(),
            aInverseMatrices.begin(),
            aInverseMatrices.end()
        );

        Loader::loadFileFree(acFileContent);

        // load textures 
        std::vector<Render::CRenderer::TextureAtlasInfo>& aTextureAtlasInfo = mCreateInfo.mpRenderer->getTextureAtlasInfo();
        uint32_t iLastEntry = (uint32_t)aTextureAtlasInfo.size();
        mCreateInfo.mpRenderer->loadTexturesIntoAtlas(
            baseName,
            "character-textures"
        );
        maAnimMeshTextureAtlasInfo.push_back(aTextureAtlasInfo.back());

        ++iAnimation;
    }

    // get the matching animation mesh to animation for getting the rig
    for(auto& animNameInfo : maAnimationNameInfo)
    {
        auto iter = std::find_if(
            maAnimFileInfo.begin(),
            maAnimFileInfo.end(),
            [animNameInfo](auto& animFileInfo)
            {
                auto end = animFileInfo.mFileName.find(".wad");
                std::string formatted = animFileInfo.mFileName.substr(0, end);
                return animNameInfo.mBaseModel == formatted;
            }
        );
        assert(iter != maAnimFileInfo.end());
        animNameInfo.miAnimMeshIndex = (uint32_t)std::distance(maAnimFileInfo.begin(), iter);
    }

    mLastTime = std::chrono::high_resolution_clock::now();
    mfTimeMilliSeconds = 0.0f;

    mafAnimTimeMilliSeconds["pitcher"] = 0.0f;
    mafAnimTimeMilliSeconds["batter"] = 0.0f;

    
    
}

/*
**
*/
void CApp::update()
{
    auto now = std::chrono::high_resolution_clock::now();
    float fElapsedMilliseconds = (float)(std::chrono::duration_cast<std::chrono::milliseconds>(now - mLastTime).count());
    mfTimeMilliSeconds += fElapsedMilliseconds;
    mLastTime = now;

    mPrevGameState = mGameState;

    // start simulating ball after released
    if(mGameState == GAME_STATE_PITCH_WINDUP && mafAnimTimeMilliSeconds["pitcher"] > 14500.0f / maAnimationNameInfo[0].mfAnimSpeed)
    {
        mGameState = GAME_STATE_PITCH_BALL_IN_FLIGHT;
        mfStartBallSimulationMilliSeconds = mfTimeMilliSeconds;
        mPitchSimulator.reset();

        Simulator::CPitchSimulator::Descriptor pitchSimulatorDesc = {};
        pitchSimulatorDesc.mInitialPosition = mBallPosition;
        pitchSimulatorDesc.mfInitialSpeed = 44.7f;
        pitchSimulatorDesc.mInitialVelocity.y -= 1.0f;
        pitchSimulatorDesc.mInitialVelocity.z = pitchSimulatorDesc.mfInitialSpeed * -1.0f;
        mPitchSimulator.setDesc(pitchSimulatorDesc);

        miNumTrailingBalls = 0;
    }
    else if(mGameState == GAME_STATE_PITCH_BALL_IN_FLIGHT)
    {
        if(mBallPosition.z <= -18.0f)
        {
            mGameState = GAME_STATE_HIT_BALL_IN_FLIGHT;

            float fExitSpeed;
            float fLaunchAngle;
            float3 exitSpinVector;
            float3 exitBallVelocity;
            float fBatSpeed = float(rand() % 30) + 30.0f;
            float fBatAttackAngleDegree = float((rand() % 120) - 60);
            float fBatHorizontalAngleDegree = float((rand() % 90) - 45);
            float fVerticalOffset = float((rand() % 100) - 50) * 0.01f;
            float fHorizontalOffset = float((rand() % 100) - 50) * 0.01f;
            
            //fBatSpeed = 20.0f;
            //fBatAttackAngleDegree = 20.0f;
            //fBatHorizontalAngleDegree = 10.0f;
            //fVerticalOffset = 0.0f;
            //fHorizontalOffset = 0.0f;

            mBattedBallSimulator.computeExitParams(
                fExitSpeed,
                fLaunchAngle,
                exitSpinVector,
                exitBallVelocity,
                fBatSpeed,
                fBatAttackAngleDegree,
                fBatHorizontalAngleDegree,
                fVerticalOffset,
                fHorizontalOffset
            );

            Simulator::CBattedBallSimulator::Descriptor battedBallDesc = {};
            battedBallDesc.mInitialPosition = mBallPosition;
            battedBallDesc.mInitialVelocity = exitBallVelocity;
            battedBallDesc.mSpinAxis = normalize(exitSpinVector);
            battedBallDesc.mfSpinRPM = length(exitSpinVector) * (3.14159f / 180.0f);
            mBattedBallSimulator.setDesc(battedBallDesc);
            mBattedBallSimulator.reset();
        }

        miNumTrailingBalls = 0;
    }
    else if(mGameState == GAME_STATE_HIT_BALL_IN_FLIGHT)
    {
        if(miNumTrailingBalls < 128 && mCreateInfo.mpRenderer->getFrameIndex() % 2 == 0)
        {
            maBallTrailPositions[miNumTrailingBalls] = mBallPosition;
            ++miNumTrailingBalls;
        }
    }

    updateBall(fElapsedMilliseconds);
    updateAnimations(fElapsedMilliseconds);
    updateMeshModelTransforms();

    ShadowUniformData shadowUniformBuffer = {};
    updateLightViewMatrices(
        shadowUniformBuffer,
        gCamera);

    // decal
    float4x4 decalViewProjectionMatrix;
    {
        float fDistance = length(float3(mBallPosition.x, 0.0f, mBallPosition.z) - float3(0.0f, 0.0f, 18.44f));
        float fDecalSizePct = maxf(minf(mBallPosition.y / 2.0f, 3.0f), 1.0f); // (mGameState != GAME_STATE_HIT_BALL_IN_FLIGHT || mBallPosition.y <= 0.1f) ? 0.0f : 1.0f;
        float fDecalDepthScale = (fDistance <= 110.0f) ? fDecalSizePct : 1000.0f;

        if(mGameState != GAME_STATE_HIT_BALL_IN_FLIGHT)
        {
            fDecalSizePct = 0.0f;
        }
        
        float4x4 decalViewMatrix = makeViewMatrix(float3(mBallPosition.x, 0.1f, mBallPosition.z), float3(mBallPosition.x, 0.0f, mBallPosition.z), float3(0.0f, 0.0f, 1.0f));
        float4x4 decalProjectionMatrix = orthographicProjection(
            -1.0f * fDecalSizePct,
            1.0f * fDecalSizePct,
            1.0f * fDecalSizePct,
            -1.0f * fDecalSizePct,
            0.1f * fDecalDepthScale,
            -0.1f * fDecalDepthScale);
        decalViewProjectionMatrix = decalProjectionMatrix * decalViewMatrix;
    }
    shadowUniformBuffer.mDecalViewProjectionMatrix = decalViewProjectionMatrix;

    // upload data
    mCreateInfo.mpDevice->GetQueue().WriteBuffer(
        maBuffers["shadowUniformBuffer"],
        0,
        &shadowUniformBuffer,
        sizeof(ShadowUniformData)
    );

}

/*
**
*/
uint32_t getJointArrayIndex(
    std::string const& name,
    std::map<uint32_t, std::string> const& aJointMapping,
    std::vector<uint32_t> const& aiJointToArrayMapping)
{
    auto iter = std::find_if(
        aJointMapping.begin(),
        aJointMapping.end(),
        [name](auto& keyValue)
        {
            return keyValue.second == name;
        }
    );

    uint32_t iArrayIndex = aiJointToArrayMapping[iter->first];
    return iArrayIndex;

}

/*
**
*/
void CApp::updateAnimations(float fElapsedMilliseconds)
{
    uint32_t iAnimNameInfo = 0;
    for(auto const& animNameInfo : maAnimationNameInfo)
    {
        uint32_t iRigIndex = animNameInfo.miAnimMeshIndex;
        uint32_t iNumJoints = (uint32_t)maaJoints[iRigIndex].size();

        std::vector<AnimFrameInfo> aAnimFrameInfo;
        std::vector<float4x4> aLocalAnimMatrices(iNumJoints);
        traverseJoint(
            aAnimFrameInfo,
            aLocalAnimMatrices,
            maaTotalAnimFrames[animNameInfo.mSrcAnimationName],
            maaJoints[iRigIndex],
            maaDstLocalBindMatrices[iRigIndex],
            maaDstInverseGlobalBindMatrices[iRigIndex],
            maaiJointToArrayMapping[iRigIndex],
            maaJointMapping[iRigIndex],
            maaJoints[iRigIndex][0],
            rotateMatrixY(3.14159f * -0.5f) * scale(-1.0f, 1.0f, 1.0f),
            mafAnimTimeMilliSeconds[animNameInfo.mDatabaseName] * 0.001f,
            0
        );
        maaCurrAnimFrameInfo[animNameInfo.mSrcAnimationName] = aAnimFrameInfo;
        maaCurrLocalAnimMatrices[animNameInfo.mSrcAnimationName] = aLocalAnimMatrices;

        // get the total matrices
        //uint32_t iArrayIndexOffset = (uint32_t)maiJointMatrixStartIndices[iRigIndex];
        uint32_t iArrayIndexOffset = maMatchVertexRangeToMeshInstance[iAnimNameInfo].miMatrixStartIndex;
        for(uint32_t i = 0; i < (uint32_t)maaCurrAnimFrameInfo[animNameInfo.mSrcAnimationName].size(); i++)
        {
            uint32_t iArrayIndex = maaiJointToArrayMapping[iRigIndex][aAnimFrameInfo[i].miJoint];
            iArrayIndex += iArrayIndexOffset;
            assert(iArrayIndex < maTotalGlobalAnimationMatrices.size());
            maTotalGlobalAnimationMatrices[iArrayIndex] = maaCurrAnimFrameInfo[animNameInfo.mSrcAnimationName][i].mTotalAnimWithInverseBindMatrix;
        }

        ++iAnimNameInfo;
    }

    // update gpu total matrix buffer
    std::string bufferName = "total-joint-global-animation-matrices";
    wgpu::Buffer& jointAnimTotalMatrixBuffer = mCreateInfo.mpRenderer->getBuffer(bufferName);
    mCreateInfo.mpDevice->GetQueue().WriteBuffer(
        jointAnimTotalMatrixBuffer,
        0,
        maTotalGlobalAnimationMatrices.data(),
        maTotalGlobalAnimationMatrices.size() * sizeof(float4x4));

    float fLastPitcherTime = maaTotalAnimFrames[maAnimationNameInfo[0].mSrcAnimationName][maaTotalAnimFrames[maAnimationNameInfo[0].mSrcAnimationName].size() - 1][0].mfTime;

    // update animation time
    mafAnimTimeMilliSeconds["pitcher"] += fElapsedMilliseconds;
    mafAnimTimeMilliSeconds["batter"] += fElapsedMilliseconds;
    //if(mBallPosition.y <= -0.0f && mafAnimTimeMilliSeconds["pitcher"] >= fLastPitcherTime * 1000.0f)
    float fDistance = length(float3(0.0f, 0.0f, -18.44f) - mBallPosition);
    if(mGameState == GAME_STATE_HIT_BALL_IN_FLIGHT && (mBattedBallSimulator.hasStopped() || mBattedBallSimulator.getNumBounces() >= 100 || fDistance >= 150.0f))
    {
        mafAnimTimeMilliSeconds["pitcher"] = 0.0f;
        mGameState = GAME_STATE_PITCH_WINDUP;

        mafAnimTimeMilliSeconds["batter"] = 1300.0f;
    }

    for(uint32_t i = 0; i < maAnimationNameInfo.size(); i++)
    {
        AnimMeshModelUniform uniformBuffer;
        uniformBuffer.mModelMatrix = translate(0.8f * float(i), 0.0f, -18.4404f * float(i));
        uniformBuffer.mLightViewProjectionMatrix = maLightViewCameras[0].getViewProjectionMatrix();
        uniformBuffer.mTextureAtlasUV = float4(
            maAnimMeshTextureAtlasInfo[i].mUV.x,
            maAnimMeshTextureAtlasInfo[i].mUV.y,
            (float)maAnimMeshTextureAtlasInfo[i].miImageWidth,
            (float)maAnimMeshTextureAtlasInfo[i].miImageHeight
        );
        uniformBuffer.mExtraInfo = float4(float(i), 0.0f, 0.0f, 0.0f);

        std::string animMeshModelUniformBufferName = "animMeshModelUniforms";
        wgpu::Buffer& animMeshModelUniformBuffer = mCreateInfo.mpRenderer->getBuffer(animMeshModelUniformBufferName);
        mCreateInfo.mpDevice->GetQueue().WriteBuffer(
            animMeshModelUniformBuffer,
            i * 256,
            &uniformBuffer,
            sizeof(AnimMeshModelUniform)
        );
    }
}

/*
**
*/
void CApp::updateMeshModelTransforms()
{
    float3 aPositions[] = {
        float3(0.0f, 0.2f, 0.0f),
        float3(-0.8f, -0.1f, -18.4404f)
    };
    for(uint32_t i = 0; i < maAnimationNameInfo.size(); i++)
    {
        AnimMeshModelUniform uniformBuffer;
        uniformBuffer.mModelMatrix = translate(aPositions[i].x, aPositions[i].y, aPositions[i].z);
        uniformBuffer.mLightViewProjectionMatrix = maLightViewCameras[0].getViewProjectionMatrix();
        uniformBuffer.mTextureAtlasUV = float4(
            maAnimMeshTextureAtlasInfo[i].mUV.x,
            maAnimMeshTextureAtlasInfo[i].mUV.y,
            (float)maAnimMeshTextureAtlasInfo[i].miImageWidth,
            (float)maAnimMeshTextureAtlasInfo[i].miImageHeight
        );
        uniformBuffer.mExtraInfo = float4(float(i), 0.0f, 0.0f, 0.0f);

        std::string animMeshModelUniformBufferName = "animMeshModelUniforms";
        wgpu::Buffer& animMeshModelUniformBuffer = mCreateInfo.mpRenderer->getBuffer(animMeshModelUniformBufferName);
        mCreateInfo.mpDevice->GetQueue().WriteBuffer(
            animMeshModelUniformBuffer,
            i * 256,
            &uniformBuffer,
            sizeof(AnimMeshModelUniform)
        );

        float4x4 localBindMatrix, parentTotalMatrix, localAnimMatrix;
        getJointMatrices(
            localBindMatrix,
            parentTotalMatrix,
            localAnimMatrix,
            "mixamorig:Hips",
            maAnimationNameInfo[i].mSrcAnimationName,
            maAnimationNameInfo[i].miAnimMeshIndex
        );
        float4x4 jointTotalMatrix = parentTotalMatrix * localBindMatrix * localAnimMatrix;
        maPlayerLocalPositions[i] = float3(
            jointTotalMatrix.mafEntries[3] * 10.0f,
            jointTotalMatrix.mafEntries[7] * 10.0f,
            jointTotalMatrix.mafEntries[11] * 10.0f
        );

    }

    // ball position while in windup, get the position of the throwing hand
    if(mGameState == GAME_STATE_PITCH_WINDUP)
    {
        float4x4 localBindMatrix, parentTotalMatrix, localAnimMatrix;
        getJointMatrices(
            localBindMatrix,
            parentTotalMatrix,
            localAnimMatrix,
            "mixamorig:LeftHand",
            maAnimationNameInfo[0].mSrcAnimationName,
            maAnimationNameInfo[0].miAnimMeshIndex
        );
        localBindMatrix.mafEntries[11] += 0.02f;
        float4x4 jointTotalMatrix = parentTotalMatrix * localBindMatrix * localAnimMatrix;
        mBallPosition = float3(
            jointTotalMatrix.mafEntries[3] * 10.0f,
            jointTotalMatrix.mafEntries[7] * 10.0f,
            jointTotalMatrix.mafEntries[11] * 10.0f
        );
        mBallPosition = mBallPosition + aPositions[0];
    }

    // bat position
    {
        uint32_t iPrevFrameIndex = UINT32_MAX;
        uint32_t iCurrFrameIndex = UINT32_MAX;
        float fCurrTime = 0.0f, fPrevTime = 0.0f;
        float fAnimTime = mafAnimTimeMilliSeconds["batter"] * 0.001f;
        for(uint32_t iFrame = 0; iFrame < (uint32_t)mafBatGlobalFrameTimes.size(); iFrame++)
        {
            if(mafBatGlobalFrameTimes[iFrame] > fAnimTime)
            {
                iCurrFrameIndex = iFrame;
                iPrevFrameIndex = (iCurrFrameIndex > 0) ? iCurrFrameIndex - 1 : 0;
                fPrevTime = mafBatGlobalFrameTimes[iPrevFrameIndex];
                break;
            }
        }

        float fPct = 1.0f;
        if(iCurrFrameIndex == UINT32_MAX)
        {
            iCurrFrameIndex = (uint32_t)mafBatGlobalFrameTimes.size() - 1;
            iPrevFrameIndex = iCurrFrameIndex;
        }
        else
        {

            float fDuration = mafBatGlobalFrameTimes[iCurrFrameIndex] - mafBatGlobalFrameTimes[iPrevFrameIndex];
            fPct = 1.0f - ((mafBatGlobalFrameTimes[iCurrFrameIndex] - fAnimTime) / fDuration);
        }

        quaternion q0, q1;
        q0 = q0.fromMatrix(maBatGlobalTransforms[iPrevFrameIndex]);
        q1 = q1.fromMatrix(maBatGlobalTransforms[iCurrFrameIndex]);
        quaternion q = quaternion::slerp(q0, q1, fPct);
        float4x4 batMatrix = q.matrix();

        float4x4 localBindMatrix, parentTotalMatrix, localAnimMatrix;
        getJointMatrices(
            localBindMatrix,
            parentTotalMatrix,
            localAnimMatrix,
            "mixamorig:LeftHand",
            maAnimationNameInfo[1].mSrcAnimationName,
            maAnimationNameInfo[1].miAnimMeshIndex
        );
        float3 matrixScaling = extractScale(parentTotalMatrix);
        float4x4 scaledParentTotalMatrix = parentTotalMatrix * scale(1.0f / matrixScaling.x, 1.0f / matrixScaling.y, 1.0f / matrixScaling.z);
        float4x4 jointTotalMatrix = parentTotalMatrix * localBindMatrix * localAnimMatrix;
        mBatPosition = float3(
            jointTotalMatrix.mafEntries[3] * 10.0f,
            jointTotalMatrix.mafEntries[7] * 10.0f,
            jointTotalMatrix.mafEntries[11] * 10.0f
        );
        mBatPosition = mBatPosition + aPositions[1];

        float4x4 jointTotalMatrixScaled = scaledParentTotalMatrix * localBindMatrix * localAnimMatrix;
        mBatAxisAngle = extractAxisAngle(jointTotalMatrixScaled);

        float4x4 batRotationMatrix = rotateMatrixZ(3.14159f);
        float4x4 totalMatrix = scale(1.0f / matrixScaling.x, 1.0f / matrixScaling.y, 1.0f / matrixScaling.z) * (parentTotalMatrix * localBindMatrix * localAnimMatrix * batRotationMatrix);
        
        
        float4x4 rotationMatrix = rotateMatrixZ(3.14159f * 0.5f);
        batMatrix.mafEntries[3] = jointTotalMatrix.mafEntries[3] * 10.0f;
        batMatrix.mafEntries[7] = jointTotalMatrix.mafEntries[7] * 10.0f;
        batMatrix.mafEntries[11] = jointTotalMatrix.mafEntries[11] * 10.0f;
        totalMatrix = batMatrix * rotationMatrix;
        
        mBatMatrix = translate(aPositions[1].x, aPositions[1].y, aPositions[1].z) * totalMatrix;
    }

    uint32_t iArrayIndex = maMeshModelInfoDB["ball"];
    ObjectInfo const& objectInfo = maMeshModelInfo[iArrayIndex];
    float4x4& meshModelMatrix = maStaticMeshModelMatrices[objectInfo.miStaticIndex];
    float4x4 ballRotationMatrix = makeFromAngleAxis(float3(mBallAxisAngle.x, mBallAxisAngle.y, mBallAxisAngle.z), mBallAxisAngle.w);
    meshModelMatrix = translate(mBallPosition.x, mBallPosition.y, mBallPosition.z) * ballRotationMatrix;

    iArrayIndex = maMeshModelInfoDB["bat"];
    ObjectInfo const& batObjectInfo = maMeshModelInfo[iArrayIndex];
    float4x4& batMeshModelMatrix = maStaticMeshModelMatrices[batObjectInfo.miStaticIndex];
    batMeshModelMatrix = mBatMatrix;

    for(uint32_t i = 0; i < 128 - 6; i++)
    {
        if(i < miNumTrailingBalls)
        {
            //maStaticMeshModelMatrices[i + 6] = translate(maBallTrailPositions[i].x, maBallTrailPositions[i].y, maBallTrailPositions[i].z);
            maStaticMeshModelMatrices[i + 6].mafEntries[3] = maBallTrailPositions[i].x;
            maStaticMeshModelMatrices[i + 6].mafEntries[7] = maBallTrailPositions[i].y;
            maStaticMeshModelMatrices[i + 6].mafEntries[11] = maBallTrailPositions[i].z;
        }
        else
        {
            //maStaticMeshModelMatrices[i + 6] = translate(0.0f, 0.0f, -99999.0f);
            maStaticMeshModelMatrices[i + 6].mafEntries[3] = 0.0f;
            maStaticMeshModelMatrices[i + 6].mafEntries[7] = 0.0f;
            maStaticMeshModelMatrices[i + 6].mafEntries[11] = -99999.0f;
        }
    }

    mCreateInfo.mpDevice->GetQueue().WriteBuffer(
        maBuffers["staticMeshModelMatrices"],
        0,
        maStaticMeshModelMatrices.data(),
        maStaticMeshModelMatrices.size() * sizeof(float4x4)
    );

    uint32_t aiUniformBufferData[] = {(uint32_t)maStaticMeshModelMatrices.size(), 0};
    mCreateInfo.mpDevice->GetQueue().WriteBuffer(
        maBuffers["meshCullingUniformBuffer"],
        0,
        aiUniformBufferData,
        sizeof(aiUniformBufferData)
    );
    mCreateInfo.mpRenderer->registerBuffer("meshCullingUniformBuffer", maBuffers["meshCullingUniformBuffer"]);

    struct LightInfo
    {
        float4      mPosition;
        float4      mRadiance;
    };
    
    std::vector<LightInfo> aLightInfo(5);
    aLightInfo[0].mPosition = float4(0.0f, 80.0f, 50.0f, 1.0f);
    aLightInfo[1].mPosition = float4(maPlayerLocalPositions[0] + aPositions[0] + float3(0.0f, 2.0f, -2.0f), 1.0f);
    aLightInfo[2].mPosition = float4(maPlayerLocalPositions[0] + aPositions[0] + float3(0.0f, 2.5f, 2.0f), 1.0f);
    aLightInfo[3].mPosition = float4(maPlayerLocalPositions[1] + aPositions[1] + float3(0.0f, 2.5f, 2.0f), 1.0f);
    aLightInfo[4].mPosition = float4(maPlayerLocalPositions[1] + aPositions[1] + float3(0.0f, 2.0f, -2.0f), 1.0f);

    float4 lightRadiance = float4(10.0f, 10.0f, 10.0f, 10.0f);

    aLightInfo[0].mRadiance = float4(20000.0f, 20000.0f, 20000.0f, 1.0f);
    aLightInfo[1].mRadiance = lightRadiance;
    aLightInfo[2].mRadiance = lightRadiance;
    aLightInfo[3].mRadiance = lightRadiance;
    aLightInfo[4].mRadiance = lightRadiance;

    mCreateInfo.mpDevice->GetQueue().WriteBuffer(
        maBuffers["lightingUniformBuffer"],
        0,
        aLightInfo.data(),
        aLightInfo.size() * sizeof(LightInfo)
    ); 

}

/*
**
*/
void CApp::updateBall(float fMillisecondElapsed)
{
    if(mGameState == GAME_STATE_PITCH_BALL_IN_FLIGHT)
    {
        mPitchSimulator.simulate(fMillisecondElapsed * 0.001f);
        //mPitchSimulator.simulate(0.0001f);
        mBallPosition = mPitchSimulator.getPosition();
        mBallAxisAngle = mPitchSimulator.getAxisAngle();
        //DEBUG_PRINTF("time: %.4f ball position (%.4f, %.4f, %.4f)\n", 
        //    mPitchSimulator.getTime(),
        //    mBallPosition.x, 
        //    mBallPosition.y, 
        //    mBallPosition.z);
    }
    else if(mGameState == GAME_STATE_HIT_BALL_IN_FLIGHT)
    {
        mBattedBallSimulator.simulate(fMillisecondElapsed * 0.001f);
        mBallPosition = mBattedBallSimulator.getPosition();
        mBallAxisAngle = mBattedBallSimulator.getAxisAngle();
        float fDistance = length(float3(mBallPosition.x, 0.0f, mBallPosition.z) - float3(0.0f, 0.0f, -18.44f));
        //DEBUG_PRINTF("time: %.4f ball position (%.4f, %.4f, %.4f) distance: %.4f\n",
        //    mPitchSimulator.getTime(),
        //    mBallPosition.x,
        //    mBallPosition.y,
        //    mBallPosition.z,
        //    fDistance);
    }
}

/*
**
*/
void CApp::traverseJoint(
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
    uint32_t iStack)
{
    // get the frame time interval
    bool bFound = false;
    float fPrevFrameTime = 0.0f, fCurrFrameTime = 0.0f;
    uint32_t iPrevFrame = 0, iCurrFrame = 0;
    for(uint32_t iFrame = 0; iFrame < (uint32_t)aaAnimFrames.size(); iFrame++)
    {
        if(aaAnimFrames[iFrame][0].mfTime > fTime)
        {
            iCurrFrame = iFrame;
            fCurrFrameTime = aaAnimFrames[iFrame][0].mfTime;
            if(iFrame > 0)
            {
                iPrevFrame = iFrame - 1;
                fPrevFrameTime = aaAnimFrames[iPrevFrame][0].mfTime;
            }

            bFound = true;
            break;
        }
    }

    // clamp at the end
    if(!bFound)
    {
        iCurrFrame = (uint32_t)aaAnimFrames.size() - 1;
        iPrevFrame = iCurrFrame - 1;
        fCurrFrameTime = aaAnimFrames[iCurrFrame][0].mfTime;
        fPrevFrameTime = aaAnimFrames[iPrevFrame][0].mfTime;

        fTime = fCurrFrameTime;
    }

    assert(fCurrFrameTime >= fPrevFrameTime);

    // interpolation percentage
    float fPct = (fCurrFrameTime > 0.0f) ?
        (fTime - fPrevFrameTime) / (fCurrFrameTime - fPrevFrameTime) :
        0.0f;

    // see if this joint has animation frame at this time
    uint32_t iIndex = UINT32_MAX;
    for(uint32_t i = 0; i < (uint32_t)aaAnimFrames[iCurrFrame].size(); i++)
    {
        if(aaAnimFrames[iCurrFrame][i].miNodeIndex == joint.miIndex)
        {
            iIndex = i;
            break;
        }
    }

    // interpolate rotation, translation, and scale 
    float4 animRotation, animTranslation;
    float4 animScale = float4(1.0f, 1.0f, 1.0f, 1.0f);
    float4x4 rotationMatrix, translationMatrix;
    if(iIndex != UINT32_MAX)
    {
        AnimFrame const& prevFrame = aaAnimFrames[iPrevFrame][iIndex];
        AnimFrame const& currFrame = aaAnimFrames[iCurrFrame][iIndex];

        animRotation =
            prevFrame.mRotation +
            (currFrame.mRotation - prevFrame.mRotation) * fPct;

        animRotation.w =
            prevFrame.mRotation.w +
            (currFrame.mRotation.w - prevFrame.mRotation.w) * fPct;

        animTranslation =
            prevFrame.mTranslation +
            (currFrame.mTranslation - prevFrame.mTranslation) * fPct;

        animScale =
            prevFrame.mScaling +
            (currFrame.mScaling - prevFrame.mScaling) * fPct;

        rotationMatrix = makeFromAngleAxis(float3(animRotation), animRotation.w);
        translationMatrix = translate(animTranslation.x, animTranslation.y, animTranslation.z);
    }

    uint32_t iJointArrayIndex = aiJointArrayIndexMapping[joint.miIndex];
    float4x4 const& localBindMatrix = aLocalBindMatrices[iJointArrayIndex];
    float4x4 const& globalInverseBindMatrix = aGlobalInverseBindMatrices[iJointArrayIndex];

    // total matrix: parent * local bind * translation * rotation * scale
    //float4x4 translationMatrix = translate(animTranslation.x, animTranslation.y, animTranslation.z);
    //float4x4 scaleMatrix = scale(animScale.x, animScale.y, animScale.z);
    float4x4 animMatrix = translationMatrix * rotationMatrix; // translationMatrix * rotationMatrix * scaleMatrix;
    float4x4 localAnimMatrix = localBindMatrix * animMatrix;

    float4x4 totalAnimMatrix = parentMatrix * localBindMatrix * animMatrix;
    float4x4 totalAnimWithInverseBindMatrix = totalAnimMatrix * globalInverseBindMatrix;

    aAnimMatrices[iJointArrayIndex] = animMatrix;

    // FOR verifying correct global bind matrix
    //float4x4 currGlobalBindMatrix = parentMatrix * localBindMatrix;
    //float4x4 verifyMatrix = currGlobalBindMatrix * globalInverseBindMatrix;
    //assert(verifyMatrix.identical(float4x4(), 1.0e-4f));

    // save out animation frame info for this joint
    AnimFrameInfo animFrameInfo;
    animFrameInfo.miJoint = joint.miIndex;
    animFrameInfo.mTotalAnimMatrix = totalAnimMatrix;
    animFrameInfo.mTotalAnimWithInverseBindMatrix = totalAnimWithInverseBindMatrix;
    aAnimFrames.push_back(animFrameInfo);

#if 0
    std::string jointName = aJointMapping[joint.miIndex];
    DEBUG_PRINTF("draw_sphere([%.4f, %.4f, %.4f], 0.01, 255, 0, 0, 255, \"%s\")\n",
        totalAnimMatrix.mafEntries[3],
        totalAnimMatrix.mafEntries[7],
        totalAnimMatrix.mafEntries[11],
        jointName.c_str()
    );
#endif // #if 0

    // traverse into children
    for(uint32_t iChild = 0; iChild < joint.miNumChildren; iChild++)
    {
        uint32_t iChildJointIndex = joint.maiChildren[iChild];
        uint32_t iChildArrayIndex = aiJointArrayIndexMapping[iChildJointIndex];
        assert(aJoints[iChildArrayIndex].miIndex == iChildJointIndex);

        traverseJoint(
            aAnimFrames,
            aAnimMatrices,
            aaAnimFrames,
            aJoints,
            aLocalBindMatrices,
            aGlobalInverseBindMatrices,
            aiJointArrayIndexMapping,
            aJointMapping,
            aJoints[iChildArrayIndex],
            totalAnimMatrix,
            fTime,
            iStack + 1
        );
    }
}

/*
**
*/
void CApp::verify0(
    std::vector<Joint> const& aJoints,
    std::vector<float4x4> const& aLocalBindMatrices,
    std::vector<float4x4> const& aGlobalInverseBindMatrices,
    std::vector<uint32_t> const& aiJointToArrayMapping,
    std::map<uint32_t, std::string> const& aJointMapping)
{
    assert(aJoints.size() == maaJoints[0].size());
    assert(aLocalBindMatrices.size() == maaLocalBindMatrices[0].size());
    assert(aGlobalInverseBindMatrices.size() == maaGlobalInverseBindMatrices[0].size());
    assert(aiJointToArrayMapping.size() == maaiJointToArrayMapping[0].size());
    assert(aJointMapping.size() == maaJointMapping[0].size());

    for(uint32_t i = 0; i < aJoints.size(); i++)
    {
        assert(aJoints[i] == maaJoints[0][i]);
    }

    for(uint32_t i = 0; i < aLocalBindMatrices.size(); i++)
    {
        assert(aLocalBindMatrices[i] == maaLocalBindMatrices[0][i]);
    }

    for(uint32_t i = 0; i < aLocalBindMatrices.size(); i++)
    {
        assert(aGlobalInverseBindMatrices[i] == maaGlobalInverseBindMatrices[0][i]);
    }

    for(uint32_t i = 0; i < aLocalBindMatrices.size(); i++)
    {
        assert(aiJointToArrayMapping[i] == maaiJointToArrayMapping[0][i]);
    }

    for(uint32_t i = 0; i < aJointMapping.size(); i++)
    {
        assert(aiJointToArrayMapping[i] == maaiJointToArrayMapping[0][i]);
    }
}

/*
**
*/
void CApp::verify1(
    std::vector<std::vector<CApp::AnimFrame>>& aaAnimFrames,
    std::vector<float4x4> const& aLocalBindMatrices,
    std::vector<std::vector<float4x4>> const& aaDstInverseGlobalBindMatrices)
{
    assert(aaAnimFrames.size() == maaAnimFrames.size());
    for(uint32_t i = 0; i < aaAnimFrames.size(); i++)
    {
        assert(aaAnimFrames[i].size() == maaAnimFrames[i].size());
        for(uint32_t j = 0; j < aaAnimFrames[i].size(); j++)
        {
            CApp::AnimFrame& check0 = aaAnimFrames[i][j];
            CApp::AnimFrame& check1 = maaAnimFrames[i][j];

            assert(check0 == check1);
        }
    }

    assert(aLocalBindMatrices.size() == maaDstLocalBindMatrices[0].size());
    for(uint32_t i = 0; i < aLocalBindMatrices.size(); i++)
    {
        assert(aLocalBindMatrices[i] == maaDstLocalBindMatrices[0][i]);
    }

    assert(aaDstInverseGlobalBindMatrices.size() == maaDstInverseGlobalBindMatrices.size());
    for(uint32_t i = 0; i < aaDstInverseGlobalBindMatrices.size(); i++)
    {
        assert(aaDstInverseGlobalBindMatrices[i].size() == maaDstInverseGlobalBindMatrices[i].size());
        for(uint32_t j = 0; j < aaDstInverseGlobalBindMatrices[i].size(); j++)
        {
            assert(aaDstInverseGlobalBindMatrices[i][j] == maaDstInverseGlobalBindMatrices[i][j]);
        }
    }
}

/*
**
*/
void CApp::getJointMatrices(
    float4x4& localBindMatrix,
    float4x4& parentTotalMatrix,
    float4x4& animMatrix,
    std::string const& jointName,
    std::string const& srcAnimName,
    uint32_t iAnimationIndex)
{
    auto iter = std::find_if(
        maaJointMapping[iAnimationIndex].begin(),
        maaJointMapping[iAnimationIndex].end(),
        [jointName](auto& keyValue)
        {
            return keyValue.second == jointName;
        }
    );
    assert(iter != maaJointMapping[iAnimationIndex].end());

    // get joint index and parent array index
    uint32_t iJointIndex = (uint32_t)std::distance(maaJointMapping[iAnimationIndex].begin(), iter);
    Joint const& joint = maaJoints[iAnimationIndex][iJointIndex];
    uint32_t iJointArrayIndex = maaiJointToArrayMapping[iAnimationIndex][joint.miIndex];
    uint32_t iParentArrayIndex = maaiJointToArrayMapping[iAnimationIndex][joint.miParent];

    animMatrix = maaCurrLocalAnimMatrices[srcAnimName][iJointArrayIndex];
    parentTotalMatrix = maaCurrAnimFrameInfo[srcAnimName][iParentArrayIndex].mTotalAnimMatrix;
    localBindMatrix = maaDstLocalBindMatrices[iAnimationIndex][iJointArrayIndex];
}

/*
**
*/
void CApp::updateLightViewMatrices(
    ShadowUniformData& shadowUniformBuffer,
    CCamera const& camera)
{
    //float fFarMinusNear = camera.getFar() - camera.getNear();
    float fFarMinusNear = 100.0f - 1.0f;
    float fCameraFieldOfView = camera.getFieldOfView();
    float2 afFrustumPct[] =
    {
        float2(0.08f, 0.125f),
        float2(0.125f, 0.3f),
        float2(0.3f, 1.75f),
    };

    float3 cameraPosition = camera.getPosition();
    float3 cameraLookAt = camera.getLookAt();
    float3 cameraLookDir = normalize(cameraLookAt - cameraPosition);
    
    float3 lightDirection = normalize(float3(0.3f, 1.0f, 0.0f));
    float3 up = (fabsf(lightDirection.y) >= 0.99f) ? float3(1.0f, 0.0f, 0.0f) : float3(0.0f, 1.0f, 0.0f);

    uint32_t iNumCascades = (uint32_t)(sizeof(afFrustumPct) / sizeof(*afFrustumPct));
    for(uint32_t i = 0; i < iNumCascades; i++)
    {
        float fNearDistance = afFrustumPct[i].x * fFarMinusNear;
        float fFarDistance = afFrustumPct[i].y * fFarMinusNear;
        float fSizeZ = fFarDistance - fNearDistance;

        float fFrustumWidth = tanf(fCameraFieldOfView * (3.14159f / 180.0f)) * fFarDistance * 2.0f;
        float fFrustumHeight = fFarDistance - fNearDistance;
        float fMaxSize = maxf(fFrustumWidth, maxf(fFrustumHeight, fSizeZ));
        float fHalfMaxSize = fMaxSize * 0.5f;

        float3 frustumCascadeNear = cameraPosition + cameraLookDir * fNearDistance;
        float3 frustumCascadeFar = cameraPosition + cameraLookDir * fFarDistance;
        float3 frustumCascadeCenter = (frustumCascadeFar + frustumCascadeNear) * 0.5f;

        float3 eyePosition = frustumCascadeCenter + lightDirection * fHalfMaxSize;
        float3 lookAt = frustumCascadeCenter;

        float4x4 viewMatrix = makeViewMatrix(
            eyePosition,
            lookAt,
            up
        );
        float4x4 projectionMatrix = orthographicProjection(
            -fHalfMaxSize,
            fHalfMaxSize,
            fHalfMaxSize,
            -fHalfMaxSize,
            fMaxSize,
            -fMaxSize
        );
        //float4x4 projectionMatrix = orthographicProjection(
        //    -5.0f,
        //    5.0f,
        //    5.0f,
        //    -5.0f,
        //    10.0f,
        //    -10.0f
        //);

        float4x4 viewProjectionMatrix = projectionMatrix * viewMatrix;
        shadowUniformBuffer.maLightViewProjectionMatrices[i] = viewProjectionMatrix;

        //DEBUG_PRINTF("light view %d eye (%.4f, %.4f, %.4f) lookAt (%.4f, %.4f, %.4f) max size: %.4f\n",
        //    i,
        //    eyePosition.x, eyePosition.y, eyePosition.z,
        //    lookAt.x, lookAt.y, lookAt.z,
        //    fMaxSize
        //);
        //int iDebug = 1;
    }
}



/*
**
*/
void CApp::loadAnimMeshDB()
{
    struct OutputMaterialInfo
    {
        float4              mDiffuse;
        float4              mRoughnessMetallic;
        float4              mEmissive;
        uint32_t            miAlbedoTextureID = UINT32_MAX;
        uint32_t            miNormalTextureID = UINT32_MAX;
        uint32_t            miEmissiveTextureID = UINT32_MAX;
        uint32_t            miRoughnessMetallicTextureID = UINT32_MAX;
    };

    maaGlobalInverseBindMatrices.resize(maAnimFileInfo.size());

    uint32_t iLoadAnimMesh = 0;
    std::vector<Vertex> aTotalVertexBuffer;
    std::vector<uint32_t> aTotalIndexBuffer;
    std::vector<float> afTotalJointInfluenceWeights;
    std::vector<uint32_t> aiTotalJointInfluenceIndices;
    std::vector<float4x4> aTotalGlobalBindMatrices;
    std::vector<float4x4> aTotalInverseGlobalBindMatrices;
    std::vector<OutputMaterialInfo> aTotalMaterialInfo;
    std::vector<uint32_t> aiTotalMaterialID;
    for(auto& animFileInfo : maAnimFileInfo)
    {
        std::string baseName = animFileInfo.mFileName;
        auto baseNameEnd = baseName.rfind(".wad");
        assert(baseNameEnd != std::string::npos);
        baseName = baseName.substr(0, baseNameEnd);

        char* acFileContentBuffer = nullptr;
        Loader::loadFile(&acFileContentBuffer, animFileInfo.mFileName, true);
        uint32_t const* piData = (uint32_t const*)acFileContentBuffer;

        uint32_t iNumMeshes = *piData++;
        uint32_t iNumAnimations = *piData++;
        assert(iNumAnimations == 1);

        DEBUG_PRINTF("num meshes: %d\n", iNumMeshes);
        DEBUG_PRINTF("num animations: %d\n", iNumAnimations);

        uint32_t iNumMeshPositions = *piData++;
        DEBUG_PRINTF("num mesh positions: %d\n", iNumMeshPositions);
        float3 const* pPositions = (float3 const*)piData;
        std::vector<float3> aPositions(iNumMeshPositions);
        memcpy(aPositions.data(), pPositions, sizeof(float3) * iNumMeshPositions);
        pPositions += iNumMeshPositions;

        piData = (uint32_t const*)pPositions;
        uint32_t iNumMeshNormals = *piData++;
        DEBUG_PRINTF("num mesh normals: %d\n", iNumMeshNormals);
        float3 const* pNormals = (float3 const*)piData;
        std::vector<float3> aNormals(iNumMeshNormals);
        memcpy(aNormals.data(), pNormals, sizeof(float3) * iNumMeshNormals);
        pNormals += iNumMeshNormals;

        piData = (uint32_t const*)pNormals;
        uint32_t iNumTexCoords = *piData++;
        DEBUG_PRINTF("num mesh tex coords: %d\n", iNumTexCoords);
        float2 const* pTexCoord = (float2 const*)piData;
        std::vector<float2> aTexCoords(iNumTexCoords);
        memcpy(aTexCoords.data(), pTexCoord, sizeof(float2) * iNumTexCoords);
        pTexCoord += iNumTexCoords;

        assert(iNumMeshPositions == iNumMeshNormals);
        assert(iNumMeshPositions == iNumTexCoords);

        piData = (uint32_t const*)pTexCoord;
        uint32_t iNumTriangleIndices = *piData++;
        DEBUG_PRINTF("num triangle indices: %d\n", iNumTriangleIndices);
        std::vector<uint32_t> aiTriangleIndices(iNumTriangleIndices);
        memcpy(aiTriangleIndices.data(), piData, sizeof(uint32_t) * iNumTriangleIndices);
        piData += iNumTriangleIndices;

        MeshTriangleRange triangleRange;
        triangleRange.miStart = 0;
        triangleRange.miEnd = iNumTriangleIndices;

        MeshVertexRange vertexRange;
        vertexRange.miStart = 0;
        vertexRange.miEnd = iNumMeshPositions;

        uint32_t iAnimMeshID = 0;

        // convert to vertex format
        std::vector<Vertex> aTotalVertices(iNumMeshPositions);
        for(uint32_t i = 0; i < iNumMeshPositions; i++)
        {
            Vertex& v = aTotalVertices[i];
            v.mPosition = float4(aPositions[i], 1.0f);
            v.mNormal = float4(aNormals[i], 1.0f);
            v.mUV = float4(aTexCoords[i].x, aTexCoords[i].y, (float)iAnimMeshID, 1.0f);
        }

        //std::vector<std::vector<float4x4>> aaGlobalInverseBindMatrices(iNumAnimations);
        std::vector<uint32_t> aiJointInfluenceIndex;
        std::vector<float> afJointInfluenceWeights;
        uint32_t iNumJoints = 0;
        
        uint32_t iNumTotalJointWeights = *piData++;
        assert(iNumTotalJointWeights == iNumMeshPositions * 4);
        DEBUG_PRINTF("num total joint weights: %d\n", iNumTotalJointWeights);
        aiJointInfluenceIndex.resize(iNumTotalJointWeights);
        memcpy(aiJointInfluenceIndex.data(), piData, sizeof(uint32_t) * iNumTotalJointWeights);
        piData += iNumTotalJointWeights;
        iNumTotalJointWeights = *piData++;
        float const* pfData = (float const*)piData;
        afJointInfluenceWeights.resize(iNumTotalJointWeights);
        memcpy(afJointInfluenceWeights.data(), pfData, sizeof(float) * iNumTotalJointWeights);
        pfData += iNumTotalJointWeights;

        piData = (uint32_t const*)pfData;
        iNumJoints = *piData++;

        DEBUG_PRINTF("num joints: %d\n", iNumJoints);

        std::vector<uint32_t> aiSkinJointIndices(iNumJoints);
        memcpy(aiSkinJointIndices.data(), piData, sizeof(uint32_t) * iNumJoints);
        piData += iNumJoints;
        iNumJoints = *piData++;

        DEBUG_PRINTF("num global inverse bind matrices: %d\n", iNumJoints);

        float4x4 const* pMatrix = (float4x4 const*)piData;
        std::vector<float4x4> aMatrices(iNumJoints);
        memcpy(aMatrices.data(), pMatrix, sizeof(float4x4) * iNumJoints);
        maaGlobalInverseBindMatrices[iLoadAnimMesh] = aMatrices;
        pMatrix += iNumJoints;

        piData = (uint32_t const*)pMatrix;
        
        uint32_t iNumAnimHierarchies = *piData++;
        assert(iNumAnimHierarchies == 1);
        DEBUG_PRINTF("num anim hierarchies: %d\n", iNumAnimHierarchies);

        // NOTE: local bind matrices has been applied with pre-transform so applying inverse global bind
        // won't work as intended.
        
        iNumJoints = *piData++;
        DEBUG_PRINTF("num joints: %d\n", iNumJoints);

        Joint const* pJoints = (Joint const*)piData;
        std::vector<Joint> aJoints(iNumJoints);
        memcpy(aJoints.data(), pJoints, sizeof(Joint) * iNumJoints);
        maaJoints.push_back(aJoints);
        pJoints += iNumJoints;

        pMatrix = (float4x4 const*)pJoints;
        std::vector<float4x4> aLocalBindMatrices(iNumJoints);
        memcpy(aLocalBindMatrices.data(), pMatrix, sizeof(float4x4) * iNumJoints);
        pMatrix += iNumJoints;
        maaLocalBindMatrices.push_back(aLocalBindMatrices);

        piData = (uint32_t const*)pMatrix;
        
        //std::vector<std::vector<uint32_t>> aaiJointToArrayMapping;
        uint32_t iNumTotalJointMappings = *piData++;
        assert(iNumTotalJointMappings == 1);
        DEBUG_PRINTF("num joint to array mapping: %d\n", iNumTotalJointMappings);

        iNumJoints = *piData++;
        DEBUG_PRINTF("num joints: %d\n", iNumJoints);

        std::vector<uint32_t> aiJointMapIndices(iNumJoints);
        memcpy(aiJointMapIndices.data(), piData, sizeof(uint32_t) * iNumJoints);
        maaiJointToArrayMapping.push_back(aiJointMapIndices);
        piData += iNumJoints;
        
        //std::vector<std::map<uint32_t, std::string>> aaJointMapping;
        iNumTotalJointMappings = *piData++;
        assert(iNumTotalJointMappings == 1);
        DEBUG_PRINTF("num joint mapping entries: %d\n", iNumTotalJointMappings);

        std::map<uint32_t, std::string> aJointMapping;
        iNumJoints = *piData++;
        DEBUG_PRINTF("num joints: %d\n", iNumJoints);

        for(uint32_t iJoint = 0; iJoint < iNumJoints; iJoint++)
        {
            uint32_t iJointIndex = *piData++;
            DEBUG_PRINTF("joint index: %d\n", iJointIndex);

            uint32_t iStrLength = *piData++;
            DEBUG_PRINTF("strlen: %d\n", iStrLength);

            char const* cChar = (char const*)piData;
            std::string s;
            for(uint32_t j = 0; j < iStrLength; j++)
            {
                s += *cChar++;
            }
            piData = (uint32_t const*)cChar;

            aJointMapping[iJointIndex] = s;
            DEBUG_PRINTF("joint %d : %s\n", iJointIndex, s.c_str());
        }
        maaJointMapping.push_back(aJointMapping);
        
        Loader::loadFileFree(acFileContentBuffer);

        AnimMeshGPUBuffers gpuBuffers;

        // mesh vertices
        std::string meshName = baseName + "-vertex-buffer";
        wgpu::BufferDescriptor bufferDesc = {};
        bufferDesc.label = meshName.c_str();
        bufferDesc.mappedAtCreation = false;
        bufferDesc.size = aTotalVertices.size() * sizeof(Vertex);
        bufferDesc.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Storage;
        wgpu::Buffer skinAnimMeshBuffer = mCreateInfo.mpDevice->CreateBuffer(
            &bufferDesc
        );
        mCreateInfo.mpDevice->GetQueue().WriteBuffer(
            skinAnimMeshBuffer,
            0,
            aTotalVertices.data(),
            aTotalVertices.size() * sizeof(Vertex));
        mCreateInfo.mpRenderer->registerBuffer(meshName, skinAnimMeshBuffer);

        gpuBuffers.mVertexBuffer = skinAnimMeshBuffer;

        // mesh indices
        std::string meshIndexName = baseName + "-index-buffer";
        bufferDesc = {};
        bufferDesc.label = meshName.c_str();
        bufferDesc.mappedAtCreation = false;
        bufferDesc.size = aiTriangleIndices.size() * sizeof(uint32_t);
        bufferDesc.usage = wgpu::BufferUsage::Index | wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Storage;
        wgpu::Buffer skinAnimMeshIndexBuffer = mCreateInfo.mpDevice->CreateBuffer(
            &bufferDesc
        );
        mCreateInfo.mpDevice->GetQueue().WriteBuffer(
            skinAnimMeshIndexBuffer,
            0,
            aiTriangleIndices.data(),
            aiTriangleIndices.size() * sizeof(uint32_t));
        mCreateInfo.mpRenderer->registerBuffer(meshIndexName, skinAnimMeshIndexBuffer);

        gpuBuffers.mIndexBuffer = skinAnimMeshIndexBuffer;

        // influence joint indices
        std::string bufferName = baseName + "-influence-joint-indices";
        bufferDesc = {};
        bufferDesc.label = bufferName.c_str();
        bufferDesc.mappedAtCreation = false;
        bufferDesc.size = aiJointInfluenceIndex.size() * sizeof(uint32_t);
        bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Storage;
        wgpu::Buffer influenceJointIndexBuffer = mCreateInfo.mpDevice->CreateBuffer(
            &bufferDesc
        );
        mCreateInfo.mpDevice->GetQueue().WriteBuffer(
            influenceJointIndexBuffer,
            0,
            aiJointInfluenceIndex.data(),
            aiJointInfluenceIndex.size() * sizeof(uint32_t));
        mCreateInfo.mpRenderer->registerBuffer(bufferName, influenceJointIndexBuffer);

        gpuBuffers.mJointInfluenceIndices = influenceJointIndexBuffer;

        wgpu::Device& device = *mCreateInfo.mpDevice;

        // influence joint weights
        bufferName = baseName + "-influence-joint-weights";
        bufferDesc = {};
        bufferDesc.label = bufferName.c_str();
        bufferDesc.mappedAtCreation = false;
        bufferDesc.size = afJointInfluenceWeights.size() * sizeof(float);
        bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Storage;
        wgpu::Buffer influenceJointWeightBuffer = mCreateInfo.mpDevice->CreateBuffer(
            &bufferDesc
        );
        device.GetQueue().WriteBuffer(
            influenceJointWeightBuffer,
            0,
            afJointInfluenceWeights.data(),
            afJointInfluenceWeights.size() * sizeof(float));
        mCreateInfo.mpRenderer->registerBuffer(bufferName, influenceJointWeightBuffer);

        gpuBuffers.mJointInfluenceWeights = influenceJointWeightBuffer;

        // global inverse bind matrices for now
        // bone matrices
        bufferName = baseName + "-joint-inverse-global-bind-matrices";
        bufferDesc = {};
        bufferDesc.label = bufferName.c_str();
        bufferDesc.mappedAtCreation = false;
        bufferDesc.size = maaGlobalInverseBindMatrices[iLoadAnimMesh].size() * sizeof(float4x4);
        bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Storage;
        wgpu::Buffer globalInverseBindMatrixBuffer = device.CreateBuffer(
            &bufferDesc
        );
        device.GetQueue().WriteBuffer(
            globalInverseBindMatrixBuffer,
            0,
            maaGlobalInverseBindMatrices[iLoadAnimMesh].data(),
            maaGlobalInverseBindMatrices[iLoadAnimMesh].size() * sizeof(float4x4));
        mCreateInfo.mpRenderer->registerBuffer(bufferName, globalInverseBindMatrixBuffer);

        gpuBuffers.mJointInveseGlobalBindMatrices = globalInverseBindMatrixBuffer;

        // global bind matrices
        std::vector<float4x4> aGlobalBindMatrices;
        aGlobalBindMatrices.resize(maaGlobalInverseBindMatrices[iLoadAnimMesh].size());
        for(uint32_t i = 0; i < (uint32_t)maaGlobalInverseBindMatrices[iLoadAnimMesh].size(); i++)
        {
            aGlobalBindMatrices[i] = invert(maaGlobalInverseBindMatrices[iLoadAnimMesh][i]);
        }
        
        bufferName = baseName + "-joint-global-bind-matrices";
        bufferDesc = {};
        bufferDesc.label = bufferName.c_str();
        bufferDesc.mappedAtCreation = false;
        bufferDesc.size = aGlobalBindMatrices.size() * sizeof(float4x4);
        bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Storage;
        wgpu::Buffer globalBindMatrixBuffer = device.CreateBuffer(
            &bufferDesc
        );
        device.GetQueue().WriteBuffer(
            globalBindMatrixBuffer,
            0,
            aGlobalBindMatrices.data(),
            aGlobalBindMatrices.size() * sizeof(float4x4));
        mCreateInfo.mpRenderer->registerBuffer(bufferName, globalBindMatrixBuffer);

        gpuBuffers.mJointGlobalBindMatrices = globalBindMatrixBuffer;

        bufferName = baseName + "-joint-animation-total-matrices";
        bufferDesc = {};
        bufferDesc.label = bufferName.c_str();
        bufferDesc.mappedAtCreation = false;
        bufferDesc.size = aGlobalBindMatrices.size() * sizeof(float4x4) * maAnimationNameInfo.size();
        bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Storage;
        wgpu::Buffer jointAnimTotalMatrixBuffer = device.CreateBuffer(
            &bufferDesc
        );
        mCreateInfo.mpRenderer->registerBuffer(bufferName, jointAnimTotalMatrixBuffer);

        gpuBuffers.mJointTotalGlobalAnimationMatrices = jointAnimTotalMatrixBuffer;

        maAnimMeshGPUBuffers[baseName] = gpuBuffers;

        maAnimMeshModelNames.push_back(baseName);

        char* acMaterials = nullptr;
        bufferDesc.size = Loader::loadFile(&acMaterials, baseName + ".mat");
        printf("mesh material size: %d\n", (uint32_t)bufferDesc.size);

        piData = (uint32_t const*)acMaterials;

        uint32_t iNumMaterials = *piData++;
        std::vector<OutputMaterialInfo> aMaterialInfo(iNumMaterials);
        memcpy(aMaterialInfo.data(), piData, sizeof(OutputMaterialInfo) * iNumMaterials);

        Loader::loadFileFree(acMaterials);

        char* acMaterialIDs = nullptr;
        bufferDesc.size = Loader::loadFile(&acMaterialIDs, baseName + ".mid");
        printf("mesh material size: %d\n", (uint32_t)bufferDesc.size);
        piData = (uint32_t const*)acMaterialIDs;

        iNumMeshes = *piData++;
        std::vector<uint32_t> aiMaterialID(iNumMeshes);
        memcpy(aiMaterialID.data(), piData, sizeof(uint32_t) * iNumMeshes);

        Loader::loadFileFree(acMaterialIDs);

        // record the vertex buffer and index buffer names for this file
        std::string vertexBufferName = baseName + "-vertex-buffer";
        std::string indexBufferName = baseName + "-index-buffer";
        uint32_t iCurrNumTriangleIndices = (maAnimMeshTriangleRanges.size() > 0) ? (uint32_t)maAnimMeshTriangleRanges.back().miEnd : 0;
        uint32_t iCurrNumVertices = (maAnimMeshVertexRanges.size() > 0) ? (uint32_t)maAnimMeshVertexRanges.back().miEnd : 0;
        uint32_t iNumTotalVertices = iCurrNumVertices;
        for(uint32_t i = 0; i < animFileInfo.maMeshInstanceNames.size(); i++)
        {
            // record the vertex buffer and index buffer names for this file
            maAnimMeshVertexBufferNames.push_back(vertexBufferName);
            maAnimMeshIndexBufferNames.push_back(indexBufferName);

            // triangle index and vertex ranges
            MeshTriangleRange triangleRange;
            triangleRange.miStart = iCurrNumTriangleIndices;
            triangleRange.miEnd = triangleRange.miStart + iNumTriangleIndices;
            
            MeshVertexRange vertexRange;
            vertexRange.miStart = iCurrNumVertices;
            vertexRange.miEnd = vertexRange.miStart + iNumMeshPositions;
            
            maAnimMeshTriangleRanges.push_back(triangleRange);
            maAnimMeshVertexRanges.push_back(vertexRange);

            uint32_t iLastVertexBufferSize = (uint32_t)aTotalVertexBuffer.size();
            aTotalVertexBuffer.resize(iLastVertexBufferSize + aTotalVertices.size());
            memcpy(
                (char*)aTotalVertexBuffer.data() + iLastVertexBufferSize * sizeof(Vertex),
                aTotalVertices.data(),
                aTotalVertices.size() * sizeof(Vertex)
            );
            
            uint32_t iLastIndexBufferSize = (uint32_t)aTotalIndexBuffer.size();
            aTotalIndexBuffer.resize(iLastIndexBufferSize + aiTriangleIndices.size());
            memcpy(
                (char*)aTotalIndexBuffer.data() + iLastIndexBufferSize * sizeof(uint32_t),
                aiTriangleIndices.data(),
                sizeof(uint32_t) * aiTriangleIndices.size()
            );

            uint32_t iLastJointInfluenceWeights = (uint32_t)afTotalJointInfluenceWeights.size();
            afTotalJointInfluenceWeights.resize(iLastJointInfluenceWeights + afJointInfluenceWeights.size());
            memcpy(
                (char*)afTotalJointInfluenceWeights.data() + iLastJointInfluenceWeights * sizeof(uint32_t),
                afJointInfluenceWeights.data(),
                afJointInfluenceWeights.size() * sizeof(uint32_t)
            );

            uint32_t iLastJointInfluenceIndices = (uint32_t)aiTotalJointInfluenceIndices.size();
            aiTotalJointInfluenceIndices.resize(iLastJointInfluenceIndices + aiJointInfluenceIndex.size());
            memcpy(
                (char*)aiTotalJointInfluenceIndices.data() + iLastJointInfluenceIndices * sizeof(float),
                aiJointInfluenceIndex.data(),
                aiJointInfluenceIndex.size() * sizeof(float)
            );

            uint32_t iLastTotalGlobalBindMatrices = (uint32_t)aTotalGlobalBindMatrices.size();
            aTotalGlobalBindMatrices.resize(iLastTotalGlobalBindMatrices + aGlobalBindMatrices.size());
            memcpy(
                (char*)aTotalGlobalBindMatrices.data() + iLastTotalGlobalBindMatrices * sizeof(float4x4),
                aGlobalBindMatrices.data(),
                aGlobalBindMatrices.size() * sizeof(float4x4)
            );

            uint32_t iLastTotalInverseGlobalBindMatrices = (uint32_t)aTotalInverseGlobalBindMatrices.size();
            aTotalInverseGlobalBindMatrices.resize(iLastTotalInverseGlobalBindMatrices + maaGlobalInverseBindMatrices[iLoadAnimMesh].size());
            memcpy(
                (char*)aTotalInverseGlobalBindMatrices.data() + iLastTotalInverseGlobalBindMatrices * sizeof(float4x4),
                maaGlobalInverseBindMatrices[iLoadAnimMesh].data(),
                maaGlobalInverseBindMatrices[iLoadAnimMesh].size() * sizeof(float4x4)
            );

            uint32_t iLastTotalGlobalAnimatedMatrices = (uint32_t)maTotalGlobalAnimationMatrices.size();
            maTotalGlobalAnimationMatrices.resize(iLastTotalGlobalAnimatedMatrices + aGlobalBindMatrices.size());

            uint32_t iLastTotalMaterials = (uint32_t)aTotalMaterialInfo.size();
            aTotalMaterialInfo.resize(iLastTotalMaterials + aMaterialInfo.size());
            memcpy(
                (char*)aTotalMaterialInfo.data() + iLastTotalMaterials * sizeof(OutputMaterialInfo),
                aMaterialInfo.data(),
                aMaterialInfo.size() * sizeof(OutputMaterialInfo)
            );

            uint32_t iLastTotalMaterialIDs = (uint32_t)aiTotalMaterialID.size();
            aiTotalMaterialID.resize(iLastTotalMaterialIDs + aiMaterialID.size());
            memcpy(
                (char*)aiTotalMaterialID.data() + iLastTotalMaterialIDs * sizeof(uint32_t),
                aiMaterialID.data(),
                aiMaterialID.size() * sizeof(uint32_t)
            );

            maiVertexBufferStartIndices.push_back(iLastVertexBufferSize);
            maiIndexBufferStartIndices.push_back(iLastIndexBufferSize);
            maiJointMatrixStartIndices.push_back(iLastTotalGlobalBindMatrices);

            MatchVertexRangeToMeshInstance matchVertexRangeToMeshInstance;
            matchVertexRangeToMeshInstance.miStart = iNumTotalVertices;
            matchVertexRangeToMeshInstance.miEnd = iNumTotalVertices + iNumMeshPositions;
            matchVertexRangeToMeshInstance.miMeshModelIndex = iLoadAnimMesh;
            matchVertexRangeToMeshInstance.miMatrixStartIndex = iLastTotalGlobalBindMatrices;
            matchVertexRangeToMeshInstance.miMatrixEndIndex = iLastTotalGlobalBindMatrices + (uint32_t)maaGlobalInverseBindMatrices[iLoadAnimMesh].size();
            iNumTotalVertices += iNumMeshPositions;

            maMatchVertexRangeToMeshInstance.push_back(matchVertexRangeToMeshInstance);
        }
        
        ++iLoadAnimMesh;

    }   // for anim mesh file to num anim mesh file

    // total vertex buffer
    std::string uniformBufferName = "total-anim-mesh-vertex-buffer";
    wgpu::BufferDescriptor bufferDesc = {};
    bufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex;
    bufferDesc.size = aTotalVertexBuffer.size() * sizeof(Vertex);
    maBuffers[uniformBufferName] = mCreateInfo.mpDevice->CreateBuffer(&bufferDesc);
    maBuffers[uniformBufferName].SetLabel(uniformBufferName.c_str());
    maBufferSizes[uniformBufferName] = (uint32_t)bufferDesc.size;
    mCreateInfo.mpRenderer->registerBuffer(uniformBufferName, maBuffers[uniformBufferName]);
    mCreateInfo.mpDevice->GetQueue().WriteBuffer(
        maBuffers[uniformBufferName],
        0,
        aTotalVertexBuffer.data(),
        aTotalVertexBuffer.size() * sizeof(Vertex)
    );
    maTotalAnimMeshVertexBufferNames.resize(maAnimationNameInfo.size());
    for(uint32_t iMesh = 0; iMesh < maAnimationNameInfo.size(); iMesh++)
    {
        maTotalAnimMeshVertexBufferNames[iMesh] = uniformBufferName;
    }

    // total index buffer
    uniformBufferName = "total-anim-mesh-index-buffer";
    bufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Index;
    bufferDesc.size = aTotalIndexBuffer.size() * sizeof(uint32_t);
    maBuffers[uniformBufferName] = mCreateInfo.mpDevice->CreateBuffer(&bufferDesc);
    maBuffers[uniformBufferName].SetLabel(uniformBufferName.c_str());
    maBufferSizes[uniformBufferName] = (uint32_t)bufferDesc.size;
    mCreateInfo.mpRenderer->registerBuffer(uniformBufferName, maBuffers[uniformBufferName]);
    mCreateInfo.mpDevice->GetQueue().WriteBuffer(
        maBuffers[uniformBufferName],
        0,
        aTotalIndexBuffer.data(),
        aTotalIndexBuffer.size() * sizeof(uint32_t)
    );
    maTotalAnimMeshIndexBufferNames.resize(maAnimationNameInfo.size());
    for(uint32_t iMesh = 0; iMesh < maAnimationNameInfo.size(); iMesh++)
    {
        maTotalAnimMeshIndexBufferNames[iMesh] = uniformBufferName;
    }

    // total joint influence weight buffer
    uniformBufferName = "total-influence-joint-weights";
    bufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
    bufferDesc.size = afTotalJointInfluenceWeights.size() * sizeof(float);
    maBuffers[uniformBufferName] = mCreateInfo.mpDevice->CreateBuffer(&bufferDesc);
    maBuffers[uniformBufferName].SetLabel(uniformBufferName.c_str());
    maBufferSizes[uniformBufferName] = (uint32_t)bufferDesc.size;
    mCreateInfo.mpRenderer->registerBuffer(uniformBufferName, maBuffers[uniformBufferName]);
    mCreateInfo.mpDevice->GetQueue().WriteBuffer(
        maBuffers[uniformBufferName],
        0,
        afTotalJointInfluenceWeights.data(),
        afTotalJointInfluenceWeights.size() * sizeof(float)
    );

    // total joint influence weight buffer
    uniformBufferName = "total-influence-joint-indices";
    bufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
    bufferDesc.size =aiTotalJointInfluenceIndices.size() * sizeof(float);
    maBuffers[uniformBufferName] = mCreateInfo.mpDevice->CreateBuffer(&bufferDesc);
    maBuffers[uniformBufferName].SetLabel(uniformBufferName.c_str());
    maBufferSizes[uniformBufferName] = (uint32_t)bufferDesc.size;
    mCreateInfo.mpRenderer->registerBuffer(uniformBufferName, maBuffers[uniformBufferName]);
    mCreateInfo.mpDevice->GetQueue().WriteBuffer(
        maBuffers[uniformBufferName],
        0,
        aiTotalJointInfluenceIndices.data(),
        aiTotalJointInfluenceIndices.size() * sizeof(uint32_t)
    );

    // total joint global bind matrices
    uniformBufferName = "total-joint-global-bind-matrices";
    bufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
    bufferDesc.size = aTotalGlobalBindMatrices.size() * sizeof(float4x4);
    maBuffers[uniformBufferName] = mCreateInfo.mpDevice->CreateBuffer(&bufferDesc);
    maBuffers[uniformBufferName].SetLabel(uniformBufferName.c_str());
    maBufferSizes[uniformBufferName] = (uint32_t)bufferDesc.size;
    mCreateInfo.mpRenderer->registerBuffer(uniformBufferName, maBuffers[uniformBufferName]);
    mCreateInfo.mpDevice->GetQueue().WriteBuffer(
        maBuffers[uniformBufferName],
        0,
        aTotalGlobalBindMatrices.data(),
        aTotalGlobalBindMatrices.size() * sizeof(float4x4)
    );

    // total joint inverse global bind matrices
    uniformBufferName = "total-joint-inverse-global-bind-matrices";
    bufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
    bufferDesc.size = aTotalInverseGlobalBindMatrices.size() * sizeof(float4x4);
    maBuffers[uniformBufferName] = mCreateInfo.mpDevice->CreateBuffer(&bufferDesc);
    maBuffers[uniformBufferName].SetLabel(uniformBufferName.c_str());
    maBufferSizes[uniformBufferName] = (uint32_t)bufferDesc.size;
    mCreateInfo.mpRenderer->registerBuffer(uniformBufferName, maBuffers[uniformBufferName]);
    mCreateInfo.mpDevice->GetQueue().WriteBuffer(
        maBuffers[uniformBufferName],
        0,
        aTotalInverseGlobalBindMatrices.data(),
        aTotalInverseGlobalBindMatrices.size() * sizeof(float4x4)
    );

    // total joint global animation matrices
    uniformBufferName = "total-joint-global-animation-matrices";
    bufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
    bufferDesc.size = maTotalGlobalAnimationMatrices.size() * sizeof(float4x4);
    maBuffers[uniformBufferName] = mCreateInfo.mpDevice->CreateBuffer(&bufferDesc);
    maBuffers[uniformBufferName].SetLabel(uniformBufferName.c_str());
    maBufferSizes[uniformBufferName] = (uint32_t)bufferDesc.size;
    mCreateInfo.mpRenderer->registerBuffer(uniformBufferName, maBuffers[uniformBufferName]);

    // total joint matrix start index
    uniformBufferName = "total-joint-matrix-start-indices";
    bufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
    bufferDesc.size = maiJointMatrixStartIndices.size() * sizeof(uint32_t);
    maBuffers[uniformBufferName] = mCreateInfo.mpDevice->CreateBuffer(&bufferDesc);
    maBuffers[uniformBufferName].SetLabel(uniformBufferName.c_str());
    maBufferSizes[uniformBufferName] = (uint32_t)bufferDesc.size;
    mCreateInfo.mpRenderer->registerBuffer(uniformBufferName, maBuffers[uniformBufferName]);
    mCreateInfo.mpDevice->GetQueue().WriteBuffer(
        maBuffers[uniformBufferName],
        0,
        maiJointMatrixStartIndices.data(),
        maiJointMatrixStartIndices.size() * sizeof(uint32_t)
    );

    // total anim mesh materials
    std::string bufferName = "total-anim-mesh-materials";
    bufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
    bufferDesc.size = aTotalMaterialInfo.size() * sizeof(OutputMaterialInfo);
    maBuffers[bufferName] = mCreateInfo.mpDevice->CreateBuffer(&bufferDesc);
    maBuffers[bufferName].SetLabel(bufferName.c_str());
    maBufferSizes[bufferName] = (uint32_t)bufferDesc.size;
    mCreateInfo.mpRenderer->registerBuffer(bufferName, maBuffers[bufferName]);
    mCreateInfo.mpDevice->GetQueue().WriteBuffer(
        maBuffers[bufferName],
        0,
        aTotalMaterialInfo.data(),
        aTotalMaterialInfo.size() * sizeof(OutputMaterialInfo)
    );

    // total anim mesh material id
    bufferName = "total-anim-mesh-material-ids";
    bufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
    bufferDesc.size = aiTotalMaterialID.size() * sizeof(uint32_t);
    maBuffers[bufferName] = mCreateInfo.mpDevice->CreateBuffer(&bufferDesc);
    maBuffers[bufferName].SetLabel(bufferName.c_str());
    maBufferSizes[bufferName] = (uint32_t)bufferDesc.size;
    mCreateInfo.mpRenderer->registerBuffer(bufferName, maBuffers[bufferName]);
    mCreateInfo.mpDevice->GetQueue().WriteBuffer(
        maBuffers[bufferName],
        0,
        aiTotalMaterialID.data(),
        aiTotalMaterialID.size() * sizeof(uint32_t)
    );

    bufferName = "anim-mesh-instance-vertex-ranges";
    bufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
    bufferDesc.size = maAnimMeshVertexRanges.size() * sizeof(MeshVertexRange);
    maBuffers[bufferName] = mCreateInfo.mpDevice->CreateBuffer(&bufferDesc);
    maBuffers[bufferName].SetLabel(bufferName.c_str());
    maBufferSizes[bufferName] = (uint32_t)bufferDesc.size;
    mCreateInfo.mpRenderer->registerBuffer(bufferName, maBuffers[bufferName]);
    mCreateInfo.mpDevice->GetQueue().WriteBuffer(
        maBuffers[bufferName],
        0,
        maAnimMeshVertexRanges.data(),
        maAnimMeshVertexRanges.size() * sizeof(MeshVertexRange)
    );

    uniformBufferName = "match-vertex-range-to-anim-mesh-instance";
    bufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex;
    bufferDesc.size = maMatchVertexRangeToMeshInstance.size() * sizeof(MatchVertexRangeToMeshInstance);
    maBuffers[uniformBufferName] = mCreateInfo.mpDevice->CreateBuffer(&bufferDesc);
    maBuffers[uniformBufferName].SetLabel(uniformBufferName.c_str());
    maBufferSizes[uniformBufferName] = (uint32_t)bufferDesc.size;
    mCreateInfo.mpRenderer->registerBuffer(uniformBufferName, maBuffers[uniformBufferName]);
    mCreateInfo.mpDevice->GetQueue().WriteBuffer(
        maBuffers[uniformBufferName],
        0,
        maMatchVertexRangeToMeshInstance.data(),
        maMatchVertexRangeToMeshInstance.size() * sizeof(MatchVertexRangeToMeshInstance)
    );

    // uniform buffers for all the animated mesh models
    uniformBufferName = "animMeshModelUniforms";
    bufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
    bufferDesc.size = 256 * 256;            // 256 is the min binding alignment
    maBuffers[uniformBufferName] = mCreateInfo.mpDevice->CreateBuffer(&bufferDesc);
    maBuffers[uniformBufferName].SetLabel(uniformBufferName.c_str());
    maBufferSizes[uniformBufferName] = (uint32_t)bufferDesc.size;
    mCreateInfo.mpRenderer->registerBuffer(uniformBufferName, maBuffers[uniformBufferName]);

}