// -------------------------------------
// Defines
const EPS: f32 =               1e-6f;
const PLANET_RADIUS: f32 =       6371000.0f;
const PLANET_CENTER: vec3<f32> =       vec3<f32>(0.0f, -PLANET_RADIUS, 0.0f);
const ATMOSPHERE_HEIGHT: f32 =   100000.0f;
const RAYLEIGH_HEIGHT: f32 =     (ATMOSPHERE_HEIGHT * 0.06f);
const MIE_HEIGHT: f32 =          (ATMOSPHERE_HEIGHT * 0.012f);
const PI: f32 =                  3.14159f;

// -------------------------------------
// Coefficients
const C_RAYLEIGH: vec3<f32> =          (vec3<f32>(5.802f, 13.558f, 33.100f) * 1e-6f);
const C_MIE: vec3<f32> =               (vec3<f32>(3.996f,  3.996f,  3.996f) * 1e-6f);
const C_OZONE: vec3<f32> =             (vec3<f32>(0.650f,  1.881f,  0.085f) * 1e-6f);

const ATMOSPHERE_DENSITY: f32 =  1.0f;
const EXPOSURE: f32 =            15.0f;

// -------------------------------------
// Math
fn SphereIntersection(
	rayStart: vec3<f32>, 
	rayDir: vec3<f32>, 
	sphereCenter: vec3<f32>, 
	sphereRadius: f32) -> vec2<f32>
{
	let rayStartDiff: vec3<f32> = rayStart - sphereCenter;
	let a: f32 = dot(rayDir, rayDir);
	let b: f32 = 2.0f * dot(rayStartDiff, rayDir);
	let c: f32 = dot(rayStartDiff, rayStartDiff) - (sphereRadius * sphereRadius);
	var d: f32 = b * b - 4.0f * a * c;
	if(d < 0)
	{
		return vec2<f32>(-1.0f, -1.0f);
	}
	else
	{
		d = sqrt(d);
		return vec2<f32>(-b - d, -b + d) / (2.0f * a);
	}
}
fn PlanetIntersection(
	rayStart: vec3<f32>, 
	rayDir: vec3<f32>) -> vec2<f32>
{
	return SphereIntersection(rayStart, rayDir, PLANET_CENTER, PLANET_RADIUS);
}
fn AtmosphereIntersection(
	rayStart: vec3<f32>, 
	rayDir: vec3<f32>) -> vec2<f32>
{
	return SphereIntersection(rayStart, rayDir, PLANET_CENTER, PLANET_RADIUS + ATMOSPHERE_HEIGHT);
}

// -------------------------------------
// Phase functions
fn PhaseRayleigh(costh: f32) -> f32
{
	return 3.0f * (1.0f + costh * costh) / (16.0f * PI);
}
fn PhaseMie(
    costh: f32, 
    g: f32) -> f32
{
	let gCopy: f32 = min(g, 0.9381f);
	let k: f32 = 1.55f * gCopy - 0.55f * gCopy * gCopy * gCopy;
	let kcosth: f32 = k * costh;
	return (1.0f - k * k) / ((4.0f * PI) * (1.0f - kcosth) * (1.0f - kcosth));
}

// -------------------------------------
// Atmosphere
fn AtmosphereHeight(positionWS: vec3<f32>) -> f32
{
	//return distance(positionWS, PLANET_CENTER) - PLANET_RADIUS;

	return length(positionWS - PLANET_CENTER) - PLANET_RADIUS;
}
fn DensityRayleigh(h: f32) -> f32
{
	return exp(-max(0.0f, h / RAYLEIGH_HEIGHT));
}
fn DensityMie(h: f32) -> f32
{
	return exp(-max(0.0f, h / MIE_HEIGHT));
}
fn DensityOzone(h: f32) -> f32
{
	// The ozone layer is represented as a tent function with a width of 30km, centered around an altitude of 25km.
	return max(0.0f, 1.0f - abs(h - 25000.0f) / 15000.0f);
}
fn AtmosphereDensity(h: f32) -> vec3<f32>
{
	return vec3<f32>(DensityRayleigh(h), DensityMie(h), DensityOzone(h));
}

