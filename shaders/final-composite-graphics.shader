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

@group(0) @binding(0)
var diffuseLightingTexture: texture_2d<f32>;

@group(0) @binding(1)
var specularLightingTexture: texture_2d<f32>;

@group(0) @binding(2)
var ambientLightingTexture: texture_2d<f32>;

@group(0) @binding(3)
var albedoTexture: texture_2d<f32>;

@group(0) @binding(4)
var ambientOcclusionOutputTexture: texture_2d<f32>;

@group(0) @binding(5)
var shadowOutputTexture: texture_2d<f32>;

@group(0) @binding(6)
var decalTexture: texture_2d<f32>;

@group(0) @binding(7)
var skyTexture: texture_2d<f32>;

@group(0) @binding(8)
var indirectLightingTexture: texture_2d<f32>;

@group(0) @binding(9)
var cloudTexture: texture_2d<f32>;

@group(0) @binding(10)
var sunLightTexture: texture_2d<f32>;

@group(1) @binding(0)
var<uniform> defaultUniformBuffer: DefaultUniformData;

@group(1) @binding(1)
var textureSampler: sampler;

struct VertexOutput 
{
    @builtin(position) pos: vec4f,
    @location(0) uv: vec2f,
};
struct FragmentOutput 
{
    @location(0) mColor: vec4<f32>,
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

    let diffuseLighting: vec4<f32> = textureSample(
        diffuseLightingTexture,
        textureSampler,
        in.uv
    );
    let specularLighting: vec4<f32> = textureSample(
        specularLightingTexture,
        textureSampler,
        in.uv
    );
    let ambientLighting: vec4<f32> = textureSample(
        ambientLightingTexture,
        textureSampler,
        in.uv
    );
    let albedo: vec4<f32> = textureSample(
        albedoTexture,
        textureSampler,
        in.uv
    );
    let ambientOcclusion: vec4<f32> = textureSample(
        ambientOcclusionOutputTexture,
        textureSampler,
        in.uv
    );
    let shadow: vec4<f32> = textureSample(
        shadowOutputTexture,
        textureSampler,
        in.uv
    );
    let decal: vec4<f32> = textureSample(
        decalTexture,
        textureSampler,
        in.uv
    );
    let indirectLighting: vec4<f32> = textureSample(
        indirectLightingTexture,
        textureSampler,
        in.uv
    );

    let totalColor: vec4<f32> = vec4<f32>(
        albedo.xyz * ambientOcclusion.xyz * shadow.xyz * diffuseLighting.xyz + ambientOcclusion.xyz * shadow.xyz * specularLighting.xyz + ambientLighting.xyz * albedo.xyz + indirectLighting.xyz * 2.0f,
        1.0f
    );

    
    out.mColor = vec4<f32>(
        totalColor.xyz * (1.0f - decal.w) + decal.xyz * decal.w,
        1.0f
    );

