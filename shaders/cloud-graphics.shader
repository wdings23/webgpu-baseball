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
    @location(0) mColor: vec4<f32>,
};

@group(0) @binding(0)
var worldPositionTexture: texture_2d<f32>;

@group(0) @binding(1)
var prevCloudTexture: texture_2d<f32>;

@group(0) @binding(2)
var skyMotionVectorTexture: texture_2d<f32>;

@group(1) @binding(0)
var blueNoiseTexture: texture_2d<f32>;

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

//////
@fragment
fn fs_main(in: VertexOutput) -> FragmentOutput
{
	var output: FragmentOutput;

    //let worldPosition: vec4<f32> = textureSample(
    //    worldPositionTexture,
    //    textureSampler,
    //    in.uv.xy
    //);

    let iCurrFrame: i32 = defaultUniformBuffer.miFrame / 16;
    let iStep: i32 = defaultUniformBuffer.miFrame % 16;

    let motionVectorTextureSize: vec2<u32> = textureDimensions(skyMotionVectorTexture); 
    let textureSize: vec2<u32> = textureDimensions(prevCloudTexture);
    let screenCoord: vec2<i32> = vec2<i32>(
        i32(in.uv.x * f32(textureSize.x)),
        i32(in.uv.y * f32(textureSize.y)) 
    );
    
    let skyMotionVector: vec2<f32> = textureLoad(
        skyMotionVectorTexture,
        vec2<i32>(
            i32(in.uv.x * f32(motionVectorTextureSize.x)),
            i32(in.uv.y * f32(motionVectorTextureSize.y))
        ),
        0
    ).xy;
    let prevUV: vec2<f32> = in.uv.xy + skyMotionVector;
    let prevScreenCoord: vec2<i32> = vec2<i32>(
        i32(prevUV.x * f32(textureSize.x)),
        i32(prevUV.y * f32(textureSize.y)) 
    );

    // TODO: fix prev screen coord, taking the delay tile build into account
    let prevColor: vec4<f32> = textureLoad(
        prevCloudTexture,
        screenCoord,
        0
    );

    let blueNoiseTextureSize: vec2<u32> = textureDimensions(blueNoiseTexture);
    var uv: vec2<f32> = in.uv.xy + vec2<f32>(defaultUniformBuffer.mfRand0, defaultUniformBuffer.mfRand1);
    var blueNoise: vec4<f32> = textureSample(
        blueNoiseTexture,
        textureSampler,
        uv
    );

    let iTotalIndex: i32 = screenCoord.x + screenCoord.y * i32(textureSize.x);

    //let iIndexX: i32 = screenCoord.x / (i32(textureSize.x) / 4);
    //let iIndexY: i32 = screenCoord.y / (i32(textureSize.y) / 4);
    //let iTotalIndex: i32 = iIndexY * 4 + iIndexX;
    if(iTotalIndex % 16 != iStep)
    {
        output.mColor = prevColor;

        return output;
    }

    var screenSpace: vec3<f32> = vec3<f32>(in.uv.xy * 2.0f - 1.0f, 1.0f);
    screenSpace.y *= -1.0f;
    var worldPosition: vec4<f32> = vec4<f32>(screenSpace.xyz, 1.0f) * defaultUniformBuffer.mInverseViewProjectionMatrix;
    let fOneOverW: f32 = 1.0f / worldPosition.w;
    worldPosition.x *= fOneOverW;
    worldPosition.y *= fOneOverW;
    worldPosition.z *= fOneOverW;

    //let cloudPosition: vec3<f32> = vec3<f32>(f32(iCurrFrame % 1024) * 0.05f, 0.0f, 20.0f);
    let cloudPosition: vec3<f32> = vec3<f32>(0.0f, 0.0f, 20.0f);
    let fCloudRadius: f32 = 30.0f;

    let fDistanceToCloud: f32 = length(cloudPosition - defaultUniformBuffer.mCameraPosition.xyz);

    //let fCloudDistance: f32 = length(cloudPosition - defaultUniformBuffer.mCameraPosition.xyz);
    let rayDirection: vec3<f32> = normalize(worldPosition.xyz - defaultUniformBuffer.mCameraPosition.xyz);
    let rayOrigin: vec3<f32> = defaultUniformBuffer.mCameraPosition.xyz + rayDirection * (fDistanceToCloud - fCloudRadius * 1.1f);

    let lightDirection: vec3<f32> = defaultUniformBuffer.mLightDirection.xyz;

    var fTotalTransmittance: f32 = 1.0f;
    let fAbsorptionCoefficient: f32 = 0.9f;
    let fScatteringAnisotropic: f32 = 0.3f;

    var fLightEnergy: f32 = 0.0f;
    var fTotalDensity: f32 = 0.0f;

    let iNumSteps: i32 = 20;
    let fStepSize: f32 = fCloudRadius / f32(iNumSteps * 6);
    var currPosition: vec3<f32> = rayOrigin;

    for(var i: i32 = 0; i < iNumSteps; i++)
    {
        // scene density
        var fSceneDensity: f32 = scene(
            currPosition,
            cloudPosition,
            fCloudRadius
        );

        if(fSceneDensity >= 0.0f)
        {
            // anisotropic scattering
            let fPhase: f32 = henyeyGreenstein(fScatteringAnisotropic, max(dot(rayDirection, lightDirection), 0.0f));

            // march along the light direction
            var lightMarchPosition: vec3<f32> = currPosition;
            let fLightMarchStepSize: f32 = fStepSize * 0.5f;
            var fTotalLightSampleDensity: f32 = 0.0f;
            for(var j: i32 = 0; j < 10; j++)
            {
                // scene density
                let fLightSampleDensity: f32 = scene(lightMarchPosition, cloudPosition, fCloudRadius);
                if(fLightSampleDensity > 0.0f)
                {
                    fTotalLightSampleDensity += fLightSampleDensity;
                    lightMarchPosition += lightDirection * fLightMarchStepSize;
                }
            }

            // total up the light energy
            var fTotalLightSampleTransmittance: f32 = beersLaw(fTotalLightSampleDensity, fAbsorptionCoefficient);
            let fLuminance: f32 = fSceneDensity * fPhase;
            fTotalTransmittance *= fTotalLightSampleTransmittance;
            fLightEnergy += fTotalTransmittance * fLuminance;

            fTotalDensity += fSceneDensity;
        }

        currPosition += rayDirection * (fStepSize * fract(blueNoise.y));
    }

    fLightEnergy *= 100.0f;
    output.mColor = vec4<f32>(fLightEnergy, fLightEnergy, fLightEnergy, fTotalDensity);

    return output;
}

