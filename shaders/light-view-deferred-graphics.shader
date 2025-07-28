const PI: f32 = 3.14159f;

struct StaticMeshModelUniform
{
    mModelMatrix: mat4x4<f32>,
    mLightViewProjectionMatrix: mat4x4<f32>,
    mTextureAtlasInfo: vec4<f32>,
    mExtraInfo: vec4<f32>,
};

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
var<storage, read> aStaticMeshMatrices: array<mat4x4<f32>>;

@group(1) @binding(2)
var<uniform> constantBuffer: ConstantBufferData;

@group(1) @binding(3)
var<uniform> defaultUniformBuffer: DefaultUniformData;

@group(1) @binding(4)
var textureSampler: sampler;

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
    
    let iMesh: u32 = u32(ceil(in.texCoord.z - 0.5f));

    let iCascade: u32 = u32(floor(constantBuffer.mCascadePartition - 0.5f));

    // total mesh extent is at the very end of list
     var worldPosition: vec4<f32> = vec4<f32>(
        in.worldPosition.x,
        in.worldPosition.y,
        in.worldPosition.z,
        1.0f
    );
    let staticMeshMatrix: mat4x4<f32> = aStaticMeshMatrices[iMesh];
    let totalMatrix: mat4x4<f32> = staticMeshMatrix * uniformBuffer.maLightViewProjectionMatrices[iCascade];
    out.pos = worldPosition * totalMatrix;
    out.clipSpacePosition = out.pos;
    out.worldPosition = worldPosition;

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

