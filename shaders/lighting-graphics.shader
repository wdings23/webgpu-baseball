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
    @location(0) mDiffuse: vec4<f32>,
    @location(1) mSpecular: vec4<f32>,
    @location(2) mAmbient: vec4<f32>,
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

@group(0) @binding(0)
var worldPositionTexture: texture_2d<f32>;

@group(0) @binding(1)
var normalTexture: texture_2d<f32>;

@group(0) @binding(2)
var albedoTexture: texture_2d<f32>;

@group(0) @binding(3)
var skyConvolutionTexture: texture_2d<f32>;

@group(0) @binding(4)
var sunLightColorTexture: texture_2d<f32>;

@group(0) @binding(5)
var animMeshMaskTexture: texture_2d<f32>;

@group(1) @binding(0)
var<storage, read> uniformBuffer: UniformData; 

@group(1) @binding(1)
var<uniform> defaultUniformBuffer: DefaultUniformData;

@group(1) @binding(2)
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
    
    let worldPosition: vec4<f32> = textureSample(
        worldPositionTexture,
        textureSampler,
        in.uv.xy
    );

    let normal: vec4<f32> = textureSample(
        normalTexture,
        textureSampler,
        in.uv.xy
    );
    
    let skyUV: vec2<f32> = octahedronMap2(normal.xyz);
    let ambientLight: vec4<f32> = textureSample(
        skyConvolutionTexture,
        textureSampler,
        skyUV
    );
    let albedo: vec4<f32> = textureSample(
        albedoTexture,
        textureSampler,
        in.uv.xy
    );
    let animMeshMask: vec4<f32> = textureSample(
        animMeshMaskTexture,
        textureSampler,
        in.uv.xy   
    );

    let fRoughness: f32 = 0.4f;
    let fMetallic: f32 = 0.01f;

    let lighting: LightResult = pbr(
        worldPosition.xyz,
        normal.xyz,
        fRoughness,
        fMetallic,
        ambientLight.xyz,
        animMeshMask.xyz
    );

    out.mDiffuse = vec4<f32>(lighting.mDiffuse.xyz, 1.0f);
    out.mSpecular = vec4<f32>(lighting.mSpecular.xyz, 1.0f);
    out.mAmbient = vec4<f32>(lighting.mAmbient.xyz, 1.0f);

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
fn pbr(worldPosition: vec3f,
       normal: vec3f, 
       roughness: f32,
       metallic: f32,
       ambientLighting: vec3<f32>,
       animMeshMask: vec3<f32>) -> LightResult
{
    let albedo: vec3f = vec3f(1.0f, 1.0f, 1.0f);

    //let sunLightColor: vec3<f32> = vec3<f32>(7.0f, 7.0f, 7.0f) * 0.5f;
    let uv: vec2<f32> = octahedronMap2(normal.xyz);
    let sunLightColor: vec3<f32> = textureSample(
        sunLightColorTexture,
        textureSampler,
        uv.xy
    ).xyz;

    let view: vec3f = normalize(defaultUniformBuffer.mCameraPosition.xyz - worldPosition);

    // calculate reflectance at normal incidence; if dia-electric (like plastic) use F0 
    // of 0.04 and if it's a metal, use the albedo color as F0 (metallic workflow)    
    var F0: vec3<f32> = vec3<f32>(0.04f, 0.04f, 0.04f); 
    F0 = mix(F0, albedo, vec3<f32>(metallic, metallic, metallic));

    var totalSpecular: vec3<f32> = vec3<f32>(0.0f, 0.0f, 0.0f);
    var totalDiffuse: vec3<f32> = vec3<f32>(0.0f, 0.0f, 0.0f);

    // reflectance equation
    var Lo: vec3f = vec3f(0.0f, 0.0f, 0.0f);
    /// directional lighting
    {
        // calculate per-light radiance
        let L: vec3<f32> = normalize(vec3<f32>(0.2f, 1.0f, 0.0f));
        let H: vec3f = normalize(view + L);
        let radiance: vec3f = sunLightColor * 4.0f; // * attenuation;

        // Cook-Torrance BRDF
        let NDF: f32 = DistributionGGX(normal, H, roughness);   
        let G: f32   = GeometrySmith(normal, view, L, roughness);      
        let F: vec3f    = fresnelSchlick(max(dot(H, view), 0.0f), F0);
           
        let numerator: vec3f    = NDF * G * F; 
        let denominator: f32 = 4.0f * max(dot(normal, view), 0.0) * max(dot(normal, L), 0.0) + 0.0001f; // + 0.0001 to prevent divide by zero
        let specular: vec3f = numerator / denominator;
        
        // kS is equal to Fresnel
        let kS: vec3f = F;
        // for energy conservation, the diffuse and specular light can't
        // be above 1.0 (unless the surface emits light); to preserve this
        // relationship the diffuse component (kD) should equal 1.0 - kS.
        var kD: vec3f = vec3f(1.0f, 1.0f, 1.0f) - kS;
        // multiply kD by the inverse metalness such that only non-metals 
        // have diffuse lighting, or a linear blend if partly metal (pure metals
        // have no diffuse light).
        kD *= 1.0f - metallic;	  

        // scale light by NdotL
        let NdotL: f32 = max(dot(normal, L), 0.0f);        
        
        // add to outgoing radiance Lo
        totalSpecular += specular * radiance * NdotL;
        totalDiffuse += (kD * albedo / PI) * radiance * NdotL; 
    }   
    
    for(var i: i32 = 0; i < 5; i++)
    {
        if(i >= 1 && animMeshMask.x < 1.0f)
        {
            continue;
        }

        let lightingResult: LightResult = pointLight(
            view,
            normal,
            uniformBuffer.maPointLightInfo[i].mPosition.xyz,
            uniformBuffer.maPointLightInfo[i].mRadiance.xyz,
            worldPosition,
            roughness,
            metallic
        );

        totalSpecular += lightingResult.mSpecular;
        totalDiffuse += lightingResult.mDiffuse;
    }

    let F: vec3<f32> = fresnelSchlickRoughness(max(dot(normal, view), 0.0f), F0, roughness);
    let kS: vec3<f32> = F;
    var kD: vec3<f32> = 1.0f - kS;
    kD *= 1.0 - metallic;	

    let ambient: vec3f = kD * ambientLighting * albedo;
    
    var ret: LightResult;
    ret.mDiffuse = totalDiffuse;
    ret.mSpecular = totalSpecular;
    ret.mAmbient = ambient;

    return ret;
}

