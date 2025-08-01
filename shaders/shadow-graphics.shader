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

struct UniformData
{
    maLightViewProjectionMatrices: array<mat4x4<f32>, 3>,
    mDecalViewProjectionMatrix: mat4x4<f32>,
};

@group(0) @binding(0)
var worldPositionTexture: texture_2d<f32>;

@group(0) @binding(1)
var lightViewClipTexture0: texture_2d<f32>;

@group(0) @binding(2)
var lightViewClipTexture1: texture_2d<f32>;

@group(0) @binding(3)
var lightViewClipTexture2: texture_2d<f32>;

@group(0) @binding(4)
var lightViewMomentTexture0: texture_2d<f32>;

@group(0) @binding(5)
var lightViewMomentTexture1: texture_2d<f32>;

@group(0) @binding(6)
var lightViewMomentTexture2: texture_2d<f32>;

@group(1) @binding(0)
var<storage> uniformBuffer: UniformData;

@group(1) @binding(1)
var decalTexture: texture_2d<f32>;

@group(1) @binding(2)
var<uniform> defaultUniformBuffer: DefaultUniformData;

@group(1) @binding(3)
var textureSampler: sampler;

struct VertexOutput 
{
    @builtin(position) pos: vec4f,
    @location(0) uv: vec2f,
};

struct FragmentOutput 
{
    @location(0) mOutput : vec4<f32>,
    @location(1) mDecal: vec4<f32>,
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
    let worldPosition: vec4f = textureLoad(
        worldPositionTexture,
        screenCoord,
        0
    );

    var fRet0: f32 = getShadow(
        vec4<f32>(worldPosition.xyz, 1.0f), 
        0
    );
    var fRet1: f32 = getShadow(
        vec4<f32>(worldPosition.xyz, 1.0f), 
        1
    );
    //var fRet2: f32 = getShadow(
    //    vec4<f32>(worldPosition.xyz, 1.0f), 
    //    2
    //);
    //var fRet: f32 = min(fRet0, min(fRet1, fRet2));
    var fRet: f32 = min(fRet0, fRet1);

    out.mOutput = vec4<f32>(fRet, fRet, fRet, 1.0f);

    out.mDecal = drawDecal(
        vec4<f32>(worldPosition.xyz, 1.0f),
        in.uv
    ) * fRet;

    return out;
}

/*
**
*/
fn drawDecal(
    worldPosition: vec4<f32>,
    uv: vec2<f32>) -> vec4<f32>
{
    var decalPosition: vec4<f32> = vec4<f32>(worldPosition.xyz, 1.0f) * uniformBuffer.mDecalViewProjectionMatrix;
    decalPosition = decalPosition * 0.5f + 0.5f;
    decalPosition.y = 1.0f - decalPosition.y;

    var decal: vec4<f32> = textureSample(
        decalTexture,
        textureSampler,
        decalPosition.xy
    );

    var ret: vec4<f32> = decal;
    if(decalPosition.x < 0.0f || decalPosition.x > 1.0f || 
       decalPosition.y < 0.0f || decalPosition.y > 1.0f || 
       decalPosition.z < 0.0f || decalPosition.z > 1.0f || 
       worldPosition.w <= 0.0f)
    {
        ret = vec4<f32>(0.0f, 0.0f, 0.0f, 0.0f);
    }

    return ret;
}

/*
**
*/
fn getShadow(
    worldPosition: vec4<f32>,
    iCascade: u32) -> f32
{
    var lightClipSpace: vec4<f32> = vec4<f32>(worldPosition.x, worldPosition.y, worldPosition.z, 1.0f) * uniformBuffer.maLightViewProjectionMatrices[iCascade];
    lightClipSpace.x = lightClipSpace.x * 0.5f + 0.5f;
    lightClipSpace.y = 1.0f - (lightClipSpace.y * 0.5f + 0.5f);
    var lightViewScreenCoord: vec2<i32> = vec2<i32>(
        i32(f32(defaultUniformBuffer.miScreenWidth) * lightClipSpace.x),
        i32(f32(defaultUniformBuffer.miScreenHeight) * lightClipSpace.y) 
    );
    var sampleLightViewClipCoord: vec4<f32> = vec4<f32>(0.0f, 0.0f, 0.0f, 0.0f); 
    var sampleLightViewMoment: vec4<f32> = vec4<f32>(0.0f, 0.0f, 0.0f, 0.0f); 
    if(iCascade == 0u)
    {
        sampleLightViewClipCoord = textureLoad(
            lightViewClipTexture0,
            lightViewScreenCoord,
            0
        );

        sampleLightViewMoment = textureLoad(
            lightViewMomentTexture0,
            lightViewScreenCoord,
            0
        );
    }
    else if(iCascade == 1u)
    {
        sampleLightViewClipCoord = textureLoad(
            lightViewClipTexture1,
            lightViewScreenCoord,
            0
        );

        sampleLightViewMoment = textureLoad(
            lightViewMomentTexture1,
            lightViewScreenCoord,
            0
        );
    }
    else if(iCascade == 2u)
    {
        sampleLightViewClipCoord = textureLoad(
            lightViewClipTexture2,
            lightViewScreenCoord,
            0
        );

        sampleLightViewMoment = textureLoad(
            lightViewMomentTexture2,
            lightViewScreenCoord,
            0
        );
    }

    var fRet: f32 = 1.0f;
    if(sampleLightViewClipCoord.w <= 0.0f)
    {
        fRet = 1.0f;
    }
    else if(
        lightClipSpace.x >= 0.0f && lightClipSpace.x <= 1.0f && 
        lightClipSpace.y >= 0.0f && lightClipSpace.y <= 1.0f)
    {
        //fRet = 0.4f;
        fRet = max(chebyshevUpperBound(
            sampleLightViewMoment.xy,
            lightClipSpace.z
        ), 
        0.3f);
    }

    return fRet;
}

