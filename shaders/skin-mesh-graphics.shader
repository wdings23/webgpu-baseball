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

@group(1) @binding(2)
var<storage> aiJointInfluenceIndices: array<u32>;

@group(1) @binding(3)
var<storage> afJointInfluenceWeights: array<f32>;

@group(1) @binding(4)
var<storage> aJointGlobalBindMatrices: array<mat4x4<f32>>;

@group(1) @binding(5)
var<storage> aJointInverseGlobalBindMatrices: array<mat4x4<f32>>;

@group(1) @binding(6)
var<storage> aJointAnimationTotalMatrices: array<mat4x4<f32>>;

@group(1) @binding(7)
var diffuseTextureAtlas: texture_2d<f32>;

@group(1) @binding(8)
var<uniform> defaultUniformBuffer: DefaultUniformData;

@group(1) @binding(9)
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
    @location(0) worldPosition: vec4<f32>,
    @location(1) texCoord: vec4<f32>,
    @location(2) normal: vec4<f32>,

    @location(3) lightViewPosition: vec4<f32>,
};
struct FragmentOutput 
{
    @location(0) worldPosition : vec4<f32>,
    @location(1) normal: vec4<f32>,
    @location(2) texCoordAndClipSpace: vec4<f32>,
    @location(3) motionVector: vec4<f32>,
    @location(4) mColorOutput: vec4<f32>,
    @location(5) mMask: vec3<f32>,
};