/*
**
*/
fn fbm(p: vec3<f32>, octaves: i32, persistence: f32) -> f32 
{
    var total: f32 = 0.0f;
    var amp: f32 = 1.0f;
    var freq: f32 = 1.0f;
    for (var i: i32 = 0; i < octaves; i++) 
    {
        total += noise2(p * freq) * amp;
        amp *= persistence;
        freq *= 2.0f;
    }

    return total;
}

fn hash(n: f32) -> f32
{
    return fract(sin(n)*43758.5453f);
}

fn noise2(x: vec3<f32>) -> f32 
{
    let p: vec3<f32> = floor(x);
    var f: vec3<f32> = fract(x);

    f = f * f * (3.0f - 2.0f * f);

    let n: f32 = p.x + p.y * 57.0f + 113.0f * p.z;

    let res: f32 = mix(mix(mix( hash(n+  0.0f), hash(n+  1.0f), f.x),
                        mix( hash(n+ 57.0f), hash(n+ 58.0f), f.x), f.y),
                    mix(mix( hash(n+113.0f), hash(n+114.0f), f.x),
                        mix( hash(n+170.0f), hash(n+171.0f), f.x), f.y), f.z);
    return res;
}



/*
**
*/
fn sphere( 
    currPosition: vec3<f32>,
    centerPosition: vec3<f32>,
    fRadius: f32) -> f32
{
    let diff: vec3<f32> = centerPosition - currPosition;
    let fDistance: f32 = length(diff);
    var fSceneDensity: f32 = fRadius - fDistance;
    
    return fSceneDensity;
}

/*
**
*/
fn scene( 
    currPosition: vec3<f32>,
    cloudPosition: vec3<f32>,
    fCloudRadius: f32) -> f32
{
    var fDensity: f32 = sphere(
        currPosition,
        cloudPosition,
        fCloudRadius
    );

    let noisePosition: vec3<f32> = currPosition + f32(defaultUniformBuffer.miFrame) * 0.005f * vec3<f32>(1.0f, -0.2f, -1.0f);
    fDensity += fbm(noisePosition, 5, 0.3f);

    let diff: vec3<f32> = currPosition - cloudPosition;
    fDensity *= clamp((diff.y + 0.25f) / fCloudRadius, 0.0f, 0.5f);

    return fDensity;
}

/*
**
*/
fn beersLaw(
    fDistance: f32, 
    fAbsorption: f32) -> f32
{

  return exp(-fDistance * fAbsorption);
}

/*
**
*/
fn henyeyGreenstein(g: f32, mu: f32) -> f32 
{
    let gg: f32 = g * g;
	return (1.0f / (4.0f * 3.14159f))  * ((1.0f - gg) / pow(1.0f + gg - 2.0f * g * mu, 1.5f));
}






