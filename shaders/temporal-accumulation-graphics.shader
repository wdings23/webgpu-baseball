const UINT32_MAX: u32 = 1000000;
const FLT_MAX: f32 = 1.0e+10;
const PI: f32 = 3.14159f;
const kfOneOverMaxBlendFrames: f32 = 1.0f / 10.0f;

struct VertexInput 
{
    @location(0) pos : vec4<f32>,
    @location(1) texCoord: vec2<f32>,
    @location(2) normal : vec4<f32>
};
struct VertexOutput 
{
    @builtin(position) pos: vec4f,
    @location(0) uv: vec2f,
};
struct FragmentOutput 
{
    @location(0) mAmbientOcclusion: vec4<f32>,
    @location(1) mIndirectLighting: vec4<f32>,
};

struct UniformData
{
    miFrame: u32,
    miScreenWidth: u32,
    miScreenHeight: u32,
    mfRand: f32,

    mViewProjectionMatrix: mat4x4<f32>,
    
    miStep: u32,

};

struct RandomResult 
{
    mfNum: f32,
    miSeed: u32,
};

struct SVGFFilterResult
{
    mRadiance: vec3<f32>,
    mMoments: vec3<f32>,
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

@group(0) @binding(0)
var motionVectorTexture: texture_2d<f32>;

@group(0) @binding(1)
var previousMotionVectorTexture: texture_2d<f32>;

@group(0) @binding(2)
var indirectLightingTexture: texture_2d<f32>;

@group(0) @binding(3)
var previousIndirectLightingTexture: texture_2d<f32>;

@group(0) @binding(4)
var ambientOcclusionTexture: texture_2d<f32>;

@group(0) @binding(5)
var previousAmbientOcclusionTexture: texture_2d<f32>;

@group(1) @binding(0)
var<uniform> defaultUniformBuffer: DefaultUniformData;

@group(1) @binding(1)
var textureSampler: sampler;

@vertex
fn vs_main(@builtin(vertex_index) i : u32) -> VertexOutput 
{
    const pos = array(vec2f(-1, 3), vec2f(-1, -1), vec2f(3, -1));
    const uv = array(vec2f(0, -1), vec2f(0, 1), vec2f(2, 1));
    var output: VertexOutput;
    output.pos = vec4f(pos[i], 0.0f, 1.0f);
    output.uv = uv[i];        

    return output;
}

@fragment
fn fs_main(in: VertexOutput) -> FragmentOutput 
{
    var out: FragmentOutput;

    let motionVectorTextureSize: vec2<u32> = textureDimensions(motionVectorTexture);
    let screenCoord: vec2<i32> = vec2<i32>(
        i32(f32(motionVectorTextureSize.x) * in.uv.x),
        i32(f32(motionVectorTextureSize.y) * in.uv.y)
    );

    let motionVector: vec4<f32> = textureLoad(
        motionVectorTexture,
        screenCoord,
        0
    );
    let backProjectedUV: vec2<f32> = in.uv.xy + motionVector.xy;
    let backProjectedScreenCoord: vec2<i32> = vec2<i32>(
        i32(backProjectedUV.x * f32(motionVectorTextureSize.x)),
        i32(backProjectedUV.y * f32(motionVectorTextureSize.y))
    );

    // compare depth for disocclusion
    var bDisoccluded: bool = false;
    let previousMotionVector: vec4<f32> = textureLoad(
        previousMotionVectorTexture,
        backProjectedScreenCoord,
        0
    );
    if(abs(previousMotionVector.w - motionVector.w) >= 0.0005f)
    {
        bDisoccluded = true;
    }

    // fetch from current and previous accumulated frame
    let indirectLightTextureSize: vec2<u32> = textureDimensions(indirectLightingTexture);
    let indirectLightScreenCoord: vec2<i32> = vec2<i32>(
        i32(f32(indirectLightTextureSize.x) * in.uv.x),
        i32(f32(indirectLightTextureSize.y) * in.uv.y)
    );
    let backProjectedIndirectLightScreenCoord: vec2<i32> = vec2<i32>(
        i32(f32(indirectLightTextureSize.x) * backProjectedUV.x),
        i32(f32(indirectLightTextureSize.y) * backProjectedUV.y)
    );

    var indirectLighting: vec3<f32> = textureLoad(
        indirectLightingTexture,
        indirectLightScreenCoord,
        0
    ).xyz;
    var previousIndirectLighting: vec4<f32> = textureLoad(
        previousIndirectLightingTexture,
        backProjectedIndirectLightScreenCoord,
        0
    );

    var ambientOcclusion: vec3<f32> = textureLoad(
        ambientOcclusionTexture,
        indirectLightScreenCoord,
        0
    ).xyz;
    var previousAmbientOcclusion: vec4<f32> = textureLoad(
        previousAmbientOcclusionTexture,
        backProjectedIndirectLightScreenCoord,
        0
    );

    // accumulate on non-disoccluded pixel
    if(bDisoccluded)
    {
        out.mIndirectLighting = vec4<f32>(
            indirectLighting,
            1.0f
        );
        out.mAmbientOcclusion = vec4<f32>(
            ambientOcclusion,
            1.0f
        );
    }
    else
    {
        let fNumAccumulatedFrames: f32 = previousIndirectLighting.w;
        out.mIndirectLighting = vec4<f32>(mix(
            previousIndirectLighting.xyz, 
            indirectLighting.xyz, 
            1.0f / 30.0f),
            fNumAccumulatedFrames + 1.0f
        );
        out.mAmbientOcclusion = vec4<f32>(mix(
            previousAmbientOcclusion.xyz, 
            ambientOcclusion.xyz, 
            1.0f / 30.0f),
            fNumAccumulatedFrames + 1.0f
        );

        //out.mIndirectLighting = vec4<f32>(
        //    max(previousIndirectLighting.xyz, indirectLighting.xyz),
        //    fNumAccumulatedFrames + 1.0f
        //);
    }
    
    return out;
}

