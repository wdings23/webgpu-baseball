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
    @location(2) mSphericalHarmonicsCoefficient0: vec4<f32>,
    @location(3) mSphericalHarmonicsCoefficient1: vec4<f32>,
    @location(4) mSphericalHarmonicsCoefficient2: vec4<f32>,
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

struct LightResult
{
    mDiffuse: vec3<f32>,
    mSpecular: vec3<f32>,
    mAmbient: vec3<f32>,
};

struct LightInfo
{
    mPosition: vec4<f32>,
    mRadiance: vec4<f32>,
}

struct UniformData
{
    maPointLightInfo: array<LightInfo, 16>,
};

struct SSGIResult
{
    mSphericalHarmonicsCoefficient0: vec4<f32>,
    mSphericalHarmonicsCoefficient1: vec4<f32>,
    mSphericalHarmonicsCoefficient2: vec4<f32>,
    mIndirectDiffuseRadiance: vec3<f32>,
    mfNumSamples: f32,
    mfNumValidSamples: f32,
    mfNumOccluded: f32,
};

struct SHOutput
{
    mSphericalHarmonicsCoefficient0: vec4<f32>,
    mSphericalHarmonicsCoefficient1: vec4<f32>,
    mSphericalHarmonicsCoefficient2: vec4<f32>,
}

@group(0) @binding(0)
var worldPositionTexture: texture_2d<f32>;

@group(0) @binding(1)
var normalTexture: texture_2d<f32>;

@group(0) @binding(2)
var albedoTexture: texture_2d<f32>;

@group(0) @binding(3)
var skyTexture: texture_2d<f32>;

@group(0) @binding(4)
var prevIndirectDiffuseTexture: texture_2d<f32>;

@group(0) @binding(5)
var motionVectorTexture: texture_2d<f32>;

@group(0) @binding(6)
var prevMotionVectorTexture: texture_2d<f32>;

@group(0) @binding(7)
var prevSphericalHarmonicsCoefficientTexture0: texture_2d<f32>;

@group(0) @binding(8)
var prevSphericalHarmonicsCoefficientTexture1: texture_2d<f32>;

@group(0) @binding(9)
var prevSphericalHarmonicsCoefficientTexture2: texture_2d<f32>;

@group(0) @binding(10)
var prevAmbientOcclusionTexture: texture_2d<f32>;

@group(1) @binding(0)
var<storage, read> uniformBuffer: UniformData; 

@group(1) @binding(1)
var blueNoiseTexture: texture_2d<f32>;

@group(1) @binding(2)
var<uniform> defaultUniformBuffer: DefaultUniformData;

