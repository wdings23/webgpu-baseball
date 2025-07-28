struct Range
{
    miStart: u32,
    miEnd: u32,
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
    maiNumMeshVertices: array<vec4<u32>, 16>,
};

struct Vertex
{
    mPosition : vec4<f32>,
    mTexCoord: vec4<f32>,
    mNormal : vec4<f32>
};

struct MeshInstanceVertexRange
{
    miStart: u32,
    miEnd: u32,
    miMeshIndex: u32,
    miMatrixStartIndex: u32,
    miMatrixEndIndex: u32
};

@group(0) @binding(0)
var<storage, read_write> aXFormVertices: array<Vertex>;

@group(1) @binding(0)
var<storage, read> aOrigVertexBuffer: array<Vertex>; 

@group(1) @binding(1)
var<storage> aiJointInfluenceIndices: array<u32>;

@group(1) @binding(2)
var<storage> afJointInfluenceWeights: array<f32>;

@group(1) @binding(3)
var<storage> aJointAnimationTotalMatrices: array<mat4x4<f32>>;

@group(1) @binding(4)
var<storage, read> aiTotalJointStartIndices: array<u32>;

@group(1) @binding(5)
var<storage, read> aMeshInstanceVertexRanges: array<MeshInstanceVertexRange>;

@group(1) @binding(6)
var<storage, read> uniformBuffer: UniformData;

const iNumThreads: u32 = 256u;

@compute
@workgroup_size(iNumThreads)
fn cs_main(
    @builtin(num_workgroups) numWorkGroups: vec3<u32>,
    @builtin(local_invocation_index) iLocalThreadIndex: u32,
    @builtin(workgroup_id) workGroup: vec3<u32>)
{
    var iVertexIndex: u32 = iLocalThreadIndex + workGroup.x * iNumThreads;
    var iOutputVertexIndex: u32 = iVertexIndex;

    // first entry is the number of meshes
    var iMesh: u32 = 0xffffffffu;
    var iStartMatrixIndex: u32 = 0xffffffffu;
    for(var i: u32 = 0; i < 32; i++)
    {
        if(iVertexIndex >= aMeshInstanceVertexRanges[i].miStart && iVertexIndex < aMeshInstanceVertexRanges[i].miEnd)
        {
            iMesh = aMeshInstanceVertexRanges[i].miMeshIndex;
            iStartMatrixIndex = aMeshInstanceVertexRanges[i].miMatrixStartIndex;
            break;
        }
    }
    if(iMesh == 0xffffffffu)
    {
        return;
    }

    let iJointStartIndex: u32 = aiTotalJointStartIndices[iMesh];

    // influence joint indices
    var aiJointInfluence: array<u32, 4>;
    aiJointInfluence[0] = aiJointInfluenceIndices[iVertexIndex * 4] + iStartMatrixIndex;
    aiJointInfluence[1] = aiJointInfluenceIndices[iVertexIndex * 4 + 1] + iStartMatrixIndex;
    aiJointInfluence[2] = aiJointInfluenceIndices[iVertexIndex * 4 + 2] + iStartMatrixIndex;
    aiJointInfluence[3] = aiJointInfluenceIndices[iVertexIndex * 4 + 3] + iStartMatrixIndex;

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

    // skinned position
    let position: vec4<f32> = aOrigVertexBuffer[iVertexIndex].mPosition;
    let skinnedPos: vec4<f32> = 
        position * xformMatrix0 * afJointInfluenceWeight[0] +
        position * xformMatrix1 * afJointInfluenceWeight[1] + 
        position * xformMatrix2 * afJointInfluenceWeight[2] + 
        position * xformMatrix3 * afJointInfluenceWeight[3];
    
    // skinned normal
    let normal: vec4<f32> = aOrigVertexBuffer[iVertexIndex].mNormal;
    let skinnedNormal: vec4<f32> = 
        normal * xformMatrix0 * afJointInfluenceWeight[0] +
        normal * xformMatrix1 * afJointInfluenceWeight[1] + 
        normal * xformMatrix2 * afJointInfluenceWeight[2] + 
        normal * xformMatrix3 * afJointInfluenceWeight[3];

    // save out
    aXFormVertices[iOutputVertexIndex].mPosition = skinnedPos;
    aXFormVertices[iOutputVertexIndex].mNormal = skinnedNormal;
    aXFormVertices[iOutputVertexIndex].mTexCoord = aOrigVertexBuffer[iVertexIndex].mTexCoord;
}