/*
**
*/
fn orthographicProjection(
    fLeft : f32,
    fRight : f32,
    fTop : f32,
    fBottom : f32,
    fFar : f32,
    fNear : f32) -> mat4x4<f32>
{
    let fWidth: f32 = fRight - fLeft;
    let fHeight: f32 = fTop - fBottom;

    let fFarMinusNear: f32 = fFar - fNear;

    var ret: mat4x4<f32>;

    ret[0][0] = -2.0f / fWidth;
    ret[0][1] = 0.0f;
    ret[0][2] = 0.0f;
    ret[0][3] = -(fRight + fLeft) / (fRight - fLeft);
    
    ret[1][0] = 0.0f;
    ret[1][1] = 2.0f / fHeight;
    ret[1][2] = 0.0f;
    ret[1][3] = -(fTop + fBottom) / (fTop - fBottom);

    ret[2][0] = 0.0f;
    ret[2][1] = 0.0f;
    ret[2][2] = -1.0f / fFarMinusNear;
    ret[2][3] = -fNear / fFarMinusNear;

    ret[3][0] = 0.0f;
    ret[3][1] = 0.0f;
    ret[3][2] = 0.0f;
    ret[3][3] = 1.0f;

    ret[0][0] = -ret[0][0];

    return ret;
}

/*
**
*/
fn makeViewMatrix(
    eyePos: vec3<f32>, 
    lookAt: vec3<f32>, 
    up: vec3<f32>) -> mat4x4<f32>
{
    var dir: vec3<f32> = lookAt - eyePos;
    dir = normalize(dir);
    
    var tangent: vec3<f32> = normalize(cross(up, dir));
    var binormal: vec3<f32> = normalize(cross(dir, tangent));
    
    var xform: mat4x4<f32>;
    xform[0][0] = tangent.x; xform[0][1] = tangent.y; xform[0][2] = tangent.z; xform[0][3] = 0.0f;
    xform[1][0] = binormal.x; xform[1][1] = binormal.y; xform[1][2] = binormal.z; xform[1][3] = 0.0f;
    xform[2][0] = -dir.x; xform[2][1] = -dir.y; xform[2][2] = -dir.z; xform[2][3] = 0.0f;
    xform[3][0] = 0.0f; xform[3][1] = 0.0f; xform[3][2] = 0.0f; xform[3][3] = 1.0f;
    
    var translation: mat4x4<f32>;
    translation[0][0] = 1.0f; translation[0][1] = 0.0f; translation[0][2] = 0.0f; translation[0][3] = -eyePos.x;
    translation[1][0] = 0.0f; translation[1][1] = 1.0f; translation[1][2] = 0.0f; translation[1][3] = -eyePos.y;
    translation[2][0] = 0.0f; translation[2][1] = 0.0f; translation[2][2] = 1.0f; translation[2][3] = -eyePos.z;
    translation[3][0] = 0.0f; translation[3][1] = 0.0f; translation[3][2] = 0.0f; translation[3][3] = 1.0f;
    
    
    return (xform * translation);
}

/*
**
*/
fn chebyshevUpperBound(
    moments: vec2<f32>, 
    fT: f32) -> f32
{
    // t = the test depth
    let fMean: f32 = moments.x;
    let fMeanSq: f32 = moments.y;
    var fVariance: f32 = fMeanSq - (fMean * fMean);
    fVariance = max(fVariance, 0.0001f); // Prevent division by zero

    let fD: f32 = fT - fMean;
    let fP: f32 = fVariance / (fVariance + fD * fD);

    var fRet: f32 = fP;
    if(fT <= fMean)
    {
        fRet = 1.0f;
    }

    // If test depth is in front of the surface depth, it's fully lit
    return fRet;
}