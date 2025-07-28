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

struct SHOutput 
{
    mCoefficients0: vec4<f32>,
    mCoefficients1: vec4<f32>,
    mCoefficients2: vec4<f32>,
};

struct UniformData
{
    maLightViewProjectionMatrices: array<mat4x4<f32>, 3>,
    mDecalViewProjectionMatrix: mat4x4<f32>,
};


@group(0) @binding(0)
var worldPositionTexture: texture_2d<f32>;

@group(0) @binding(1)
var normalTexture: texture_2d<f32>;

@group(0) @binding(2) 
var skyTexture: texture_2d<f32>;

@group(0) @binding(3)
var prevSHCoefficientTexture0: texture_2d<f32>;

@group(0) @binding(4)
var prevSHCoefficientTexture1: texture_2d<f32>;

@group(0) @binding(5)
var prevSHCoefficientTexture2: texture_2d<f32>;

@group(0) @binding(6)
var prevDecodedTexture: texture_2d<f32>;

@group(1) @binding(0)
var<storage> blueNoiseBuffer: array<vec2<f32>>;

@group(1) @binding(1)
var<storage, read> shadowUniform : UniformData;

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
    @location(0) mSHCoefficients0: vec4<f32>,
    @location(1) mSHCoefficients1: vec4<f32>,
    @location(2) mSHCoefficients2: vec4<f32>,

    @location(3) mDecoded: vec4<f32>,
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

    out.mSHCoefficients0 = vec4<f32>(0.0f, 0.0f, 0.0f, 0.0f);
    out.mSHCoefficients1 = vec4<f32>(0.0f, 0.0f, 0.0f, 0.0f);
    out.mSHCoefficients2 = vec4<f32>(0.0f, 0.0f, 0.0f, 0.0f);
    out.mDecoded = vec4<f32>(0.0f, 0.0f, 0.0f, 0.0f);

    let texCoord: vec2<i32> = vec2<i32>(
        i32(in.uv.x * f32(defaultUniformBuffer.miScreenWidth)),
        i32(in.uv.y * f32(defaultUniformBuffer.miScreenHeight))
    );

    let worldPosition: vec4<f32> = textureLoad(
        worldPositionTexture,
        texCoord,
        0
    );

    if(worldPosition.w > 0.0f)
    {
        let normal: vec3<f32> = textureLoad(
            normalTexture,
            texCoord,
            0
        ).xyz;

        var prevCoefficent0: vec4<f32> = textureLoad(
            prevSHCoefficientTexture0,
            texCoord,
            0
        );
        var prevCoefficent1: vec4<f32> = textureLoad(
            prevSHCoefficientTexture1,
            texCoord,
            0
        );
        var prevCoefficent2: vec4<f32> = textureLoad(
            prevSHCoefficientTexture2,
            texCoord,
            0
        );

prevCoefficent0 = vec4<f32>(0.0f, 0.0f, 0.0f, 0.0f);
prevCoefficent1 = vec4<f32>(0.0f, 0.0f, 0.0f, 0.0f);
prevCoefficent2 = vec4<f32>(0.0f, 0.0f, 0.0f, 0.0f);

        var prevDecoded: vec4<f32> = textureLoad(
            prevDecodedTexture,
            texCoord,
            0
        );
        var fHistoryCount: f32 = prevDecoded.w;

        let iTotalBufferIndex: u32 = u32(texCoord.y * defaultUniformBuffer.miScreenWidth + texCoord.x);
        let skyTextureSize: vec2<u32> = textureDimensions(skyTexture);

        let iNumSamples: u32 = 128u;
        for(var iSample: u32 = 0u; iSample < iNumSamples; iSample++)
        {
            let iBlueNoiseSampleIndex: u32 = (iTotalBufferIndex + iNumSamples * u32(defaultUniformBuffer.miFrame)) % 255u;

            //let fOffsetY: f32 = f32(u32(defaultUniformBuffer.miFrame) * iNumSamples) / f32(defaultUniformBuffer.miScreenWidth);
            //let iX: u32 = (u32(in.uv.x * f32(defaultUniformBuffer.miScreenWidth)) + u32(u32(defaultUniformBuffer.miFrame) * iNumSamples) + iSample) % u32(defaultUniformBuffer.miScreenWidth);
            //let iY: u32 = (u32(in.uv.y * f32(defaultUniformBuffer.miScreenHeight)) + u32(fOffsetY)) % u32(defaultUniformBuffer.miScreenHeight);

            //let iBlueNoiseBufferWidth: u32 = 32u;
            //let iBufferIndex = iY * iBlueNoiseBufferWidth + iX;
            var blueNoise: vec2<f32> = blueNoiseBuffer[iSample] * vec2<f32>(0.5f, 0.5f) + vec2<f32>(0.5f, 0.5f);

            var rayDirection: vec3<f32> = uniformSampling(
                worldPosition.xyz,
                normal.xyz,
                blueNoise.x,
                blueNoise.y
            );

            if(iSample == 0u)
            {
                rayDirection = normal.xyz;
            }

            let sampleUV: vec2<f32> = octahedronMap2(vec3<f32>(rayDirection.x, rayDirection.y, rayDirection.z));
            let sampleTexCoord: vec2<i32> = vec2<i32>(
                i32(sampleUV.x * f32(skyTextureSize.x)),
                i32(sampleUV.y * f32(skyTextureSize.y))
            );

            let skyRadiance: vec3<f32> = textureLoad(
                skyTexture,
                sampleTexCoord,
                0
            ).xyz;

            let encodedCoefficients: SHOutput = encodeSphericalHarmonics(
                skyRadiance,
                rayDirection,
                prevCoefficent0,
                prevCoefficent1,
                prevCoefficent2
            );

            prevCoefficent0 = encodedCoefficients.mCoefficients0;
            prevCoefficent1 = encodedCoefficients.mCoefficients1;
            prevCoefficent2 = encodedCoefficients.mCoefficients2;
        }

        fHistoryCount += f32(iNumSamples);
        let fMaxHistoryCount: f32 = 200.0f;
        var fPct: f32 = 1.0f;
        if(fHistoryCount > fMaxHistoryCount)
        {
            fPct = fMaxHistoryCount / fHistoryCount;
            fHistoryCount = fPct;
        }

        let radiance: vec3<f32> = decodeFromSphericalHarmonicCoefficients(
            prevCoefficent0,
            prevCoefficent1,
            prevCoefficent2,
            normal,
            vec3<f32>(10.0f, 10.0f, 10.0f),
            f32(iNumSamples)
        );

        out.mSHCoefficients0 = prevCoefficent0 * fPct;
        out.mSHCoefficients1 = prevCoefficent1 * fPct;
        out.mSHCoefficients2 = prevCoefficent2 * fPct;

        let fDP: f32 = max(dot(defaultUniformBuffer.mLightDirection.xyz, normal.xyz), 0.0f);

        //let lightViewDebug0: vec4<f32> = debugLightView(
        //    vec4<f32>(worldPosition.xyz, 1.0f),
        //    0,
        //    vec4<f32>(1.0f, 0.0f, 0.0f, 1.0f));
        //let lightViewDebug1: vec4<f32> = debugLightView(
        //    vec4<f32>(worldPosition.xyz, 1.0f),
        //    1,
        //    vec4<f32>(0.0f, 1.0f, 0.0f, 1.0f));
        //let lightViewDebug2: vec4<f32> = debugLightView(
        //    vec4<f32>(worldPosition.xyz, 1.0f),
        //    2,
        //    vec4<f32>(0.0f, 0.0f, 1.0f, 1.0f));

        out.mDecoded = vec4<f32>(radiance.xyz + vec3<f32>(fDP), 1.0f); // * lightViewDebug0 * lightViewDebug1 * lightViewDebug2;
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
fn uniformSampling(
    worldPosition: vec3<f32>,
    normal: vec3<f32>,
    fRand0: f32,
    fRand1: f32) -> vec3<f32>
{
    let fPhi: f32 = 2.0f * PI * fRand0;
    let fCosTheta: f32 = 1.0f - fRand1;
    let fSinTheta: f32 = sqrt(1.0f - fCosTheta * fCosTheta);
    let h: vec3<f32> = vec3<f32>(
        cos(fPhi) * fSinTheta,
        sin(fPhi) * fSinTheta,
        fCosTheta);

    var up: vec3<f32> = vec3<f32>(0.0f, 1.0f, 0.0f);
    if(abs(normal.y) > 0.999f)
    {
        up = vec3<f32>(1.0f, 0.0f, 0.0f);
    }
    let tangent: vec3<f32> = normalize(cross(up, normal));
    let binormal: vec3<f32> = normalize(cross(normal, tangent));
    let rayDirection: vec3<f32> = normalize(tangent * h.x + binormal * h.y + normal * h.z);

    return rayDirection;
}

/*
**
*/
fn encodeSphericalHarmonics(
    radiance: vec3<f32>,
    direction: vec3<f32>,
    SHCoefficent0: vec4<f32>,
    SHCoefficent1: vec4<f32>,
    SHCoefficent2: vec4<f32>
) -> SHOutput
{
    var output: SHOutput;

    output.mCoefficients0 = SHCoefficent0;
    output.mCoefficients1 = SHCoefficent1;
    output.mCoefficients2 = SHCoefficent2;

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

    output.mCoefficients0.x = output.mCoefficients0.x * fSrcPct + aResults[0].x;
    output.mCoefficients0.y = output.mCoefficients0.y * fSrcPct + aResults[0].y;
    output.mCoefficients0.z = output.mCoefficients0.z * fSrcPct + aResults[0].z;
    output.mCoefficients0.w = output.mCoefficients0.w * fSrcPct + aResults[1].x;

    output.mCoefficients1.x = output.mCoefficients1.x * fSrcPct + aResults[1].y;
    output.mCoefficients1.y = output.mCoefficients1.y * fSrcPct + aResults[1].z;
    output.mCoefficients1.z = output.mCoefficients1.z * fSrcPct + aResults[2].x;
    output.mCoefficients1.w = output.mCoefficients1.w * fSrcPct + aResults[2].y;

    output.mCoefficients2.x = output.mCoefficients2.x * fSrcPct + aResults[2].z;
    output.mCoefficients2.y = output.mCoefficients2.y * fSrcPct + aResults[3].x;
    output.mCoefficients2.z = output.mCoefficients2.z * fSrcPct + aResults[3].y;
    output.mCoefficients2.w = output.mCoefficients2.w * fSrcPct + aResults[3].z;


/*
    output.mCoefficients0.x += aResults[0].x;
    output.mCoefficients0.y += aResults[0].y;
    output.mCoefficients0.z += aResults[0].z;
    output.mCoefficients0.w += aResults[1].x;

    output.mCoefficients1.x += aResults[1].y;
    output.mCoefficients1.y += aResults[1].z;
    output.mCoefficients1.z += aResults[2].x;
    output.mCoefficients1.w += aResults[2].y;

    output.mCoefficients2.x += aResults[2].z;
    output.mCoefficients2.y += aResults[3].x;
    output.mCoefficients2.z += aResults[3].y;
    output.mCoefficients2.w += aResults[3].z;
*/
    return output;
}

/*
**
*/
fn decodeFromSphericalHarmonicCoefficients(
    SHCoefficent0: vec4<f32>,
    SHCoefficent1: vec4<f32>,
    SHCoefficent2: vec4<f32>,
    direction: vec3<f32>,
    maxRadiance: vec3<f32>,
    fHistoryCount: f32
) -> vec3<f32>
{
    var aTotalCoefficients: array<vec3<f32>, 4>;
    let fFactor: f32 = 1.0f;

    aTotalCoefficients[0] = vec3<f32>(SHCoefficent0.x, SHCoefficent0.y, SHCoefficent0.z) * fFactor;
    aTotalCoefficients[1] = vec3<f32>(SHCoefficent0.w, SHCoefficent1.x, SHCoefficent1.y) * fFactor;
    aTotalCoefficients[2] = vec3<f32>(SHCoefficent1.z, SHCoefficent1.w, SHCoefficent2.x) * fFactor;
    aTotalCoefficients[3] = vec3<f32>(SHCoefficent2.y, SHCoefficent2.z, SHCoefficent2.w) * fFactor;

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
fn debugLightView(
    worldPosition: vec4<f32>,
    iCascade: u32,
    color: vec4<f32>) -> vec4<f32>
{
    var lightViewPosition: vec4<f32> = worldPosition * shadowUniform.maLightViewProjectionMatrices[iCascade];
    lightViewPosition.x = lightViewPosition.x * 0.5f + 0.5f;
    lightViewPosition.y = lightViewPosition.y * 0.5f + 0.5f;
    lightViewPosition.z = lightViewPosition.z * 0.5f + 0.5f;

    var ret: vec4<f32> = color;
    lightViewPosition.y = 1.0f - lightViewPosition.y;
    if(lightViewPosition.x < 0.0f || lightViewPosition.x > 1.0f ||
        lightViewPosition.y < 0.0f || lightViewPosition.y > 1.0f ||
        lightViewPosition.z < 0.0f || lightViewPosition.z > 1.0f)
    {
        ret = vec4<f32>(1.0f, 1.0f, 1.0f, 1.0f);
    }

    return ret;
}