@vertex
fn vs_main(in: VertexInput,
    @builtin(vertex_index) iVertexIndex: u32) -> VertexOutput 
{
    var out: VertexOutput;
    
    let iMesh: u32 = u32(ceil(animMeshUniformBuffer.mExtraInfo.x - 0.5f));

    // influence joint indices
    var aiJointInfluence: array<u32, 4>;
    aiJointInfluence[0] = aiJointInfluenceIndices[iVertexIndex * 4] + iMesh * 65;
    aiJointInfluence[1] = aiJointInfluenceIndices[iVertexIndex * 4 + 1] + iMesh * 65;
    aiJointInfluence[2] = aiJointInfluenceIndices[iVertexIndex * 4 + 2] + iMesh * 65;
    aiJointInfluence[3] = aiJointInfluenceIndices[iVertexIndex * 4 + 3] + iMesh * 65;

    // joint influence weights
    var afJointInfluenceWeight: array<f32, 4>;
    afJointInfluenceWeight[0] = afJointInfluenceWeights[iVertexIndex * 4];
    afJointInfluenceWeight[1] = afJointInfluenceWeights[iVertexIndex * 4 + 1];
    afJointInfluenceWeight[2] = afJointInfluenceWeights[iVertexIndex * 4 + 2];
    afJointInfluenceWeight[3] = afJointInfluenceWeights[iVertexIndex * 4 + 3];

    // joint matrices
    var xformMatrix0: mat4x4<f32> = aJointAnimationTotalMatrices[aiJointInfluence[0]];
    var xformMatrix1: mat4x4<f32> = aJointAnimationTotalMatrices[aiJointInfluence[1]];
    var xformMatrix2: mat4x4<f32> = aJointAnimationTotalMatrices[aiJointInfluence[2]];
    var xformMatrix3: mat4x4<f32> = aJointAnimationTotalMatrices[aiJointInfluence[3]];

    var worldPosition: vec4<f32> = vec4<f32>(
        in.worldPosition.x,
        in.worldPosition.y,
        in.worldPosition.z,
        1.0f
    );

    // skinned position
    let skinnedPos: vec4<f32> = 
        worldPosition * xformMatrix0 * afJointInfluenceWeight[0] +
        worldPosition * xformMatrix1 * afJointInfluenceWeight[1] + 
        worldPosition * xformMatrix2 * afJointInfluenceWeight[2] + 
        worldPosition * xformMatrix3 * afJointInfluenceWeight[3];
    
    let skinnedNormal: vec4<f32> = 
        in.normal * xformMatrix0 * afJointInfluenceWeight[0] +
        in.normal * xformMatrix1 * afJointInfluenceWeight[1] + 
        in.normal * xformMatrix2 * afJointInfluenceWeight[2] + 
        in.normal * xformMatrix3 * afJointInfluenceWeight[3];

    //let iMesh: u32 = u32(ceil(in.texCoord.z - 0.5f));
    
    out.worldPosition = vec4<f32>(skinnedPos.xyz * 10.0f, 1.0f) * animMeshUniformBuffer.mModelMatrix;
    out.pos = out.worldPosition * defaultUniformBuffer.mJitteredViewProjectionMatrix;
    out.texCoord = vec4f(in.texCoord.x, in.texCoord.y, f32(iMesh), 1.0f);
    out.normal = skinnedNormal;

    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> FragmentOutput 
{
    var out: FragmentOutput;
    
    let iMesh: u32 = 1u;
    
    out.worldPosition = vec4<f32>(in.worldPosition.xyz, f32(iMesh));
    let normalXYZ: vec3<f32> = normalize(in.normal.xyz);
    out.normal.x = normalXYZ.x;
    out.normal.y = normalXYZ.y;
    out.normal.z = normalXYZ.z;
    out.normal.w = 1.0f;

    var texCoord: vec4<f32> = vec4<f32>(
        in.texCoord.xy,
        0.0f,
        0.0f);
    if(texCoord.x < 0.0f)
    {
        texCoord.x = fract(abs(1.0f - texCoord.x));
    }
    else if(texCoord.x > 1.0f)
    {
        texCoord.x = fract(texCoord.x);
    }

    if(texCoord.y < 0.0f)
    {
        texCoord.y = fract(abs(1.0f - texCoord.y));
    }
    else if(texCoord.y > 1.0f)
    {
        texCoord.y = fract(texCoord.y);
    }

    out.texCoordAndClipSpace.x = texCoord.x;
    out.texCoordAndClipSpace.y = texCoord.y;

    // store depth and mesh id in worldPosition.w
    out.worldPosition.w = clamp(in.pos.z, 0.0f, 0.999f) + floor(in.worldPosition.w + 0.5f);

    let currClipSpace: vec4<f32> = vec4<f32>(in.worldPosition.xyz, 1.0f) * defaultUniformBuffer.mJitteredViewProjectionMatrix;
    let prevClipSpace: vec4<f32> = vec4<f32>(in.worldPosition.xyz, 1.0f) * defaultUniformBuffer.mPrevViewProjectionMatrix;

    var currClipSpacePos: vec3<f32> = vec3<f32>(
        currClipSpace.x / currClipSpace.w,
        currClipSpace.y / currClipSpace.w,
        currClipSpace.z / currClipSpace.w
    ) * 0.5f + 
    vec3<f32>(0.5f);
    currClipSpacePos.y = 1.0f - currClipSpacePos.y;

    var prevClipSpacePos: vec3<f32> = vec3<f32>(
        prevClipSpace.x / prevClipSpace.w,
        prevClipSpace.y / prevClipSpace.w,
        prevClipSpace.z / prevClipSpace.w
    ) * 0.5f + 
    vec3<f32>(0.5f);
    prevClipSpacePos.y = 1.0f - prevClipSpacePos.y;

    out.texCoordAndClipSpace.z = currClipSpacePos.x;
    out.texCoordAndClipSpace.w = currClipSpacePos.y;

    out.motionVector.x = (currClipSpacePos.x - prevClipSpacePos.x);
    out.motionVector.y = (currClipSpacePos.y - prevClipSpacePos.y);
    out.motionVector.z = floor(in.worldPosition.w + 0.5f);      // mesh id
    out.motionVector.w = currClipSpacePos.z;                    // depth

    var xform: vec4<f32> = vec4<f32>(in.worldPosition.xyz, 1.0f) * defaultUniformBuffer.mJitteredViewProjectionMatrix;
    var fDepth: f32 = xform.z / xform.w;
    out.normal = vec4<f32>(normalXYZ, 1.0f);
    
    let lightDir: vec3f = normalize(vec3f(1.0f, -1.0f, 1.0f));
    let fDP: f32 = max(dot(normalXYZ, lightDir), 0.3f);
    //out.mMaterial = vec4f(aMaterials[iMesh].mDiffuse.xyz * fDP, 1.0f);

    // encode clip space 
    out.normal.w = currClipSpacePos.z;

    let fLightDP: f32 = max(dot(normalXYZ, normalize(vec3<f32>(1.0f, 1.0f, -1.0f))), 0.0f);
    
    let diffuseAtlasTextureSize: vec2u = textureDimensions(diffuseTextureAtlas);
    let atlasPct: vec2f = vec2f(
        animMeshUniformBuffer.mTextureAtlasInfo.z / f32(diffuseAtlasTextureSize.x),
        animMeshUniformBuffer.mTextureAtlasInfo.w / f32(diffuseAtlasTextureSize.y)
    );
    let textureUV: vec2f = animMeshUniformBuffer.mTextureAtlasInfo.xy + vec2f(in.texCoord.x, in.texCoord.y) * atlasPct.xy;
    out.mColorOutput = textureSample(
        diffuseTextureAtlas,
        textureSampler,
        textureUV
    );

    out.mMask = vec4<f32>(1.0f, 1.0f, 1.0f, 1.0f);

    return out;
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