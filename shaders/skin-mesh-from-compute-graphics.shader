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

struct TextureAtlasInfo
{
    miTextureCoord: vec2i,
    mUV: vec2f,
    miTextureID: u32,
    miImageWidth: u32,
    miImageHeight: u32,
    miPadding0: u32,
};

struct AnimMeshModelUniform
{
    mModelMatrix: mat4x4<f32>,
    mLightViewProjectionMatrix: mat4x4<f32>,
    mTextureAtlasInfo: vec4<f32>,
    mExtraInfo: vec4<f32>,
};

struct Vertex
{
    mPosition: vec4<f32>,
    mTexCoord: vec4<f32>,
    mNormal: vec4<f32>,
};

struct MeshVertexRange
{
    miStart: u32,
    miEnd: u32,
};

@group(0) @binding(0)
var<storage, read> previousSkinMeshVertices: array<Vertex>;

@group(1) @binding(0)
var<storage> aMaterials: array<Material>;

@group(1) @binding(1)
var<storage> aMeshMaterialID: array<u32>;

@group(1) @binding(2)
var<storage, read> diffuseTextureAtlasInfoBuffer: array<TextureAtlasInfo>;

@group(1) @binding(3)
var diffuseTextureAtlas: texture_2d<f32>;

@group(1) @binding(4)
var<storage, read> aMeshVertexRanges: array<MeshVertexRange>;

@group(1) @binding(5)
var<uniform> defaultUniformBuffer: DefaultUniformData;

@group(1) @binding(6)
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
    @location(3) prevVertexPosition: vec4<f32>,
};
struct FragmentOutput 
{
    @location(0) worldPosition : vec4<f32>,
    @location(1) normal: vec4<f32>,
    @location(2) texCoordAndClipSpace: vec4<f32>,
    @location(3) motionVector: vec4<f32>,
    @location(4) mAlbedo: vec4<f32>,
    @location(5) mMask: vec4<f32>,
};

@vertex
fn vs_main(in: VertexInput,
    @builtin(vertex_index) iVertexIndex: u32,
    @builtin(instance_index) iInstanceIndex: u32) -> VertexOutput 
{
    var out: VertexOutput;
    
    let iMesh: u32 = u32(ceil(animMeshUniformBuffer.mExtraInfo.x - 0.5f));
    let animMeshMatrix: mat4x4<f32> = animMeshUniformBuffer.mModelMatrix;
    let totalMatrix: mat4x4<f32> = animMeshMatrix * defaultUniformBuffer.mJitteredViewProjectionMatrix;

    var rotationMatrix: mat4x4<f32> = animMeshMatrix;
    rotationMatrix[0][3] = 0.0f;
    rotationMatrix[1][3] = 0.0f;
    rotationMatrix[2][3] = 0.0f;
    var normal: vec4<f32> = in.normal * rotationMatrix;

    out.pos = vec4<f32>(in.worldPosition.xyz * 10.0f, 1.0f) * totalMatrix;
    out.worldPosition = vec4<f32>(in.worldPosition.xyz * 10.0f, 1.0f) * animMeshMatrix;
    out.texCoord = vec4f(in.texCoord.x, in.texCoord.y, f32(iMesh), 1.0f);
    out.normal = normal;

    let iVertexRange: u32 = aMeshVertexRanges[iInstanceIndex].miStart;

    let prevVertexPosition: vec4<f32> = vec4<f32>(previousSkinMeshVertices[iVertexIndex].mPosition.xyz * 10.0f, 1.0f) * animMeshMatrix;
    out.prevVertexPosition = prevVertexPosition;

    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> FragmentOutput 
{
    var out: FragmentOutput;
    
    //let iMesh: u32 = u32(ceil(in.worldPosition.w - 0.5f));
    //let iMesh: u32 = u32(ceil(in.texCoord.z - 0.5f));
    let iMesh: u32 = u32(ceil(animMeshUniformBuffer.mExtraInfo.x - 0.5f));

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
    let prevClipSpace: vec4<f32> = vec4<f32>(in.prevVertexPosition.xyz, 1.0f) * defaultUniformBuffer.mPrevViewProjectionMatrix;

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

    out.motionVector.x = (prevClipSpacePos.x - currClipSpacePos.x);
    out.motionVector.y = (prevClipSpacePos.y - currClipSpacePos.y);
    out.motionVector.z = f32(iMesh);      // mesh id
    out.motionVector.w = linearizeDepth(currClipSpacePos.z, 1.0f, 100.0f);                    // depth

    var xform: vec4<f32> = vec4<f32>(in.worldPosition.xyz, 1.0f) * defaultUniformBuffer.mJitteredViewProjectionMatrix;
    var fDepth: f32 = xform.z / xform.w;
    //out.clipSpace = vec4<f32>(currClipSpacePos.xyz, 1.0f);
    out.normal = vec4<f32>(normalXYZ, 1.0f);
    
    let lightDir: vec3f = normalize(vec3f(1.0f, -1.0f, 1.0f));
    let fDP: f32 = max(dot(normalXYZ, lightDir), 0.3f);
    
    // albedo
    var albedo: vec4<f32> = vec4f(1.0f, 1.0f, 1.0f, 1.0f);
    let diffuseAtlasTextureSize: vec2u = textureDimensions(diffuseTextureAtlas);
    let atlasPct: vec2f = vec2f(
        animMeshUniformBuffer.mTextureAtlasInfo.z / f32(diffuseAtlasTextureSize.x),
        animMeshUniformBuffer.mTextureAtlasInfo.w / f32(diffuseAtlasTextureSize.y)
    );
    let textureUV: vec2f = animMeshUniformBuffer.mTextureAtlasInfo.xy + vec2f(in.texCoord.x, in.texCoord.y) * atlasPct.xy;
    out.mAlbedo = textureSample(
        diffuseTextureAtlas,
        textureSampler,
        textureUV
    );

    let fNear: f32 = 1.0f;
    let fFar: f32 = 100.0f;
    out.mMask = vec4<f32>(
        f32(iMesh) + 100000.0f, 
        //currClipSpacePos.x, 
        //currClipSpacePos.y, 
        //((fNear * fFar) / (fFar - currClipSpacePos.z * (fFar - fNear))) / fFar

        in.prevVertexPosition.x, 
        in.prevVertexPosition.y,
        in.prevVertexPosition.z
    );

    return out;
}

/*
**
*/
fn linearizeDepth(
    fDepth: f32,
    fNear: f32,
    fFar: f32) -> f32
{
    return ((fNear * fFar) / (fFar - fDepth * (fFar - fNear))) / fFar;
}