@group(1) @binding(3)
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
    
    let prevIndirectDiffuseRadiance: vec4<f32> = textureSample(
        prevIndirectDiffuseTexture,
        textureSampler,
        in.uv.xy
    );

    let blueNoiseTextureSize: vec2<u32> = textureDimensions(blueNoiseTexture);

    let textureSize: vec2<u32> = textureDimensions(prevSphericalHarmonicsCoefficientTexture0);
    let screenSize: vec2<i32> = vec2<i32>(
        i32(textureSize.x),
        i32(textureSize.y)
    );
    let screenCoord: vec2<i32> = vec2<i32>(
        i32(in.uv.x * f32(screenSize.x)),
        i32(in.uv.y * f32(screenSize.y))
    );

    let sampleTextureSize: vec2<u32> = textureDimensions(worldPositionTexture);
    let sampleScreenCoord: vec2<i32> = vec2<i32>(
        i32(in.uv.x * f32(sampleTextureSize.x)),
        i32(in.uv.y * f32(sampleTextureSize.y))
    );
    let motionVector: vec4<f32> = textureLoad(
        motionVectorTexture,
        sampleScreenCoord,
        0
    );
    let prevSampleScreenCoord: vec2<i32> = screenCoord + vec2<i32>(
        i32(motionVector.x * f32(sampleTextureSize.x)),
        i32(motionVector.y * f32(sampleTextureSize.y))
    );
    let prevMotionVector: vec4<f32> = textureLoad(
        prevMotionVectorTexture,
        prevSampleScreenCoord,
        0
    );

    let prevAmbientOcclusion: vec4<f32> = textureLoad(
        prevAmbientOcclusionTexture,
        screenCoord,
        0        
    );
    var fHistoryCount: f32 = prevAmbientOcclusion.x;
    var fValidHistoryCount: f32 = prevAmbientOcclusion.y;
    var fOccludedHistoryCount: f32 = prevAmbientOcclusion.z;

    var fMixPct: f32 = 1.0f / 60.0f;
    if(abs(motionVector.w - prevMotionVector.w) > 0.0001f)
    {
        fMixPct = 0.0f;
        fHistoryCount = 0.0f;
        fValidHistoryCount = 0.0f;
        fOccludedHistoryCount = 0.0f;
    }

    var shCoefficient0: vec4<f32> = vec4<f32>(0.0f, 0.0f, 0.0f, 0.0f);
    var shCoefficient1: vec4<f32> = vec4<f32>(0.0f, 0.0f, 0.0f, 0.0f);
    var shCoefficient2: vec4<f32> = vec4<f32>(0.0f, 0.0f, 0.0f, 0.0f);
    if(fMixPct > 0.0f)
    {
        shCoefficient0 = textureLoad(
            prevSphericalHarmonicsCoefficientTexture0,
            screenCoord,
            0
        );
        shCoefficient1 = textureLoad(
            prevSphericalHarmonicsCoefficientTexture1,
            screenCoord,
            0
        );
        shCoefficient2 = textureLoad(
            prevSphericalHarmonicsCoefficientTexture2,
            screenCoord,
            0
        );
    }

    let skyTextureSize: vec2<u32> = textureDimensions(skyTexture);

    let worldPosition: vec4<f32> = textureLoad(
        worldPositionTexture,
        sampleScreenCoord,
        0
    );
    let normal: vec4<f32> = textureLoad(
        normalTexture,
        sampleScreenCoord,
        0
    );

    var fNumSlices: f32 = 8.0f;
    var fNumSections: f32 = 16.0f;
    var iNumLoops: i32 = 1;
    var fSampleRadius: f32 = 256.0f * clamp(fHistoryCount / 100.0f, 0.1f, 1.0f);
    
    if(fHistoryCount <= 100.0f)
    {
        fNumSections *= 4.0f;
    }

    var shOutput: SHOutput;
    shOutput.mSphericalHarmonicsCoefficient0 = shCoefficient0;
    shOutput.mSphericalHarmonicsCoefficient1 = shCoefficient1;
    shOutput.mSphericalHarmonicsCoefficient2 = shCoefficient2;

    var totalSSGIResult: SSGIResult;
    for(var i: i32 = 0; i < iNumLoops; i++)
    {
        //let ssgiResult: SSGIResult = ssgi2(
        //    worldPosition.xyz,
        //    normal.xyz,
        //    in.uv.xy,
        //    shOutput,
        //    screenSize,
        //    vec2<i32>(i32(sampleTextureSize.x), i32(sampleTextureSize.y)),
        //    blueNoiseTextureSize,
        //    fNumSlices,
        //    fNumSections,
        //    defaultUniformBuffer.miFrame + i,
        //    fSampleRadius
        //);

        let ssgiResult: SSGIResult = ssgi(
            in.uv.xy,
            shOutput,
            skyTextureSize
        );
    
        totalSSGIResult.mSphericalHarmonicsCoefficient0 = ssgiResult.mSphericalHarmonicsCoefficient0;
        totalSSGIResult.mSphericalHarmonicsCoefficient1 = ssgiResult.mSphericalHarmonicsCoefficient1;
        totalSSGIResult.mSphericalHarmonicsCoefficient2 = ssgiResult.mSphericalHarmonicsCoefficient2;
        totalSSGIResult.mfNumSamples += ssgiResult.mfNumSamples;
        totalSSGIResult.mfNumValidSamples += ssgiResult.mfNumValidSamples;
        totalSSGIResult.mfNumOccluded += ssgiResult.mfNumOccluded;

        shOutput.mSphericalHarmonicsCoefficient0 = ssgiResult.mSphericalHarmonicsCoefficient0;
        shOutput.mSphericalHarmonicsCoefficient1 = ssgiResult.mSphericalHarmonicsCoefficient1;
        shOutput.mSphericalHarmonicsCoefficient2 = ssgiResult.mSphericalHarmonicsCoefficient2;
    }

    fHistoryCount += totalSSGIResult.mfNumSamples;
    fValidHistoryCount += totalSSGIResult.mfNumValidSamples;
    fOccludedHistoryCount += totalSSGIResult.mfNumOccluded;

    let output: vec3<f32> = decodeFromSphericalHarmonicCoefficients(
        totalSSGIResult,
        normal.xyz,
        vec3<f32>(10.0f, 10.0f, 10.0f),
        fValidHistoryCount
    );

    let prevAO: vec4<f32> = textureLoad(
        prevAmbientOcclusionTexture,
        screenCoord,
        0
    );

    out.mIndirectDiffuse = vec4<f32>(output, fHistoryCount);
    //out.mIndirectDiffuse = vec4<f32>(
    //    1.0f - (fOccludedHistoryCount / fHistoryCount),
    //    1.0f - (fOccludedHistoryCount / fHistoryCount),
    //    1.0f - (fOccludedHistoryCount / fHistoryCount),
    //    1.0f
    //);
    out.mSphericalHarmonicsCoefficient0 = totalSSGIResult.mSphericalHarmonicsCoefficient0;
    out.mSphericalHarmonicsCoefficient1 = totalSSGIResult.mSphericalHarmonicsCoefficient1;
    out.mSphericalHarmonicsCoefficient2 = totalSSGIResult.mSphericalHarmonicsCoefficient2;
    out.mAmbientOcclusionOutput = vec4<f32>(
        fHistoryCount, 
        fValidHistoryCount, 
        fOccludedHistoryCount, 
        1.0f - (fOccludedHistoryCount / fHistoryCount));

    return out;
}