// Optical depth is a unitless measurement of the amount of absorption of a participating medium (such as the atmosphere).
// This function calculates just that for our three atmospheric elements:
// R: Rayleigh
// G: Mie
// B: Ozone
// If you find the term "optical depth" confusing, you can think of it as "how much density was found along the ray in total".
fn IntegrateOpticalDepth(
    rayStart: vec3<f32>, 
    rayDir: vec3<f32>) -> vec3<f32>
{
	let intersection: vec2<f32> = AtmosphereIntersection(rayStart, rayDir);
	let  rayLength: f32 = intersection.y;

	let    sampleCount: i32 = 8;
	let  stepSize: f32 = rayLength / f32(sampleCount);

	var opticalDepth: vec3<f32> = vec3<f32>(0.0f, 0.0f, 0.0f);

	for(var i: i32 = 0; i < sampleCount; i++)
	{
		let localPosition: vec3<f32> = rayStart + rayDir * (f32(i) + 0.5f) * stepSize;
		let  localHeight: f32 = AtmosphereHeight(localPosition);
		let localDensity: vec3<f32> = AtmosphereDensity(localHeight);

		opticalDepth += localDensity * stepSize;
	}

	return opticalDepth;
}

// Calculate a luminance transmittance value from optical depth.
fn Absorb(opticalDepth: vec3<f32>) -> vec3<f32>
{
	// Note that Mie results in slightly more light absorption than scattering, about 10%
	let ret: vec3<f32> = ((C_RAYLEIGH * opticalDepth.x) + (C_MIE * (opticalDepth.y * 1.1f)) + (C_OZONE * opticalDepth.z)) * ATMOSPHERE_DENSITY * -1.0f;
	return vec3<f32>(
		exp(ret.x),
		exp(ret.y),
		exp(ret.z));

}

struct ScatteringResult
{
    radiance: vec3<f32>,
    transmittance: vec3<f32>,
};

// Integrate scattering over a ray for a single directional light source.
// Also return the transmittance for the same ray as we are already calculating the optical depth anyway.
fn IntegrateScattering(
	rayStart: vec3<f32>, 
	rayDir: vec3<f32>, 
	rayLength: f32, 
	lightDir: vec3<f32>, 
	lightColor: vec3<f32>, 
	transmittance: vec3<f32>) -> ScatteringResult
{
	// We can reduce the number of atmospheric samples required to converge by spacing them exponentially closer to the camera.
	// This breaks space view however, so let's compensate for that with an exponent that "fades" to 1 as we leave the atmosphere.
	var rayStartCopy: vec3<f32> = rayStart;
    let  rayHeight: f32 = AtmosphereHeight(rayStartCopy);
	let  sampleDistributionExponent: f32 = 1.0f + clamp(1.0f - rayHeight / ATMOSPHERE_HEIGHT, 0.0f, 1.0f) * 8; // Slightly arbitrary max exponent of 9

	let intersection: vec2<f32> = AtmosphereIntersection(rayStartCopy, rayDir);
	var rayLengthCopy: f32 = min(rayLength, intersection.y);
	if(intersection.x > 0)
	{
		// Advance ray to the atmosphere entry point
		rayStartCopy += rayDir * intersection.x;
		rayLengthCopy -= intersection.x;
	}

	let  costh: f32 = dot(rayDir, lightDir);
	let  phaseR: f32 = PhaseRayleigh(costh);
	let  phaseM: f32 = PhaseMie(costh, 0.85f);

	let    sampleCount: i32 = 64;

	var opticalDepth: vec3<f32> = vec3<f32>(0.0f, 0.0f, 0.0f);
	var rayleigh: vec3<f32> = vec3<f32>(0.0f, 0.0f, 0.0f);
	var mie: vec3<f32> = vec3<f32>(0.0f, 0.0f, 0.0f);

	var  prevRayTime: f32 = 0.0f;

	for(var i: i32 = 0; i < sampleCount; i++)
	{
		let  rayTime: f32 = pow(f32(i) / f32(sampleCount), sampleDistributionExponent) * rayLengthCopy;
		// Because we are distributing the samples exponentially, we have to calculate the step size per sample.
		let  stepSize: f32 = (rayTime - prevRayTime);

		let localPosition: vec3<f32> = rayStartCopy + rayDir * rayTime;
		let  localHeight: f32 = AtmosphereHeight(localPosition);
		let localDensity: vec3<f32> = AtmosphereDensity(localHeight);

		opticalDepth += localDensity * stepSize;

		// The atmospheric transmittance from rayStart to localPosition
		let viewTransmittance: vec3<f32> = Absorb(opticalDepth);

		let opticalDepthlight: vec3<f32> = IntegrateOpticalDepth(localPosition, lightDir);
		// The atmospheric transmittance of light reaching localPosition
		let lightTransmittance: vec3<f32> = Absorb(opticalDepthlight);

		rayleigh += viewTransmittance * lightTransmittance * phaseR * localDensity.x * stepSize;
		mie += viewTransmittance * lightTransmittance * phaseM * localDensity.y * stepSize;

		prevRayTime = rayTime;
	}

    var ret: ScatteringResult;
	ret.transmittance = Absorb(opticalDepth);
    ret.radiance = (rayleigh * C_RAYLEIGH + mie * C_MIE) * lightColor * EXPOSURE;

	return ret;
}

