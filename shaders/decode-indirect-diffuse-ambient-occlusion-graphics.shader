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
    @location(0) mIndirectDiffuse: vec4<f32>,
    @location(1) mAmbientOcclusionOutput: vec4<f32>,
    @location(2) mIndirectDiffuseMoments: vec4<f32>,
    @location(3) mAmbientOcclusionMoments: vec4<f32>,
    @location(4) mVariance: vec4<f32>,
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

struct SHOutput
{
    mSphericalHarmonicsCoefficient0: vec4<f32>,
    mSphericalHarmonicsCoefficient1: vec4<f32>,
    mSphericalHarmonicsCoefficient2: vec4<f32>,
}

@group(0) @binding(0)
var sphericalHarmonicsTexture0: texture_2d<f32>;

@group(0) @binding(1)
var sphericalHarmonicsTexture1: texture_2d<f32>;

@group(0) @binding(2)
var sphericalHarmonicsTexture2: texture_2d<f32>;

@group(0) @binding(3)
var ambientOcclusionTexture: texture_2d<f32>;

@group(0) @binding(4)
var normalTexture: texture_2d<f32>;

@group(0) @binding(5)
var prevIndirectLightingTexture: texture_2d<f32>;

@group(0) @binding(6)
var prevAmbientOcclusionTexture: texture_2d<f32>;

@group(0) @binding(7)
var motionVectorTexture: texture_2d<f32>;

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
    let textureSize: vec2<u32> = textureDimensions(sphericalHarmonicsTexture0);
    let screenSize: vec2<i32> = vec2<i32>(
        i32(textureSize.x),
        i32(textureSize.y)
    );
    let screenCoord: vec2<i32> = vec2<i32>(
        i32(in.uv.x * f32(screenSize.x)),
        i32(in.uv.y * f32(screenSize.y))
    );

    let sphericalHarmonicsCoefficient0: vec4<f32> = textureLoad(
        sphericalHarmonicsTexture0,
        screenCoord,
        0
    );
    let sphericalHarmonicsCoefficient1: vec4<f32> = textureLoad(
        sphericalHarmonicsTexture1,
        screenCoord,
        0
    );
    let sphericalHarmonicsCoefficient2: vec4<f32> = textureLoad(
        sphericalHarmonicsTexture2,
        screenCoord,
        0
    );
    let ambientOcclusion: vec4<f32> = textureLoad(
        ambientOcclusionTexture,
        screenCoord,
        0
    );
    let normal: vec4<f32> = textureLoad(
        normalTexture,
        screenCoord,
        0
    );

    let motionVector: vec4<f32> = textureLoad(
        motionVectorTexture,
        screenCoord,
        0
    );
    var prevScreenCoord: vec2<i32> = vec2<i32>(
        screenCoord.x + i32(motionVector.x * f32(screenSize.x)),
        screenCoord.y + i32(motionVector.y * f32(screenSize.y))
    );
    let prevIndirectLighting: vec4<f32> = textureLoad(
        prevIndirectLightingTexture,
        prevScreenCoord,
        0
    );
    let prevAmbientOcclusion: vec4<f32> = textureLoad(
        prevAmbientOcclusionTexture,
        prevScreenCoord,
        0
    );

    let fHistoryCount: f32 = ambientOcclusion.x;
    let fValidHistoryCount: f32 = ambientOcclusion.y;
    let fOccludedHistoryCount: f32 = ambientOcclusion.z;
    let fAO: f32 = 1.0f - (fOccludedHistoryCount / fHistoryCount);
    let fPrevAO: f32 = 1.0f - (prevAmbientOcclusion.z / prevAmbientOcclusion.x);
    let fAODiff: f32 = fAO - fPrevAO;
    let fPrevLuminance: f32 = computeLuminance(prevIndirectLighting.xyz);

    let output: vec3<f32> = decodeFromSphericalHarmonicCoefficients(
        sphericalHarmonicsCoefficient0,
        sphericalHarmonicsCoefficient1,
        sphericalHarmonicsCoefficient2,
        normal.xyz,
        vec3<f32>(10.0f, 10.0f, 10.0f),
        fValidHistoryCount
    );

    let fLuminance: f32 = computeLuminance(output);
    let fLuminanceDiff: f32 = fLuminance - fPrevLuminance;

    out.mIndirectDiffuse = vec4<f32>(output, 1.0f);
    out.mAmbientOcclusionOutput = vec4<f32>(
        fAO,
        fAO,
        fAO,
        1.0f
    );
    let indirectLightingDiff: vec3<f32> = out.mIndirectDiffuse.xyz - prevIndirectLighting.xyz;
    let ambientOcclusionDiff: vec3<f32> = vec3<f32>(fAODiff, fAODiff, fAODiff);
    out.mIndirectDiffuseMoments = vec4<f32>(
        fLuminanceDiff,
        fLuminanceDiff * fLuminanceDiff,
        0.0f,
        0.0f
    );
    out.mAmbientOcclusionMoments = vec4<f32>(
        fAODiff,
        fAODiff * fAODiff,
        0.0f,
        0.0f
    );

    return out;
}

/*
**
*/
fn decodeFromSphericalHarmonicCoefficients(
    sphericalHarmonicsCoefficient0: vec4<f32>,
    sphericalHarmonicsCoefficient1: vec4<f32>,
    sphericalHarmonicsCoefficient2: vec4<f32>,
    direction: vec3<f32>,
    maxRadiance: vec3<f32>,
    fHistoryCount: f32
) -> vec3<f32>
{
    var aTotalCoefficients: array<vec3<f32>, 4>;
    let fFactor: f32 = 1.0f;

    aTotalCoefficients[0] = vec3<f32>(sphericalHarmonicsCoefficient0.x, sphericalHarmonicsCoefficient0.y, sphericalHarmonicsCoefficient0.z) * fFactor;
    aTotalCoefficients[1] = vec3<f32>(sphericalHarmonicsCoefficient0.w, sphericalHarmonicsCoefficient1.x, sphericalHarmonicsCoefficient1.y) * fFactor;
    aTotalCoefficients[2] = vec3<f32>(sphericalHarmonicsCoefficient1.z, sphericalHarmonicsCoefficient1.w, sphericalHarmonicsCoefficient2.x) * fFactor;
    aTotalCoefficients[3] = vec3<f32>(sphericalHarmonicsCoefficient2.y, sphericalHarmonicsCoefficient2.z, sphericalHarmonicsCoefficient2.w) * fFactor;

    let fC1: f32 = 0.42904276540489171563379376569857f;
    let fC2: f32 = 0.51166335397324424423977581244463f;
    let fC3: f32 = 0.24770795610037568833406429782001f;
    let fC4: f32 = 0.88622692545275801364908374167057f;

    var decoded: vec3<f32> =
        aTotalCoefficients[0] * fC4 +
        (aTotalCoefficients[3] * direction.x + aTotalCoefficients[1] * direction.y + aTotalCoefficients[2] * direction.z) *
        fC2 * 2.0f;
    decoded /= fHistoryCount;
    decoded = clamp(decoded, vec3<f32>(0.0f, 0.0f, 0.0f), maxRadiance);

    return decoded;
}

/*
**
*/
fn computeLuminance(
    radiance: vec3<f32>) -> f32
{
    return dot(radiance, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
}