/*
**
*/
fn ssgi(
    uv: vec2<f32>,
    prevSphericalHarmonics: SHOutput,
    skyTextureSize: vec2<u32>) -> SSGIResult
{
    let blueNoiseTextureSize: vec2<u32> = textureDimensions(blueNoiseTexture);
    var ret: SSGIResult;
    ret.mfNumSamples = 0.0f;

    var sphericalHarmonicsOutput: SHOutput = prevSphericalHarmonics;

    let worldPosition: vec4<f32> = textureSample(
        worldPositionTexture,
        textureSampler,
        uv
    );

    let normal: vec4<f32> = textureSample(
        normalTexture,
        textureSampler,
        uv
    );

    let kiNumRays: i32 = 32;
    let kiMaxSteps = 128;
    let kfMaxDistance: f32 = 5.0f;
    var fStepSize: f32 = (1.0f / (1.0f * f32(kiMaxSteps))) * kfMaxDistance;
    var fT: f32 = fStepSize;

    let blueNoise: vec2<f32> = blueNoiseSample(
        uv,
        blueNoiseTextureSize
    );

    let rayDirection: vec3<f32> = sampleHemisphere(
        normal.xyz,
        blueNoise.x,
        blueNoise.y
    );

/*
    // TODO: get ray direction in screen space and march through that
    var clipSpace: vec4<f32> = vec4<f32>(worldPosition.xyz, 1.0f) * defaultUniformBuffer.mViewProjectionMatrix;
    clipSpace.x /= clipSpace.w;
    clipSpace.y /= clipSpace.w;
    clipSpace.x = clipSpace.x * 0.5f + 0.5f;
    clipSpace.y = clipSpace.y * 0.5f + 0.5f;
    clipSpace.x = clamp(clipSpace.x, 0.0f, 1.0f);
    clipSpace.y = clamp(clipSpace.y, 0.0f, 1.0f);
    let sampleScreenCoord0: vec2<i32> = vec2<i32>(
        i32(clipSpace.x * f32(defaultUniformBuffer.miScreenWidth)),
        i32((1.0f - clipSpace.y) * f32(defaultUniformBuffer.miScreenHeight))
    );
    clipSpace = vec4<f32>(worldPosition.xyz + rayDirection.xyz * kfMaxDistance, 1.0f) * defaultUniformBuffer.mViewProjectionMatrix;
    clipSpace.x /= clipSpace.w;
    clipSpace.y /= clipSpace.w;
    clipSpace.x = clipSpace.x * 0.5f + 0.5f;
    clipSpace.y = clipSpace.y * 0.5f + 0.5f;
    clipSpace.x = clamp(clipSpace.x, 0.0f, 1.0f);
    clipSpace.y = clamp(clipSpace.y, 0.0f, 1.0f);
    let sampleScreenCoord1: vec2<i32> = vec2<i32>(
        i32(clipSpace.x * f32(defaultUniformBuffer.miScreenWidth)),
        i32((1.0f - clipSpace.y) * f32(defaultUniformBuffer.miScreenHeight))
    );
    let sampleScreenCoordDiff: vec2<i32> = sampleScreenCoord1 - sampleScreenCoord0;
    var iLargeDiff: i32 = max(abs(sampleScreenCoordDiff.x), abs(sampleScreenCoordDiff.y));
    var totalRadiance: vec3<f32> = vec3<f32>(0.0f, 0.0f, 0.0f);
    let fDXDY: f32 = f32(sampleScreenCoordDiff.x) / f32(sampleScreenCoordDiff.y);
    let fDYDX: f32 = f32(sampleScreenCoordDiff.y) / f32(sampleScreenCoordDiff.x);
    var currSampleScreenCoord: vec2<f32> = vec2<f32>(
        f32(sampleScreenCoord0.x) + fDXDY,
        f32(sampleScreenCoord0.y) + fDYDX
    );
    for(var i: i32 = 0; i < iLargeDiff; i++)
    {
        let sampleScreenCoord: vec2<i32> = vec2<i32>(
            i32(currSampleScreenCoord.x),
            i32(currSampleScreenCoord.y)
        );
        currSampleScreenCoord.x += fDXDY;
        currSampleScreenCoord.y += fDYDX;

        let sampleWorldPosition: vec4<f32> = textureLoad(
            worldPositionTexture,
            sampleScreenCoord,
            0
        );

        let worldPositionDiff: vec3<f32> = sampleWorldPosition.xyz - worldPosition.xyz;
        let fLengthSquared: f32 = dot(worldPositionDiff, worldPositionDiff);
        if(fLengthSquared < 0.1f)
        {
            continue;
        }

        let bIntersect: bool = isPointOnRay(worldPosition.xyz, rayDirection.xyz, sampleWorldPosition.xyz);
        if(bIntersect)
        {
            var sampleToRayPositionDiff: vec3<f32> = sampleWorldPosition.xyz - worldPosition.xyz;
            var fWorldPositionDistance: f32 = length(sampleToRayPositionDiff);

            let sampleAlbedo: vec3<f32> = textureLoad(
                albedoTexture,
                sampleScreenCoord,
                0
            ).xyz * 5.0f;
            let sampleNormal: vec3<f32> = textureLoad(
                normalTexture,
                sampleScreenCoord,
                0
            ).xyz;
            let worldPositionDirection: vec3<f32> = sampleToRayPositionDiff / fWorldPositionDistance;
            let fDP: f32 = max(dot(sampleNormal, worldPositionDirection * -1.0f), 0.0f);
            let fDP1: f32 = max(dot(normal.xyz, rayDirection), 0.0f);
            let fDenom: f32 = max(1.0f / (fWorldPositionDistance * fWorldPositionDistance), 1.0f);
            let radiance: vec3<f32> =  ((sampleAlbedo * fDP * fDP1) * fDenom);
            totalRadiance += radiance; 

            let shOutput: SHOutput = encodeSphericalHarmonics(
                radiance,
                worldPositionDirection, 
                sphericalHarmonicsOutput.mSphericalHarmonicsCoefficient0,
                sphericalHarmonicsOutput.mSphericalHarmonicsCoefficient1,
                sphericalHarmonicsOutput.mSphericalHarmonicsCoefficient2);
            sphericalHarmonicsOutput = shOutput;

            ret.mfNumSamples += 1.0f;
            
            break;
        }

    }
*/

    var fNumOccluded: f32 = 0.0f;
    var totalRadiance: vec3<f32> = vec3<f32>(0.0f, 0.0f, 0.0f);
    var lastScreenCoord: vec2<i32> = vec2<i32>(-1, -1);
    for(var iRay: i32 = 0; iRay < kiNumRays; iRay++)
    {
        var bHit: bool = false;
        for(var iStep: i32 = 0; iStep < kiMaxSteps; iStep++)
        {
            var currRayWorldPosition: vec3<f32> = worldPosition.xyz + rayDirection * fT; 

            var clipSpace: vec4<f32> = vec4<f32>(currRayWorldPosition.xyz, 1.0f) * defaultUniformBuffer.mViewProjectionMatrix;
            clipSpace.x /= clipSpace.w;
            clipSpace.y /= clipSpace.w;
            clipSpace.x = clipSpace.x * 0.5f + 0.5f;
            clipSpace.y = clipSpace.y * 0.5f + 0.5f;
            let sampleScreenCoord: vec2<i32> = vec2<i32>(
                i32(clipSpace.x * f32(defaultUniformBuffer.miScreenWidth)),
                i32((1.0f - clipSpace.y) * f32(defaultUniformBuffer.miScreenHeight))
            );
            if(sampleScreenCoord.x == lastScreenCoord.x && sampleScreenCoord.y == lastScreenCoord.y)
            {
                continue;
            }
            lastScreenCoord = sampleScreenCoord;

            if(clipSpace.x < 0.0f || clipSpace.x > 1.0f ||
               clipSpace.y < 0.0f || clipSpace.y > 1.0f)
            {
                fT += fStepSize;
                continue;
            }

            let sampleWorldPosition: vec3<f32> = textureLoad(
                worldPositionTexture,
                sampleScreenCoord,
                0
            ).xyz;
            
            let sampleToRayPositionDiff: vec3<f32> = sampleWorldPosition - currRayWorldPosition;
            var fLengthSquared = dot(sampleToRayPositionDiff, sampleToRayPositionDiff);
            let fWorldPositionDistance: f32 = length(sampleWorldPosition.xyz - worldPosition.xyz);
            if(fLengthSquared <= 0.0001f && fWorldPositionDistance >= 0.05f)
            {
                if(fWorldPositionDistance < 1.0f)
                {
                    fNumOccluded += 1.0f;
                }
                else 
                {
                    let sampleAlbedo: vec3<f32> = textureLoad(
                        albedoTexture,
                        sampleScreenCoord,
                        0
                    ).xyz;
                    let sampleNormal: vec3<f32> = textureLoad(
                        normalTexture,
                        sampleScreenCoord,
                        0
                    ).xyz;
                    let worldPositionDirection: vec3<f32> = sampleToRayPositionDiff / fWorldPositionDistance;
                    let fDP: f32 = max(dot(sampleNormal, worldPositionDirection * -1.0f), 0.0f);
                    let fDP1: f32 = max(dot(normal.xyz, rayDirection), 0.0f);
                    let fDenom: f32 = max(1.0f / (fWorldPositionDistance * fWorldPositionDistance), 0.2f);
                    let radiance: vec3<f32> =  sampleAlbedo * fDP1 * fDenom;
                    totalRadiance += radiance; 

                    let shOutput: SHOutput = encodeSphericalHarmonics(
                        radiance,
                        worldPositionDirection, 
                        sphericalHarmonicsOutput.mSphericalHarmonicsCoefficient0,
                        sphericalHarmonicsOutput.mSphericalHarmonicsCoefficient1,
                        sphericalHarmonicsOutput.mSphericalHarmonicsCoefficient2);
                    sphericalHarmonicsOutput = shOutput;

                    ret.mfNumValidSamples += 1.0f;
                    ret.mfNumSamples += 1.0f;
                    bHit = true;
                }

                break;
            }

            fT += fStepSize;

        }   // for ray step = 0 to num ray steps

    }   // for ray = 0 to num rays

    totalRadiance.x /= f32(kiNumRays);
    totalRadiance.y /= f32(kiNumRays);
    totalRadiance.z /= f32(kiNumRays);

    ret.mSphericalHarmonicsCoefficient0 = sphericalHarmonicsOutput.mSphericalHarmonicsCoefficient0;
    ret.mSphericalHarmonicsCoefficient1 = sphericalHarmonicsOutput.mSphericalHarmonicsCoefficient1;
    ret.mSphericalHarmonicsCoefficient2 = sphericalHarmonicsOutput.mSphericalHarmonicsCoefficient2;

    ret.mIndirectDiffuseRadiance = totalRadiance;
    ret.mfNumOccluded = fNumOccluded;

    return ret;
}

