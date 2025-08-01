const MAX_STEPS: i32 = 20;
const DENSITY_MIN: f32 = -1.0f;
const DENSITY_MAX: f32 = 1.0f;

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

    mInverseViewProjectionMatrix: mat4x4<f32>,
};

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
    @location(0) mMotionVector: vec4<f32>,
};

@group(0) @binding(0)
var albedoTexture: texture_2d<f32>;

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

//////
@fragment
fn fs_main(in: VertexOutput) -> FragmentOutput
{
	var output: FragmentOutput;

    let textureSize: vec2<u32> = textureDimensions(albedoTexture);
    let screenCoord: vec2<i32> = vec2<i32>(
        i32(in.uv.x * f32(textureSize.x)),
        i32(in.uv.y * f32(textureSize.y))
    );
    let albedo: vec4<f32> = textureLoad(
        albedoTexture,
        screenCoord,
        0
    );

    if(albedo.w <= 0.0f)
    {
        var clipSpacePos: vec3<f32> = vec3<f32>(in.uv.xy * 2.0f - 1.0f, 1.0f);
        clipSpacePos.y *= -1.0f;
        
        var worldPosition: vec4<f32> = vec4<f32>(clipSpacePos.xyz, 1.0f) * defaultUniformBuffer.mInverseViewProjectionMatrix;
        let fOneOverW: f32 = 1.0f / worldPosition.w;
        worldPosition.x *= fOneOverW;
        worldPosition.y *= fOneOverW;
        worldPosition.z *= fOneOverW;

        var currClipSpace: vec4<f32> = vec4<f32>(worldPosition.xyz, 1.0f) * defaultUniformBuffer.mJitteredViewProjectionMatrix;
        var prevClipSpace: vec4<f32> = vec4<f32>(worldPosition.xyz, 1.0f) * defaultUniformBuffer.mPrevViewProjectionMatrix;

        let fCurrOneOverW: f32 = 1.0f / currClipSpace.w;
        let fPrevOneOverW: f32 = 1.0f / prevClipSpace.w;
        currClipSpace.x *= fCurrOneOverW;
        currClipSpace.y *= fCurrOneOverW;
        currClipSpace.z *= fCurrOneOverW;
        prevClipSpace.x *= fPrevOneOverW;
        prevClipSpace.y *= fPrevOneOverW;
        prevClipSpace.z *= fPrevOneOverW;

        output.mMotionVector = vec4<f32>(
            prevClipSpace.x - currClipSpace.x,
            prevClipSpace.y - currClipSpace.y,
            0.0f,
            1.0f
        );
    }

    return output;
}