    if(albedo.w <= 0.0f)
    {
        let invViewProjectionMatrix: mat4x4<f32> = invert(defaultUniformBuffer.mViewProjectionMatrix);
        var clipToWorld: vec4<f32> = vec4<f32>(
            in.uv.x * 2.0f - 1.0f,
            (1.0f - in.uv.y) * 2.0f - 1.0f,
            1.0f, 
            1.0f) *
            invViewProjectionMatrix;
        clipToWorld.x /= clipToWorld.w;
        clipToWorld.y /= clipToWorld.w;
        clipToWorld.z /= clipToWorld.w;

        let skyTextureSize: vec2<u32> = textureDimensions(skyTexture);
        let direction: vec3<f32> = normalize(clipToWorld.xyz - defaultUniformBuffer.mCameraPosition.xyz);
        let skyUV: vec2<f32> = octahedronMap2(direction);
        let skyImageCoord: vec2<i32> = vec2<i32>(
            i32(skyUV.x * f32(skyTextureSize.x)),
            i32(skyUV.y * f32(skyTextureSize.y))
        );
        var skyColor: vec4<f32> = textureLoad(
            skyTexture,
            skyImageCoord,
            0
        );
        let cloudTextureSize: vec2<u32> = textureDimensions(cloudTexture);
        let cloudImageCoord: vec2<i32> = vec2<i32>(
            i32(in.uv.x * f32(cloudTextureSize.x)),
            i32(in.uv.y * f32(cloudTextureSize.y))
        );
        let cloudRadiance: vec4<f32> = textureLoad(
            cloudTexture,
            cloudImageCoord,
            0
        );
        let sunLight: vec4<f32> = textureLoad(
            sunLightTexture,
            vec2<i32>(0, 0),
            0
        );
        let fOneMinusDensity: f32 = 1.0f - clamp(cloudRadiance.w, 0.0f, 1.0f); 
        out.mColor = vec4<f32>(
            cloudRadiance.x * sunLight.x + fOneMinusDensity * skyColor.x,
            cloudRadiance.y * sunLight.y + fOneMinusDensity * skyColor.y,
            cloudRadiance.z * sunLight.z + fOneMinusDensity * skyColor.z,
            1.0f
        );
        
        //out.mColor = vec4<f32>(
        //    direction.xyz * 0.5f + 0.5f,
        //    1.0f
        //); 
    }
   
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

/*
**
*/
fn invert(m: mat4x4<f32>) -> mat4x4<f32>
{
    var inv: array<f32, 16>; 
    var invOut: array<f32, 16>; 
    var det: f32;
    var i: i32;
    
    inv[0] = m[1][1]  * m[2][2] * m[3][3] -
    m[1][1]  * m[2][3] * m[3][2] -
    m[2][1]  * m[1][2]  * m[3][3] +
    m[2][1]  * m[1][3]  * m[3][2] +
    m[3][1] * m[1][2]  * m[2][3] -
    m[3][1] * m[1][3]  * m[2][2];
    
    inv[4] = -m[1][0]  * m[2][2] * m[3][3] +
    m[1][0]  * m[2][3] * m[3][2] +
    m[2][0]  * m[1][2]  * m[3][3] -
    m[2][0]  * m[1][3]  * m[3][2] -
    m[3][0] * m[1][2]  * m[2][3] +
    m[3][0] * m[1][3]  * m[2][2];
    
    inv[8] = m[1][0]  * m[2][1] * m[3][3] -
    m[1][0]  * m[2][3] * m[3][1] -
    m[2][0]  * m[1][1] * m[3][3] +
    m[2][0]  * m[1][3] * m[3][1] +
    m[3][0] * m[1][1] * m[2][3] -
    m[3][0] * m[1][3] * m[2][1];
    
    inv[12] = -m[1][0]  * m[2][1] * m[3][2] +
    m[1][0]  * m[2][2] * m[3][1] +
    m[2][0]  * m[1][1] * m[3][2] -
    m[2][0]  * m[1][2] * m[3][1] -
    m[3][0] * m[1][1] * m[2][2] +
    m[3][0] * m[1][2] * m[2][1];
    
    inv[1] = -m[0][1]  * m[2][2] * m[3][3] +
    m[0][1]  * m[2][3] * m[3][2] +
    m[2][1]  * m[0][2] * m[3][3] -
    m[2][1]  * m[0][3] * m[3][2] -
    m[3][1] * m[0][2] * m[2][3] +
    m[3][1] * m[0][3] * m[2][2];
    
    inv[5] = m[0][0]  * m[2][2] * m[3][3] -
    m[0][0]  * m[2][3] * m[3][2] -
    m[2][0]  * m[0][2] * m[3][3] +
    m[2][0]  * m[0][3] * m[3][2] +
    m[3][0] * m[0][2] * m[2][3] -
    m[3][0] * m[0][3] * m[2][2];
    
    inv[9] = -m[0][0]  * m[2][1] * m[3][3] +
    m[0][0]  * m[2][3] * m[3][1] +
    m[2][0]  * m[0][1] * m[3][3] -
    m[2][0]  * m[0][3] * m[3][1] -
    m[3][0] * m[0][1] * m[2][3] +
    m[3][0] * m[0][3] * m[2][1];
    
    inv[13] = m[0][0]  * m[2][1] * m[3][2] -
    m[0][0]  * m[2][2] * m[3][1] -
    m[2][0]  * m[0][1] * m[3][2] +
    m[2][0]  * m[0][2] * m[3][1] +
    m[3][0] * m[0][1] * m[2][2] -
    m[3][0] * m[0][2] * m[2][1];
    
    inv[2] = m[0][1]  * m[1][2] * m[3][3] -
    m[0][1]  * m[1][3] * m[3][2] -
    m[1][1]  * m[0][2] * m[3][3] +
    m[1][1]  * m[0][3] * m[3][2] +
    m[3][1] * m[0][2] * m[1][3] -
    m[3][1] * m[0][3] * m[1][2];
    
    inv[6] = -m[0][0]  * m[1][2] * m[3][3] +
    m[0][0]  * m[1][3] * m[3][2] +
    m[1][0]  * m[0][2] * m[3][3] -
    m[1][0]  * m[0][3] * m[3][2] -
    m[3][0] * m[0][2] * m[1][3] +
    m[3][0] * m[0][3] * m[1][2];
    
    inv[10] = m[0][0]  * m[1][1] * m[3][3] -
    m[0][0]  * m[1][3] * m[3][1] -
    m[1][0]  * m[0][1] * m[3][3] +
    m[1][0]  * m[0][3] * m[3][1] +
    m[3][0] * m[0][1] * m[1][3] -
    m[3][0] * m[0][3] * m[1][1];
    
    inv[14] = -m[0][0]  * m[1][1] * m[3][2] +
    m[0][0]  * m[1][2] * m[3][1] +
    m[1][0]  * m[0][1] * m[3][2] -
    m[1][0]  * m[0][2] * m[3][1] -
    m[3][0] * m[0][1] * m[1][2] +
    m[3][0] * m[0][2] * m[1][1];
    
    inv[3] = -m[0][1] * m[1][2] * m[2][3] +
    m[0][1] * m[1][3] * m[2][2] +
    m[1][1] * m[0][2] * m[2][3] -
    m[1][1] * m[0][3] * m[2][2] -
    m[2][1] * m[0][2] * m[1][3] +
    m[2][1] * m[0][3] * m[1][2];
    
    inv[7] = m[0][0] * m[1][2] * m[2][3] -
    m[0][0] * m[1][3] * m[2][2] -
    m[1][0] * m[0][2] * m[2][3] +
    m[1][0] * m[0][3] * m[2][2] +
    m[2][0] * m[0][2] * m[1][3] -
    m[2][0] * m[0][3] * m[1][2];
    
    inv[11] = -m[0][0] * m[1][1] * m[2][3] +
    m[0][0] * m[1][3] * m[2][1] +
    m[1][0] * m[0][1] * m[2][3] -
    m[1][0] * m[0][3] * m[2][1] -
    m[2][0] * m[0][1] * m[1][3] +
    m[2][0] * m[0][3] * m[1][1];
    
    inv[15] = m[0][0] * m[1][1] * m[2][2] -
    m[0][0] * m[1][2] * m[2][1] -
    m[1][0] * m[0][1] * m[2][2] +
    m[1][0] * m[0][2] * m[2][1] +
    m[2][0] * m[0][1] * m[1][2] -
    m[2][0] * m[0][2] * m[1][1];
    
    det = m[0][0] * inv[0] + m[0][1] * inv[4] + m[0][2] * inv[8] + m[0][3] * inv[12];
    if(abs(det) <= 1.0e-5f)
    {
        for(i = 0; i < 16; i++)
        {
            invOut[i] = 99999.0f;
        }
    }
    else
    {
        det = 1.0f / det;

        for(i = 0; i < 16; i++)
        {
            invOut[i] = inv[i] * det;
        }
    }
    
    var ret: mat4x4<f32>;
    ret[0][0] = invOut[0];
    ret[0][1] = invOut[1];
    ret[0][2] = invOut[2];
    ret[0][3] = invOut[3];

    ret[1][0] = invOut[4];
    ret[1][1] = invOut[5];
    ret[1][2] = invOut[6];
    ret[1][3] = invOut[7];

    ret[2][0] = invOut[8];
    ret[2][1] = invOut[9];
    ret[2][2] = invOut[10];
    ret[2][3] = invOut[11];

    ret[3][0] = invOut[12];
    ret[3][1] = invOut[13];
    ret[3][2] = invOut[14];
    ret[3][3] = invOut[15];

    return ret;
}