/*
**
*/
fn ssgi2(
    worldPosition: vec3<f32>,
    normal: vec3<f32>,
    uv: vec2<f32>,
    prevSphericalHarmonics: SHOutput,
    screenSize: vec2<i32>,
    sampleTextureSize: vec2<i32>,
    blueNoiseTextureSize: vec2<u32>,
    fNumSlices: f32,
    fNumSections: f32,
    iFrame: i32,
    fRadius: f32) -> SSGIResult
{
    let TWO_PI: f32 = 2.0f * 3.14159f;

    let screenCoord: vec2<i32> = vec2<i32>(
        i32(uv.x * f32(screenSize.x)),
        i32(uv.y * f32(screenSize.y))
    );
    
    let centerSampleScreenCoord: vec2<i32> = vec2<i32>(
        i32(uv.x * f32(sampleTextureSize.x)),
        i32(uv.y * f32(sampleTextureSize.y))
    );

    let sampleScale: vec2<f32> = vec2<f32>(
        f32(sampleTextureSize.x) / f32(screenSize.x),
        f32(sampleTextureSize.y) / f32(screenSize.y)
    );

    var sphericalHarmonicsOutput: SHOutput = prevSphericalHarmonics;

    var ret: SSGIResult;
    ret.mfNumValidSamples = 0.0f;

    let kiNumSlices: i32 = i32(fNumSlices);
    let kiNumSections: i32 = i32(fNumSections);
    let iHalfNumSections: i32 = kiNumSections / 2;

    //let blueNoiseUV: vec2<f32> = getBlueNoiseUV(
    //    uv * vec2<f32>(f32(sampleTextureSize.x), f32(sampleTextureSize.y)) + vec2<f32>(f32(defaultUniformBuffer.miFrame), 0.0f),
    //    defaultUniformBuffer.miFrame,
    //    sampleTextureSize
    //);

    let blueNoiseUV: vec2<f32> = blueNoiseSample(uv, blueNoiseTextureSize);

    let blueNoise: vec2<f32> = textureLoad(
        blueNoiseTexture,
        vec2<i32>(
            i32(blueNoiseUV.x * f32(blueNoiseTextureSize.x)),
            i32(blueNoiseUV.y * f32(blueNoiseTextureSize.y))
        ),
        0
    ).xy;
    let fSampleRadius: f32 = fRadius * blueNoise.y;
    var fSampleSectionStep: f32 = fSampleRadius / f32(iHalfNumSections);

    var totalRadiance: vec3<f32> = vec3<f32>(0.0f, 0.0f, 0.0f);
    var fValidSampleCount: f32 = 0.0f;
    var fNumOccluded: f32 = 0.0f;
    var fNumSamples: f32 = 0.0f;
    for(var iSlice: i32 = 0; iSlice < kiNumSlices; iSlice++)
    {
        let sliceBlueNoiseUV: vec2<f32> = getBlueNoiseUV(
            uv * vec2<f32>(f32(sampleTextureSize.x), f32(sampleTextureSize.y)) + vec2<f32>(f32(iSlice), 0.0f),
            iFrame,
            sampleTextureSize
        );

        // slice direction
        let blueNoise: vec2<f32> = textureLoad(
            blueNoiseTexture,
            vec2<i32>(
                i32(sliceBlueNoiseUV.x * f32(blueNoiseTextureSize.x)),
                i32(sliceBlueNoiseUV.y * f32(blueNoiseTextureSize.y))
            ),
            0
        ).xy;
        let fAngle: f32 = blueNoise.x * TWO_PI;
        let sampleDirection: vec2<f32> = vec2<f32>(
            cos(fAngle),
            sin(fAngle)
        );
        
        fSampleSectionStep = fSampleSectionStep * sliceBlueNoiseUV.y;

        // sections
        for(var iSection: i32 = -iHalfNumSections; iSection < iHalfNumSections; iSection++)
        {
            // move along the rotated slice
            let sampleScreenCoord: vec2<i32> = vec2<i32>(
                clamp(i32(f32(centerSampleScreenCoord.x) + sampleDirection.x * fSampleSectionStep * f32(iSection)), 0, i32(sampleTextureSize.x - 1)),
                clamp(i32(f32(centerSampleScreenCoord.y) + sampleDirection.y * fSampleSectionStep * f32(iSection)), 0, i32(sampleTextureSize.y - 1))
            );
            if(sampleScreenCoord.x == screenCoord.x &&
               sampleScreenCoord.y == screenCoord.y)
            {
                continue;
            }


            let sampleWorldPosition: vec4<f32> = textureLoad(
                worldPositionTexture,
                sampleScreenCoord,
                0
            );

            let sampleWorldDiff: vec3<f32> = sampleWorldPosition.xyz - worldPosition;
            let fDistance: f32 = length(sampleWorldDiff);
            
            let sampleWorldDirection: vec3<f32> = sampleWorldDiff / fDistance;
            let fDP: f32 = dot(sampleWorldDirection, normal);
            if(fDP <= 0.0f)
            {
                continue;
            }

            let sampleNormal: vec4<f32> = textureLoad(
                normalTexture,
                sampleScreenCoord,
                0
            );

            // cosine angle of direction to sample world position and normal
            let fDP1: f32 = max(dot(sampleNormal.xyz, sampleWorldDirection * -1.0f), 0.0f);
            let sampleAlbedo: vec4<f32> = textureLoad(
                albedoTexture,
                sampleScreenCoord,
                0
            );

            // within distance to record indirect lighting
            if(fDistance >= 0.01f && fDistance <= 3.0f)
            {
                let fOneOverDistanceSquared: f32 = min(1.0f / (fDistance * fDistance), 1.0f);
                let radiance: vec3<f32> = (sampleAlbedo.xyz * fDP) * fOneOverDistanceSquared;
                totalRadiance += radiance;
                fValidSampleCount += 1.0f;

                // encode and add to total spherical harmonics coefficients 
                let shOutput: SHOutput = encodeSphericalHarmonics(
                    radiance,
                    sampleWorldDirection, 
                    sphericalHarmonicsOutput.mSphericalHarmonicsCoefficient0,
                    sphericalHarmonicsOutput.mSphericalHarmonicsCoefficient1,
                    sphericalHarmonicsOutput.mSphericalHarmonicsCoefficient2);
                sphericalHarmonicsOutput = shOutput;

                ret.mfNumValidSamples += 1.0f;
            }

            if(fDistance <= 1.0f && fDP >= 0.6f)
            {
                ret.mfNumOccluded += 1.0f;
            }

            fNumSamples += 1.0f;
        
        }   // for section = -half num sections to half num sections

    }   // for slice = 0 to num slices

    ret.mSphericalHarmonicsCoefficient0 = sphericalHarmonicsOutput.mSphericalHarmonicsCoefficient0;
    ret.mSphericalHarmonicsCoefficient1 = sphericalHarmonicsOutput.mSphericalHarmonicsCoefficient1;
    ret.mSphericalHarmonicsCoefficient2 = sphericalHarmonicsOutput.mSphericalHarmonicsCoefficient2;

    ret.mIndirectDiffuseRadiance = totalRadiance;
    ret.mfNumOccluded += fNumOccluded;
    ret.mfNumSamples = fNumSamples;

    return ret;
}

