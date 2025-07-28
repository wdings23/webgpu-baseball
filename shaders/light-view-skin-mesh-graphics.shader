const PI: f32 = 3.14159f;

struct DefaultUniformData
{
    miScreenWidth: i32,
    miScreenHeight: i32,
    miFrame: i32,
    miNumMeshes: u32,

    mfRand0: f32,
    mfRand1: f32,
    mfRand2: f32,
    mfRand3: f32,

    mViewProjectionMatrix: mat4x4<f32>,
    mPrevViewProjectionMatrix: mat4x4<f32>,
    mViewMatrix: mat4x4<f32>,
    mProjectionMatrix: mat4x4<f32>,

    mJitteredViewProjectionMatrix: mat4x4<f32>,
    mPrevJitteredViewProjectionMatrix: mat4x4<f32>,

    mCameraPosition: vec4<f32>,
    mCameraLookDir: vec4<f32>,

    mLightRadiance: vec4<f32>,
    mLightDirection: vec4<f32>,
};

struct Material
{
    mDiffuse: vec4<f32>,
    mSpecular: vec4<f32>,
    mEmissive: vec4<f32>,

    miID: u32,
    miAlbedoTextureID: u32,
    miNormalTextureID: u32,
    miEmissiveTextureID: u32
};

struct Range
{
    miStart: i32,
    miEnd: i32
};

struct MeshExtent
{
    mMinPosition: vec4<f32>,
    mMaxPosition: vec4<f32>,
};

struct SelectMeshInfo
{
    miMeshID: u32,
    miSelectionX: i32,
    miSelectionY: i32,
    mMinPosition: vec3<f32>,
    mMaxPosition: vec3<f32>,
};

struct AnimMeshModelUniform
{
    mModelMatrix: mat4x4<f32>,
    mLightViewProjectionMatrix: mat4x4<f32>,
    mTextureAtlasInfo: vec4<f32>,
    mExtraInfo: vec4<f32>,
};

struct UniformData
{
    maLightViewProjectionMatrices: array<mat4x4<f32>, 3>,
    mDecalViewProjectionMatrix: mat4x4<f32>,
};

struct ConstantBufferData
{
    mCascadePartition: f32
};

@group(1) @binding(0)
var<storage, read> uniformBuffer: UniformData;

@group(1) @binding(1)
var<uniform> constantBuffer: ConstantBufferData;

@group(1) @binding(2)
var<uniform> defaultUniformBuffer: DefaultUniformData;

@group(1) @binding(3)
var textureSampler: sampler;

@group(2) @binding(0)
var<storage> animMeshUniformBuffer: AnimMeshModelUniform;

struct VertexInput 
{
    @location(0) worldPosition : vec4<f32>,
    @location(1) texCoord: vec4<f32>,
    @location(2) normal : vec4<f32>
};
struct VertexOutput 
{
    @builtin(position) pos: vec4<f32>,
    @location(0) clipSpacePosition: vec4<f32>,
    @location(1) worldPosition: vec4<f32>,
};
struct FragmentOutput 
{
    @location(0) clipSpacePosition : vec4<f32>,
    @location(1) worldPosition: vec4<f32>,
    @location(2) moment: vec4<f32>,
};

@vertex
fn vs_main(in: VertexInput,
    @builtin(vertex_index) iVertexIndex: u32) -> VertexOutput 
{
    var out: VertexOutput;
    
    let iCascade: u32 = u32(floor(constantBuffer.mCascadePartition - 1.0f));
    let iMesh: u32 = u32(ceil(animMeshUniformBuffer.mExtraInfo.x - 0.5f));
    let animMeshMatrix: mat4x4<f32> = animMeshUniformBuffer.mModelMatrix;
    let totalMatrix: mat4x4<f32> = animMeshMatrix * uniformBuffer.maLightViewProjectionMatrices[iCascade];

    out.pos = vec4<f32>(in.worldPosition.xyz * 10.0f, 1.0f) * totalMatrix;
    out.worldPosition = vec4<f32>(in.worldPosition.xyz * 10.0f, 1.0f) * animMeshMatrix;
    out.clipSpacePosition = out.pos;

    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> FragmentOutput 
{
    var out: FragmentOutput;
    out.clipSpacePosition = in.clipSpacePosition;
    out.worldPosition = in.worldPosition;

    out.moment = vec4<f32>(
        in.clipSpacePosition.z,
        in.clipSpacePosition.z * in.clipSpacePosition.z,
        0.0f,
        0.0f 
    );

    return out;
}