/////
fn decodeOctahedronMap(uv: vec2<f32>) -> vec3<f32>
{
    let newUV: vec2<f32> = uv * 2.0f - vec2<f32>(1.0f, 1.0f);

    let absUV: vec2<f32> = vec2<f32>(abs(newUV.x), abs(newUV.y));
    var v: vec3<f32> = vec3<f32>(newUV.x, newUV.y, 1.0f - (absUV.x + absUV.y));

    if(absUV.x + absUV.y > 1.0f) 
    {
        v.x = (abs(newUV.y) - 1.0f) * -sign(newUV.x);
        v.y = (abs(newUV.x) - 1.0f) * -sign(newUV.y);
    }

    v.y *= -1.0f;

    return v;
}

struct VertexInput {
    @location(0) pos : vec4<f32>,
    @location(1) texcoord: vec2<f32>,
    @location(2) color : vec4<f32>
};
struct VertexOutput 
{
    @builtin(position) pos: vec4f,
    @location(0) uv: vec2f,
};
struct FragmentOutput {
    @location(0) colorOutput : vec4f,
	@location(1) sunLightOutput : vec4f,
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

struct UniformData
{
    mLastLightDirection: vec4<f32>,
	mLastLightRadiance: vec4<f32>,
};

@group(1) @binding(0)
var<storage, read_write> uniformData: UniformData;

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

    let direction: vec3<f32> = decodeOctahedronMap(in.uv).xyz;
    let transmittance: vec3<f32> = vec3<f32>(0.0f, 0.0f, 0.0f);
    
    var ret: ScatteringResult;
    ret = IntegrateScattering(
        vec3<f32>(0.0f, 0.0f, 0.0f), 
        direction, 
        PLANET_RADIUS, 
        defaultUniformBuffer.mLightDirection.xyz, 
        defaultUniformBuffer.mLightRadiance.xyz, 
        transmittance);

    output.colorOutput.x = ret.radiance.x;
    output.colorOutput.y = ret.radiance.y;
    output.colorOutput.z = ret.radiance.z;
    output.colorOutput.w = 1.0f;

	ret = IntegrateScattering(
        vec3<f32>(0.0f, 0.0f, 0.0f), 
        defaultUniformBuffer.mLightDirection.xyz, 
        PLANET_RADIUS, 
        defaultUniformBuffer.mLightDirection.xyz, 
        defaultUniformBuffer.mLightRadiance.xyz, 
        transmittance);

	output.sunLightOutput.x = ret.transmittance.x;
	output.sunLightOutput.y = ret.transmittance.y;
	output.sunLightOutput.z = ret.transmittance.z;
	output.sunLightOutput.w = 1.0f;

	uniformData.mLastLightDirection = defaultUniformBuffer.mLightDirection;
	uniformData.mLastLightRadiance = defaultUniformBuffer.mLightRadiance;

    return output;
}