/*
**
*/
fn blueNoiseSample(
    uv: vec2<f32>,
    blueNoiseTextureSize: vec2<u32>) -> vec2<f32> 
{
    var noiseTexCoord: vec2<i32> = vec2<i32>(
        i32(uv.x * f32(blueNoiseTextureSize.x)),
        i32(uv.y * f32(blueNoiseTextureSize.y)) 
    );

    noiseTexCoord.x = (noiseTexCoord.x + defaultUniformBuffer.miFrame) % i32(blueNoiseTextureSize.x);
    noiseTexCoord.y = (noiseTexCoord.y + (defaultUniformBuffer.miFrame / i32(blueNoiseTextureSize.x))) % i32(blueNoiseTextureSize.x);

    let noise: vec2<f32> = textureLoad(
        blueNoiseTexture, 
        noiseTexCoord,
        0).xy;

    return noise; 
}

/*
**  Cosine sampling
*/
fn sampleHemisphere(
    normal: vec3<f32>, 
    fRand0: f32,
    fRand1: f32) -> vec3<f32> 
{
    let fPhi: f32 = 2.0f * 3.141592f * fRand0;
    let fCosTheta: f32 = sqrt(1.0f - fRand1);
    let fSinTheta: f32 = sqrt(fRand1);

    var up: vec3<f32> = vec3<f32>(0.0f, 1.0f, 0.0f);
    if(abs(normal.y) >= 0.99f)
    {
        up = vec3<f32>(1.0f, 0.0f, 0.0f);
    }

    let tangent: vec3<f32> = normalize(cross(normal, up));
    let bitangent: vec3<f32> = cross(normal, tangent);

    return normalize(cos(fPhi) * fSinTheta * tangent +
                     sin(fPhi) * fSinTheta * bitangent +
                     fCosTheta * normal);
}

