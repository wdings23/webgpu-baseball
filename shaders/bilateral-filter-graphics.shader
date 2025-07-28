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

struct ConstantBufferData
{
    mfSigmaSpatial: f32,
    mfSigmaRange: f32,
    mfRadius: f32,
};

@group(0) @binding(0)
var inputTexture: texture_2d<f32>;

@group(0) @binding(1)
var normalTexture: texture_2d<f32>;

@group(1) @binding(0)
var<uniform> constantBuffer: ConstantBufferData;

@group(1) @binding(1)
var<uniform> defaultUniformBuffer: DefaultUniformData;

@group(1) @binding(2)
var textureSampler: sampler;

struct VertexOutput 
{
    @builtin(position) pos: vec4f,
    @location(0) uv: vec2f,
};

struct FragmentOutput 
{
    @location(0) mOutput : vec4<f32>,
};

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

    let screenCoord: vec2i = vec2i(
        i32(f32(defaultUniformBuffer.miScreenWidth) * in.uv.x),
        i32(f32(defaultUniformBuffer.miScreenHeight) * in.uv.y) 
    );
    let filtered: vec3<f32> = bilateralFilter(
        in.uv
    );

    out.mOutput = vec4f(filtered.xyz, 1.0f);
    return out;
}

/*
**
*/
fn gaussian(
    fX: f32, 
    fSigma: f32) -> f32
{
    return exp(- (fX * fX) / (2.0f * fSigma * fSigma));
}

/*
**
*/
fn bilateralFilter(uv: vec2<f32>) -> vec3<f32>
{
    let textureSize: vec2<u32> = textureDimensions(inputTexture);

    let texelSize: vec2<f32> = vec2<f32>(1.0f / f32(textureSize.x), 1.0f / f32(textureSize.y));       // 1.0 / texture resolution
    //let fSigmaSpatial: f32 = 2.0f * 0.01f;
    //let fSigmaRange: f32 = 0.1f * 20.0f; 
    let iKernelRadius: i32 = i32(constantBuffer.mfRadius);       

    let centerColor: vec3<f32> = textureSample(
        inputTexture,
        textureSampler,
        uv
    ).xyz;
    let normal: vec3<f32> = textureSample(
        normalTexture,
        textureSampler,
        uv
    ).xyz;

    var sum: vec3<f32> = vec3<f32>(0.0f, 0.0f, 0.0f);
    var fWeightSum: f32 = 0.0f;

    for (var y: i32 = -iKernelRadius; y <= iKernelRadius; y++)
    {
        for (var x: i32 = -iKernelRadius; x <= iKernelRadius; x++)
        {
            let offset: vec2<f32> = vec2<f32>(f32(x), f32(y)) * texelSize;
            let sampleUV: vec2<f32> = uv + offset;

            let sampleColor: vec3<f32> = textureSample(
                inputTexture,
                textureSampler,
                sampleUV
            ).xyz;

            let sampleNormal: vec3<f32> = textureSample(
                normalTexture,
                textureSampler,
                sampleUV
            ).xyz;

            let fSpatialWeight: f32 = gaussian(length(offset), constantBuffer.mfSigmaSpatial);
            let fRangeWeight: f32 = gaussian(length(sampleColor - centerColor), constantBuffer.mfSigmaRange);
            let fRangeWeightNormal: f32 = gaussian(length(sampleNormal - normal), constantBuffer.mfSigmaRange);

            let fWeight: f32 = fSpatialWeight * 
                //fRangeWeight * 
                fRangeWeightNormal;

            sum += sampleColor * fWeight;
            fWeightSum += fWeight;
        }
    }

    return vec3<f32>(sum / fWeightSum);
}