/*
**
*/
fn pointLight(
    view: vec3<f32>,
    normal: vec3<f32>,
    position: vec3<f32>,
    lightRadiance: vec3<f32>,
    worldPosition: vec3<f32>,
    fRoughness: f32,
    fMetallic: f32) -> LightResult
{
    var ret: LightResult;

    let albedo: vec3<f32> = vec3<f32>(1.0f, 1.0f, 1.0f);

    var F0: vec3<f32> = vec3<f32>(0.04f, 0.04f, 0.04f); 
    F0 = mix(F0, albedo, vec3<f32>(fMetallic, fMetallic, fMetallic));

    // calculate per-light radiance
    let L: vec3f = normalize(position - worldPosition);
    let H: vec3f = normalize(view + L);
    let distance: f32 = length(position - worldPosition);
    let attenuation: f32 = 1.0f / (distance * distance);
    let radiance: vec3f = lightRadiance * attenuation;

    // Cook-Torrance BRDF
    let NDF: f32 = DistributionGGX(normal, H, fRoughness);   
    let G: f32   = GeometrySmith(normal, view, L, fRoughness);      
    let F: vec3f    = fresnelSchlick(max(dot(H, view), 0.0f), F0);
        
    let numerator: vec3f    = NDF * G * F; 
    let denominator: f32 = 4.0f * max(dot(normal, view), 0.0) * max(dot(normal, L), 0.0) + 0.0001f; // + 0.0001 to prevent divide by zero
    let specular: vec3f = numerator / denominator;
    
    // kS is equal to Fresnel
    let kS: vec3f = F;
    // for energy conservation, the diffuse and specular light can't
    // be above 1.0 (unless the surface emits light); to preserve this
    // relationship the diffuse component (kD) should equal 1.0 - kS.
    var kD: vec3f = vec3f(1.0f, 1.0f, 1.0f) - kS;
    // multiply kD by the inverse metalness such that only non-metals 
    // have diffuse lighting, or a linear blend if partly metal (pure metals
    // have no diffuse light).
    kD *= 1.0f - fMetallic;	  

    // scale light by NdotL
    let NdotL: f32 = max(dot(normal, L), 0.0f);        

    ret.mSpecular = specular * radiance * NdotL;
    ret.mDiffuse = (kD * albedo / PI) * radiance * NdotL;

    return ret;
}

/*
**
*/
fn DistributionGGX(
    N: vec3f, 
    H: vec3f, 
    roughness: f32) -> f32
{
    let a: f32 = roughness*roughness;
    let a2: f32 = a*a;
    let NdotH: f32 = max(dot(N, H), 0.0);
    let NdotH2: f32 = NdotH*NdotH;

    let nom: f32   = a2;
    var denom: f32 = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}

/*
**
*/
fn GeometrySchlickGGX(
    NdotV: f32, 
    roughness: f32) -> f32
{
    let r: f32 = (roughness + 1.0f);
    let k: f32 = (r*r) / 8.0f;

    let nom: f32   = NdotV;
    let denom: f32 = NdotV * (1.0f - k) + k;

    return nom / denom;
}

/*
**
*/
fn GeometrySmith(
    N: vec3f, 
    V: vec3f, 
    L: vec3f, 
    roughness: f32) -> f32
{
    let NdotV: f32 = max(dot(N, V), 0.0f);
    let NdotL: f32 = max(dot(N, L), 0.0f);
    let ggx2: f32 = GeometrySchlickGGX(NdotV, roughness);
    let ggx1: f32 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

/*
**
*/
fn fresnelSchlick(
    cosTheta: f32, 
    F0: vec3f) -> vec3f
{
    return F0 + (1.0f - F0) * pow(clamp(1.0f - cosTheta, 0.0f, 1.0f), 5.0f);
}

/*
**
*/
fn fresnelSchlickRoughness(
    fCosTheta: f32, 
    F0: vec3<f32>, 
    fRoughness: f32) -> vec3<f32>
{
    return F0 + (max(vec3<f32>(1.0f - fRoughness), F0) - F0) * pow(clamp(1.0f - fCosTheta, 0.0f, 1.0f), 5.0f);
}   