/*
**
*/
fn encodeSphericalHarmonics(
    radiance: vec3<f32>,
    direction: vec3<f32>,
    SHCoefficient0: vec4<f32>,
    SHCoefficient1: vec4<f32>,
    SHCoefficient2: vec4<f32>
) -> SHOutput
{
    var output: SHOutput;

    output.mSphericalHarmonicsCoefficient0 = SHCoefficient0;
    output.mSphericalHarmonicsCoefficient1 = SHCoefficient1;
    output.mSphericalHarmonicsCoefficient2 = SHCoefficient2;

    let fDstPct: f32 = 1.0f;
    let fSrcPct: f32 = 1.0f;

    let afC: vec4<f32> = vec4<f32>(
        0.282095f,
        0.488603f,
        0.488603f,
        0.488603f
    );

    let A: vec4<f32> = vec4<f32>(
        0.886227f,
        1.023326f,
        1.023326f,
        1.023326f
    );

    // encode coefficients with direction
    let coefficient: vec4<f32> = vec4<f32>(
        afC.x * A.x,
        afC.y * direction.y * A.y,
        afC.z * direction.z * A.z,
        afC.w * direction.x * A.w
    );

    // encode with radiance
    var aResults: array<vec3<f32>, 4>;
    aResults[0] = radiance.xyz * coefficient.x * fDstPct;
    aResults[1] = radiance.xyz * coefficient.y * fDstPct;
    aResults[2] = radiance.xyz * coefficient.z * fDstPct;
    aResults[3] = radiance.xyz * coefficient.w * fDstPct;
    output.mSphericalHarmonicsCoefficient0.x += aResults[0].x;
    output.mSphericalHarmonicsCoefficient0.y += aResults[0].y;
    output.mSphericalHarmonicsCoefficient0.z += aResults[0].z;
    output.mSphericalHarmonicsCoefficient0.w += aResults[1].x;

    output.mSphericalHarmonicsCoefficient1.x += aResults[1].y;
    output.mSphericalHarmonicsCoefficient1.y += aResults[1].z;
    output.mSphericalHarmonicsCoefficient1.z += aResults[2].x;
    output.mSphericalHarmonicsCoefficient1.w += aResults[2].y;

    output.mSphericalHarmonicsCoefficient2.x += aResults[2].z;
    output.mSphericalHarmonicsCoefficient2.y += aResults[3].x;
    output.mSphericalHarmonicsCoefficient2.z += aResults[3].y;
    output.mSphericalHarmonicsCoefficient2.w += aResults[3].z;

    return output;
}

