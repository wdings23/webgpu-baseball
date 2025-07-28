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
};

@group(0) @binding(0)
var inputTexture: texture_2d<f32>;

@group(0) @binding(1)
var momentTexture: texture_2d<f32>;

@group(0) @binding(2)
var normalTexture: texture_2d<f32>;

@group(0) @binding(3)
var prevMomentTexture: texture_2d<f32>;

@group(0) @binding(4)
var motionVectorTexture: texture_2d<f32>;

@group(0) @binding(5)
var ambientOcclusionTexture: texture_2d<f32>;

@group(1) @binding(0)
var<uniform> constantBuffer: ConstantBufferData;

@group(1) @binding(1)
var<uniform> defaultUniformBuffer: DefaultUniformData;

@group(1) @binding(2)
var textureSampler: sampler;

struct FilterOutput
{
    mFiltered: vec3<f32>,
    mVariance: vec3<f32>,
};

struct VertexOutput 
{
    @builtin(position) pos: vec4f,
    @location(0) uv: vec2f,
};

struct FragmentOutput 
{
    @location(0) mOutput : vec4<f32>,
    @location(1) mVariance: vec4<f32>,
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
    let filteredOutput: FilterOutput = bilateralFilter(
        in.uv
    );

    out.mOutput = vec4f(filteredOutput.mFiltered.xyz, 1.0f);

    out.mVariance = vec4<f32>(filteredOutput.mVariance.xyz, 1.0f);

    return out;
}

/*
**
*/
fn gaussian(
    fX: f32, 
    fSigma: f32) -> f32
{
    return exp((fX * fX * -1.0f) / (2.0f * fSigma * fSigma));
}

/*
**
*/
fn bilateralFilter(uv: vec2<f32>) -> FilterOutput
{
    var out: FilterOutput;

    let textureSize: vec2<u32> = textureDimensions(inputTexture);
    let screenCoord: vec2<i32> = vec2<i32>(
        i32(uv.x * f32(textureSize.x)),
        i32(uv.y * f32(textureSize.y)) 
    );

    let texelSize: vec2<f32> = vec2<f32>(1.0f / f32(textureSize.x), 1.0f / f32(textureSize.y));       // 1.0 / texture resolution
    //let fSigmaSpatial: f32 = 2.0f * 0.01f;
    //let fSigmaRange: f32 = 0.1f * 20.0f; 
    var iKernelRadius: i32 = 4;       

    let centerColor: vec3<f32> = textureSample(
        inputTexture,
        textureSampler,
        uv
    ).xyz;
    let normal: vec3<f32> = textureLoad(
        normalTexture,
        screenCoord,
        0
    ).xyz;

    let moment: vec4<f32> = textureLoad(
        momentTexture,
        screenCoord,
        0
    );

    let motionVector: vec4<f32> = textureLoad(
        motionVectorTexture,
        screenCoord,
        0
    );
    let prevScreenCoord: vec2<i32> = vec2<i32>(
        screenCoord.x + i32(motionVector.x + f32(textureSize.x)),
        screenCoord.y + i32(motionVector.y + f32(textureSize.y))
    );
    let prevMoment: vec4<f32> = textureLoad(
        prevMomentTexture,
        prevScreenCoord,
        0
    );
    let ambientOcclusion: vec4<f32> = textureLoad(
        ambientOcclusionTexture,
        screenCoord,
        0
    );

    iKernelRadius = i32(clamp((1000.0f / (ambientOcclusion.x + 1.0f)), 2.0f, 16.0f));

    let fPrevVariance: f32 = prevMoment.y - prevMoment.x * prevMoment.x;
    let fVariance: f32 = moment.y - moment.x * moment.x;
    
    var sum: vec3<f32> = vec3<f32>(0.0f, 0.0f, 0.0f);
    var fWeightSum: f32 = 0.0f;

    var fTemporlVariance: f32 = mix(fPrevVariance, fVariance, 0.3f);

    out.mVariance = vec3<f32>(fVariance, fVariance, fVariance);

    let fRangeScaleFromVariance: f32 = 2.0f;


//out.mAmbientOcclusionOutput = vec4<f32>(
//        fHistoryCount, 
//        fValidHistoryCount, 
//        fOccludedHistoryCount, 
//        1.0f - (fOccludedHistoryCount / fHistoryCount));

    for (var y: i32 = -iKernelRadius; y <= iKernelRadius; y++)
    {
        for (var x: i32 = -iKernelRadius; x <= iKernelRadius; x++)
        {
            let offset: vec2<f32> = vec2<f32>(f32(x), f32(y)) * texelSize;
            let sampleUV: vec2<f32> = uv + offset;
            let sampleScreenCoord: vec2<i32> = vec2<i32>(
                screenCoord.x + x,
                screenCoord.y + y
            );

            let sampleColor: vec3<f32> = textureLoad(
                inputTexture,
                sampleScreenCoord,
                0
            ).xyz;

            let sampleNormal: vec3<f32> = textureLoad(
                normalTexture,
                sampleScreenCoord,
                0
            ).xyz;

            let fSpatialWeight: f32 = gaussian(length(offset), constantBuffer.mfSigmaSpatial);
            let fRangeSigma: f32 = constantBuffer.mfSigmaRange * 3.0f; // (1.0f + fRangeScaleFromVariance * fTemporlVariance);
            let fRangeWeight: f32 = gaussian(length(sampleColor - centerColor), fRangeSigma);
            let fRangeWeightNormal: f32 = gaussian(length(sampleNormal - normal), fRangeSigma);

            let fWeight: f32 = fSpatialWeight * 
                /*fRangeWeight * */ 
                fRangeWeightNormal;

            sum += sampleColor * fWeight;
            fWeightSum += fWeight;
        }
    }

    out.mFiltered = vec3<f32>(sum / fWeightSum);

    return out;
}