/*
**
*/
fn decodeFromSphericalHarmonicCoefficients(
    ssgiResult: SSGIResult,
    direction: vec3<f32>,
    maxRadiance: vec3<f32>,
    fHistoryCount: f32
) -> vec3<f32>
{
    var SHCoefficient0: vec4<f32> = ssgiResult.mSphericalHarmonicsCoefficient0;
    var SHCoefficient1: vec4<f32> = ssgiResult.mSphericalHarmonicsCoefficient1;
    var SHCoefficient2: vec4<f32> = ssgiResult.mSphericalHarmonicsCoefficient2;

    var aTotalCoefficients: array<vec3<f32>, 4>;
    let fFactor: f32 = 1.0f;

    aTotalCoefficients[0] = vec3<f32>(SHCoefficient0.x, SHCoefficient0.y, SHCoefficient0.z) * fFactor;
    aTotalCoefficients[1] = vec3<f32>(SHCoefficient0.w, SHCoefficient1.x, SHCoefficient1.y) * fFactor;
    aTotalCoefficients[2] = vec3<f32>(SHCoefficient1.z, SHCoefficient1.w, SHCoefficient2.x) * fFactor;
    aTotalCoefficients[3] = vec3<f32>(SHCoefficient2.y, SHCoefficient2.z, SHCoefficient2.w) * fFactor;

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
fn octahedronMap2(
    direction: vec3<f32>) -> vec2<f32>
{
    let fDP: f32 = dot(vec3<f32>(1.0f, 1.0f, 1.0f), abs(direction));
    let newDirection: vec3<f32> = vec3<f32>(direction.x, direction.z, direction.y) / fDP;

    var ret: vec2<f32> = vec2<f32>(
        (1.0f - abs(newDirection.z)) * sign(newDirection.x),
        (1.0f - abs(newDirection.x)) * sign(newDirection.z));

    if (newDirection.y < 0.0f)
    {
        ret = vec2<f32>(
            newDirection.x,
            newDirection.z);
    }

    ret = ret * 0.5f + vec2<f32>(0.5f, 0.5f);
    ret.y = 1.0f - ret.y;

    return ret;
}

/*
**
*/
fn isPointOnRay(
    rayOrigin: vec3<f32>, 
    rayDirection: vec3<f32>, 
    point: vec3<f32>) -> bool
{
    let v: vec3<f32> = point - rayOrigin;

    // Check if V and D are collinear (V x D == 0)
    let crossVD: vec3<f32> = cross(v, rayDirection);
    if (dot(crossVD, crossVD) >= 1.0e-4f) 
    {
        return false; // Not on the same line
    }

    // Check if point lies in the direction of the ray: V • D >= 0
    let fDotVD: f32 = dot(v, rayDirection);
    if(fDotVD < -1.0e-4f) 
    {
        return false; // Behind the ray origin
    }

    return true;
}

/*
**
*/
fn hash22(p: vec2<f32>) -> vec2<f32> 
{
    var pCopy: vec2<f32> = p;
    pCopy = vec2<f32>(
        dot(p, vec2<f32>(127.1f, 311.7f)),
        dot(p, vec2<f32>(269.5f, 183.3f))
    );
    return fract(sin(pCopy) * 43758.5453f);
}

/*
**
*/
fn getBlueNoiseUV(
    pixelCoord: vec2<f32>, 
    frame: i32,
    textureSize: vec2<i32>) -> vec2<f32> 
{
    // Hash to get a pseudo-random offset per pixel
    let jitter: vec2<f32> = hash22(pixelCoord + f32(frame));

    // Wrap to texture resolution (assuming tiling)
    let texSize: vec2<f32> = vec2<f32>(f32(textureSize.x), f32(textureSize.y));
    
    let jitteredScreenCoord: vec2<i32> = vec2<i32>(
        i32(pixelCoord.x + jitter.x * texSize.x),
        i32(pixelCoord.y + jitter.y * texSize.y)
    );
    
    let noiseCoord: vec2<f32> = 
        vec2<f32>(
            f32(jitteredScreenCoord.x % i32(texSize.x)),
            f32(jitteredScreenCoord.y % i32(texSize.y))
        );

    return noiseCoord / texSize; // normalize to [0, 1] UV
}