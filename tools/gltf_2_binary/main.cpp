#include <stdint.h>
#include <math.h>
#include <assert.h>
#include <stdio.h>

#include <vector>
#include <string>
#include <map>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tinygltf/tiny_gltf.h>

#include <math/vec.h>
#include <math/mat4.h>
#include <math/quaternion.h>

#include <utils/LogPrint.h>

#include <rapidjson/document.h>

struct AnimFrame
{
    float           mfTime;
    uint32_t        miNodeIndex;
    float4          mRotation = float4(0.0f, 0.0f, 0.0f, 1.0f);
    float4          mTranslation = float4(0.0f, 0.0f, 0.0f, 1.0f);
    float4          mScaling = float4(1.0f, 1.0f, 1.0f, 1.0f);
};

struct Joint
{
    uint32_t miIndex;
    uint32_t miNumChildren = 0;
    uint32_t maiChildren[32];
    uint32_t miParent = UINT32_MAX;

    quaternion  mRotation;
    float3      mTranslation;
    float3      mScaling;
};

struct DebugJoint
{
    uint32_t miIndex;
    uint32_t miNumChildren = 0;
    uint32_t maiChildren[32];
    uint32_t miParent = UINT32_MAX;

    quaternion  mRotation;
    float3      mTranslation;
    float3      mScaling;

    std::string mName;
};

struct OutputMaterialInfo
{
    float4              mDiffuse;
    float4              mRoughnessMetallic;
    float4              mEmissive;
    uint32_t            miAlbedoTextureID = UINT32_MAX;
    uint32_t            miNormalTextureID = UINT32_MAX;
    uint32_t            miEmissiveTextureID = UINT32_MAX;
    uint32_t            miRoughnessMetallicTextureID = UINT32_MAX;
};

void saveMatrices(
    std::string const& dir,
    std::string const& baseName,
    std::vector<float4x4> const& aLocalBindMatrices,
    std::vector<float4x4> const& aGlobalBindMatrices);

void traverseRig(
    std::map<uint32_t, float4x4>& aGlobalJointPositions,
    Joint const& joint,
    float4x4 const& matrix,
    std::vector<Joint> const& aJoints,
    std::vector<std::vector<AnimFrame>> const& aaKeyFrames,
    std::vector<uint32_t> const& aiSkinJointIndices,
    std::vector<float4x4> const& aJointLocalBindMatrices,
    std::vector<float4x4> const& aJointGlobalInverseBindMatrices,
    std::vector<uint32_t> const& aiJointIndexMapping,
    std::map<uint32_t, std::string>& aJointMapping,
    uint32_t iStack);

/*
**
*/
void loadGLTF(
    std::string const& filePath,
    std::string const& outputFilePath,
    std::vector<std::vector<float3>>& aaMeshPositions,
    std::vector<std::vector<float3>>& aaMeshNormals,
    std::vector<std::vector<float2>>& aaMeshTexCoords,
    std::vector<std::vector<uint32_t>>& aaiMeshTriangleIndices,
    std::vector<std::vector<Joint>>& aaJoints,
    std::vector<std::vector<uint32_t>>& aaaiJointInfluences,
    std::vector<std::vector<float>>& aaafJointWeights,
    std::vector<std::vector<float4x4>>& aaGlobalInverseBindMatrices,
    std::vector<std::vector<float4x4>>& aaGlobalBindMatrices,
    std::vector<std::vector<uint32_t>>& aaiSkinJointIndices,
    std::vector<std::vector<Joint>>& aaAnimHierarchy,
    std::vector<std::vector<float4x4>>& aaJointLocalBindMatrices,
    std::vector<std::vector<AnimFrame>>& aaAnimationFrames,
    std::vector<std::vector<uint32_t>>& aaiJointMapIndices,
    std::map<std::string, std::map<uint32_t, std::string>>& aaJointMapping,
    std::vector<std::vector<DebugJoint>>& aaDebugJoints,
    std::vector<OutputMaterialInfo>& aOutputMaterials,
    std::vector<std::string>& aTotalAlbedoImageURI,
    std::vector<std::string>& aTotalNormalImageURI,
    std::vector<std::string>& aTotalMetallicRoughnessImageURI,
    std::vector<std::string>& aTotalEmissiveImageURI,
    std::vector<uint32_t>& aiMeshMaterialID)
{
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, filePath.c_str());
    assert(ret);

    //std::vector<std::vector<float3>> aaMeshPositions;
    //std::vector<std::vector<float3>> aaMeshNormals;
    //std::vector<std::vector<float2>> aaMeshTexCoords;
    //std::vector<std::vector<uint32_t>> aaiMeshTriangleIndices;

    {
        std::vector<std::string> aAttributeTypes;
        aAttributeTypes.push_back("POSITION");
        aAttributeTypes.push_back("NORMAL");
        aAttributeTypes.push_back("TEXCOORD_0");

        for(const auto& mesh : model.meshes)
        {
            DEBUG_PRINTF("mesh: %s primitives: %zu\n", mesh.name.c_str(), mesh.primitives.size());

            for(const auto& primitive : mesh.primitives)
            {
                // indices
                std::vector<uint32_t> aiMeshTriangleIndices;
                {
                    const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
                    const tinygltf::BufferView& indexView = model.bufferViews[indexAccessor.bufferView];
                    const tinygltf::Buffer& indexBuffer = model.buffers[indexView.buffer];

                    const unsigned char* bufferStart = indexBuffer.data.data() + indexView.byteOffset + indexAccessor.byteOffset;
                    aiMeshTriangleIndices.resize(indexAccessor.count);
                    for(size_t i = 0; i < indexAccessor.count; ++i)
                    {
                        uint32_t index = 0;
                        switch(indexAccessor.componentType) 
                        {
                            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                                index = *(reinterpret_cast<const uint8_t*>(bufferStart + i));
                                break;
                            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                                index = *(reinterpret_cast<const uint16_t*>(bufferStart + i * sizeof(uint16_t)));
                                break;
                            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                                index = *(reinterpret_cast<const uint32_t*>(bufferStart + i * sizeof(uint32_t)));
                                break;
                            default:
                                assert(0);
                                return;
                        }

                        aiMeshTriangleIndices[i] = index;
                    }

                    aaiMeshTriangleIndices.push_back(aiMeshTriangleIndices);
                }

                // attributes
                for(auto const& attributeType : aAttributeTypes)
                {
                    auto attr = primitive.attributes.find(attributeType);
                    if(attr == primitive.attributes.end())
                    {
                        continue;
                    }

                    int accessorIndex = attr->second;
                    const tinygltf::Accessor& accessor = model.accessors[accessorIndex];
                    const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
                    const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

                    size_t byteOffset = bufferView.byteOffset + accessor.byteOffset;
                    const unsigned char* dataPtr = buffer.data.data() + byteOffset;

                    DEBUG_PRINTF("\tvertex count: %zu\n", accessor.count);

                    // Assume float3 (VEC3, float)
                    if(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT &&
                        accessor.type == TINYGLTF_TYPE_VEC3)
                    {
                        uint32_t iNumPositions = (uint32_t)(bufferView.byteLength / sizeof(float3));
                        assert(bufferView.byteLength % sizeof(float3) == 0);
                        assert(iNumPositions == accessor.count);
                        std::vector<float3> aMeshPositions(iNumPositions);

                        memcpy(
                            aMeshPositions.data(),
                            dataPtr,
                            bufferView.byteLength);

                        if(attributeType == "POSITION")
                        {
                            aaMeshPositions.push_back(aMeshPositions);
                        }
                        else if(attributeType == "NORMAL")
                        {
                            aaMeshNormals.push_back(aMeshPositions);
                        }
                    }
                    else if(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT &&
                        accessor.type == TINYGLTF_TYPE_VEC2)
                    {
                        uint32_t iNumPositions = (uint32_t)(bufferView.byteLength / sizeof(float2));
                        assert(bufferView.byteLength % sizeof(float2) == 0);
                        assert(iNumPositions == accessor.count);
                        std::vector<float2> aMeshPositions(iNumPositions);

                        memcpy(
                            aMeshPositions.data(),
                            dataPtr,
                            bufferView.byteLength);

                        if(attributeType == "TEXCOORD_0")
                        {
                            aaMeshTexCoords.push_back(aMeshPositions);
                        }
                    }

                }   // for attribute

            }   // for primitive

        }   // for mesh

    }   // load mesh attributes

    // load skinning attributes
    //std::vector<std::vector<Joint>> aaJoints;
    //std::vector<std::vector<uint32_t>> aaaiJointInfluences;
    //std::vector<std::vector<float>> aaafJointWeights;
    //std::vector<std::vector<float4x4>> aaGlobalInverseBindMatrices;
    //std::vector<std::vector<float4x4>> aaGlobalBindMatrices;
    //std::vector<std::vector<uint32_t>> aaiSkinJointIndices;
    //std::vector<std::vector<Joint>> aaAnimHierarchy;
    //std::vector<std::vector<float4x4>> aaJointLocalBindMatrices;
    //std::vector<std::vector<AnimFrame>> aaAnimationFrames;
    //std::vector<std::vector<uint32_t>> aaiJointMapIndices;
    //std::map<std::string, std::map<uint32_t, std::string>> aaJointMapping;
    {
        for(const auto& mesh : model.meshes)
        {
            DEBUG_PRINTF("mesh: %s primitives: %zu\n", mesh.name.c_str(), mesh.primitives.size());

            for(const auto& primitive : mesh.primitives)
            {
                // attributes
                auto jointAttr = primitive.attributes.find("JOINTS_0");
                if(jointAttr == primitive.attributes.end())
                {
                    continue;
                }

                auto weightAttr = primitive.attributes.find("WEIGHTS_0");
                if(weightAttr == primitive.attributes.end())
                {
                    continue;
                }

                const tinygltf::Accessor& jointsAccessor = model.accessors[jointAttr->second];
                const tinygltf::Accessor& weightsAccessor = model.accessors[weightAttr->second];

                const tinygltf::BufferView& jointsView = model.bufferViews[jointsAccessor.bufferView];
                const tinygltf::BufferView& weightsView = model.bufferViews[weightsAccessor.bufferView];

                const tinygltf::Buffer& jointsBuffer = model.buffers[jointsView.buffer];
                const tinygltf::Buffer& weightsBuffer = model.buffers[weightsView.buffer];

                const uint8_t* jointsData = jointsBuffer.data.data() + jointsView.byteOffset + jointsAccessor.byteOffset;
                const uint8_t* weightsData = weightsBuffer.data.data() + weightsView.byteOffset + weightsAccessor.byteOffset;

                size_t count = jointsAccessor.count;

                std::vector<float> aafVertexWeights;
                std::vector<uint32_t> aaiJointInfluences;
                for(size_t i = 0; i < count; ++i)
                {
                    if(jointsAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
                    {
                        std::vector<uint32_t> aiJointInfluences(4);
                        std::vector<float> afJointWeights(4);

                        uint8_t const* joint = (uint8_t const*)(jointsData + i * 4); // Typically UBYTE or USHORT
                        const float* weight = (const float*)(weightsData + i * 4 * sizeof(float));

                        aaiJointInfluences.push_back((uint32_t)joint[0]);
                        aaiJointInfluences.push_back((uint32_t)joint[1]);
                        aaiJointInfluences.push_back((uint32_t)joint[2]);
                        aaiJointInfluences.push_back((uint32_t)joint[3]);

                        aafVertexWeights.push_back(weight[0]);
                        aafVertexWeights.push_back(weight[1]);
                        aafVertexWeights.push_back(weight[2]);
                        aafVertexWeights.push_back(weight[3]);

                        DEBUG_PRINTF("v %zu joint: [%d, %d, %d, %d] weights: [%.4f, %.4f, %.4f, %.4f]\n",
                            i,
                            joint[0], joint[1], joint[2], joint[3],
                            weight[0], weight[1], weight[2], weight[3]);
                    }
                    else if(jointsAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                    {
                        std::vector<uint32_t> aiJointInfluences(4);
                        std::vector<float> afJointWeights(4);

                        uint16_t const* joint = (uint16_t const*)(jointsData + i * 4); // Typically UBYTE or USHORT
                        const float* weight = (const float*)(weightsData + i * 4 * sizeof(float));

                        aaiJointInfluences.push_back((uint32_t)joint[0]);
                        aaiJointInfluences.push_back((uint32_t)joint[1]);
                        aaiJointInfluences.push_back((uint32_t)joint[2]);
                        aaiJointInfluences.push_back((uint32_t)joint[3]);

                        aafVertexWeights.push_back(weight[0]);
                        aafVertexWeights.push_back(weight[1]);
                        aafVertexWeights.push_back(weight[2]);
                        aafVertexWeights.push_back(weight[3]);

                        DEBUG_PRINTF("v %zu joint: [%d, %d, %d, %d] weights: [%.4f, %.4f, %.4f, %.4f]\n",
                            i,
                            joint[0], joint[1], joint[2], joint[3],
                            weight[0], weight[1], weight[2], weight[3]);
                    }

                }   // for i = 0 to count

                aaaiJointInfluences.push_back(aaiJointInfluences);
                aaafJointWeights.push_back(aafVertexWeights);
            }

        }   // for mesh

        // skin hiererachy
        {
            for(size_t s = 0; s < model.skins.size(); ++s) 
            {
                std::vector<Joint> aJoints;
                std::vector<DebugJoint> aDebugJoints;

                const tinygltf::Skin& skin = model.skins[s];
                DEBUG_PRINTF("skin: %s\n", skin.name.c_str());

                // Optional root node of the skeleton
                if(skin.skeleton >= 0 && skin.skeleton < model.nodes.size())
                {
                    DEBUG_PRINTF("skeleton root: %s\n", model.nodes[skin.skeleton].name.c_str());
                }

                // All joint node indices
                for(int jointIndex : skin.joints) 
                {
                    Joint joint;
                    if(jointIndex < 0 || jointIndex >= model.nodes.size())
                    {
                        continue;
                    }

                    const auto& jointNode = model.nodes[jointIndex];
                    DEBUG_PRINTF("\tjoint %s index %d\n", jointNode.name.c_str(), jointIndex);

                    
                    joint.mRotation = (jointNode.rotation.size() > 0) ? quaternion((float)jointNode.rotation[0], (float)jointNode.rotation[1], (float)jointNode.rotation[2], (float)jointNode.rotation[3]) : quaternion(0.0f, 0.0f, 0.0f, 1.0f);
                    joint.mTranslation = (jointNode.translation.size() > 0) ? float3((float)jointNode.translation[0], (float)jointNode.translation[1], (float)jointNode.translation[2]) : float3(0.0f, 0.0f, 0.0f);
                    joint.mScaling = (jointNode.scale.size() > 0) ? float3((float)jointNode.scale[0], (float)jointNode.scale[1], (float)jointNode.scale[2]) : float3(1.0f, 1.0f, 1.0f);

                    joint.miIndex = jointIndex;
                    joint.miNumChildren = 0;
                    memset(joint.maiChildren, 0xff, sizeof(joint.maiChildren));
                    for(int child : jointNode.children) 
                    {
                        const auto& childNode = model.nodes[child];
                        
                        joint.maiChildren[joint.miNumChildren++] = child;
                        
                        DEBUG_PRINTF("\t\tchild %s index %d\n", childNode.name.c_str(), child);
                    }

                    aJoints.push_back(joint);

                    DebugJoint debugJoint;
                    memcpy(&debugJoint, &joint, sizeof(Joint));
                    debugJoint.mName = jointNode.name.c_str();
                    aDebugJoints.push_back(debugJoint);
                }

                // tag joints that are children of some other joints
                std::vector<uint32_t> aiTagged(aJoints.size());
                std::vector<Joint> aRootJoints;
                for(auto const& joint : aJoints)
                {
                    for(uint32_t i = 0; i < joint.miNumChildren; i++)
                    {
                        aiTagged[joint.maiChildren[i]] = 1;
                    }
                }
                
                // get the root of the hierarchy
                for(uint32_t i = 0; i < (uint32_t)aJoints.size(); i++)
                {
                    if(aiTagged[i] == 0)
                    {
                        for(uint32_t j = 0; j < (uint32_t)aJoints.size(); j++)
                        {
                            if(aJoints[j].miIndex == i)
                            {
                                aRootJoints.push_back(aJoints[j]);
                                break;
                            }
                        }
                    }
                }

                std::vector<float4x4> aJointLocalBindMatrices;
                for(auto const& joint : aJoints)
                {
                    float4x4 rotationMatrix = joint.mRotation.matrix();
                    float4x4 translationMatrix = translate(joint.mTranslation.x, joint.mTranslation.y, joint.mTranslation.z);
                    float4x4 scaleMatrix = scale(joint.mScaling.x, joint.mScaling.y, joint.mScaling.z);

                    float4x4 localMatrix = translationMatrix * rotationMatrix * scaleMatrix;
                    aJointLocalBindMatrices.push_back(localMatrix);
                }

                aaJoints.push_back(aJoints);
                aaAnimHierarchy.push_back(aRootJoints);
                aaJointLocalBindMatrices.push_back(aJointLocalBindMatrices);

                aaDebugJoints.push_back(aDebugJoints);
            }
        }

        // skin
        for(const auto& skin : model.skins)
        {
            DEBUG_PRINTF("skin name: %s num joints: %zu\n", skin.name.c_str(), skin.joints.size());

            const tinygltf::Accessor& ibmAccessor = model.accessors[skin.inverseBindMatrices];
            const tinygltf::BufferView& ibmView = model.bufferViews[ibmAccessor.bufferView];
            const tinygltf::Buffer& ibmBuffer = model.buffers[ibmView.buffer];

            const float* ibmData = reinterpret_cast<const float*>(
                ibmBuffer.data.data() + ibmView.byteOffset + ibmAccessor.byteOffset);

            size_t jointCount = skin.joints.size();
            std::vector<float4x4> aInverseBindMatrices(jointCount);
            memcpy(
                aInverseBindMatrices.data(),
                ibmData,
                sizeof(float4x4) * jointCount);
            aaGlobalInverseBindMatrices.push_back(aInverseBindMatrices);

            std::vector<float4x4> aBindMatrices;
            for(uint32_t i = 0; i < aInverseBindMatrices.size(); i++)
            {
                float4x4 bindMatrix = invert(transpose(aInverseBindMatrices[i]));
                aBindMatrices.push_back(bindMatrix);
            }
            aaGlobalBindMatrices.push_back(aBindMatrices);

            std::vector<uint32_t> aiJointIndices(jointCount);
            memcpy(
                aiJointIndices.data(),
                skin.joints.data(),
                jointCount * sizeof(uint32_t)
            );
            aaiSkinJointIndices.push_back(aiJointIndices);

            for(uint32_t i = 0; i < aiJointIndices.size(); i++)
            {
                uint32_t iJointIndex = aiJointIndices[i];
                aaJointMapping[skin.name][iJointIndex] = model.nodes[iJointIndex].name;
            }

            for(auto const& keyValue : aaJointMapping[skin.name])
            {
                DEBUG_PRINTF("\tkey: %d value: %s\n", keyValue.first, keyValue.second.c_str());
            }

        }   // for skin

        for(uint32_t i = 0; i < (uint32_t)aaiSkinJointIndices.size(); i++)
        {
            std::vector<uint32_t> aiJointMapIndices(aaiSkinJointIndices[i].size());
            for(uint32_t j = 0; j < (uint32_t)aaiSkinJointIndices[i].size(); j++)
            {
                uint32_t iJointIndex = aaiSkinJointIndices[i][j];
                aiJointMapIndices[iJointIndex] = j;
            }

            aaiJointMapIndices.push_back(aiJointMapIndices);
        }

        for(uint32_t i = 0; i < (uint32_t)aaJoints.size(); i++)
        {
            for(auto const& joint : aaJoints[i])
            {
                for(uint32_t j = 0; j < joint.miNumChildren; j++)
                {
                    uint32_t iArrayIndex = aaiJointMapIndices[0][joint.maiChildren[j]];
                    aaJoints[i][iArrayIndex].miParent = joint.miIndex;
                    aaDebugJoints[i][iArrayIndex].miParent = joint.miIndex;
                }
            }
        }

    }   // skinning attributes

    // animation
    {
        std::map<std::string, uint32_t> aNodeToJointNameMappings;
        for(size_t i = 0; i < model.animations.size(); ++i) 
        {
            const tinygltf::Animation& anim = model.animations[i];
            DEBUG_PRINTF("animation %zu name: %s\n", i, anim.name.c_str());

            std::vector<float4> aTranslations;
            std::vector<float4> aRotations;
            std::vector<float4> aScalings;
            std::vector<std::vector<float>> aafTime;
            std::vector<float> aafRotationTimes;
            std::vector<uint32_t> aiNodeIndices;
            std::vector<std::string> aNodeNames;
            uint32_t iChannel = 0;
            for(const auto& channel : anim.channels) 
            {
                const tinygltf::AnimationSampler& sampler = anim.samplers[channel.sampler];
                const std::string& path = channel.target_path; // "translation", "rotation", "scale", or "weights"
                int nodeIndex = channel.target_node;
                const std::string& nodeName = (nodeIndex >= 0 && nodeIndex < model.nodes.size()) ? model.nodes[nodeIndex].name : "<unnamed>";

                aNodeToJointNameMappings[nodeName] = nodeIndex;
                DEBUG_PRINTF("\tnode: %s index: %d\n", nodeName.c_str(), nodeIndex);

                // Load input times
                const auto& inputAccessor = model.accessors[sampler.input];
                const auto& inputBufferView = model.bufferViews[inputAccessor.bufferView];
                const auto& inputBuffer = model.buffers[inputBufferView.buffer];
                const float* timeData = reinterpret_cast<const float*>(
                    inputBuffer.data.data() + inputBufferView.byteOffset + inputAccessor.byteOffset);

                // Load output values
                const auto& outputAccessor = model.accessors[sampler.output];
                const auto& outputBufferView = model.bufferViews[outputAccessor.bufferView];
                const auto& outputBuffer = model.buffers[outputBufferView.buffer];
                const float* valueData = reinterpret_cast<const float*>(
                    outputBuffer.data.data() + outputBufferView.byteOffset + outputAccessor.byteOffset);

                DEBUG_PRINTF("\t\tnum key frames: %zu\n", inputAccessor.count);

                // channel data
                std::vector<float> afTime;
                for(size_t j = 0; j < inputAccessor.count; ++j) 
                {
                    afTime.push_back(timeData[j]);
                    aNodeNames.push_back(nodeName);
                    aiNodeIndices.push_back(nodeIndex);

                    if(path == "translation") 
                    {
                        aTranslations.push_back(float4(valueData[j * 3], valueData[j * 3 + 1], valueData[j * 3 + 2], 0.0f));
                        DEBUG_PRINTF("\t\t\ttime: %.4f %s (%.4f, %.4f, %4f)\n", 
                            timeData[j],
                            path.c_str(),
                            valueData[j * 3], 
                            valueData[j * 3 + 1], 
                            valueData[j * 3 + 2]);
                    }
                    else if(path == "scale")
                    {
                        aScalings.push_back(float4(valueData[j * 3], valueData[j * 3 + 1], valueData[j * 3 + 2], 0.0f));
                        DEBUG_PRINTF("\t\t\ttime: %.4f %s (%.4f, %.4f, %4f)\n",
                            timeData[j],
                            path.c_str(),
                            valueData[j * 3],
                            valueData[j * 3 + 1],
                            valueData[j * 3 + 2]);
                    }
                    else if(path == "rotation") 
                    {
                        aafRotationTimes.push_back(timeData[j]);
                        aRotations.push_back(float4(valueData[j * 4], valueData[j * 4 + 1], valueData[j * 4 + 2], valueData[j * 4 + 3]));
                        DEBUG_PRINTF("\t\t\ttime: %.4f %s (%.4f, %.4f, %4f, %.4f)\n", 
                            timeData[j],
                            path.c_str(),
                            valueData[j * 4], 
                            valueData[j * 4 + 1], 
                            valueData[j * 4 + 2], 
                            valueData[j * 4 + 3]);
                    }

                }   // for accessor
            
                ++iChannel;

                if(iChannel > 0 && iChannel % 3 == 0)
                {
                    //assert(aRotations.size() == aTranslations.size() && aTranslations.size() == aScalings.size());
                    aafTime.push_back(afTime);
                    std::vector<AnimFrame> aAnimationFrames(aRotations.size());
                    for(uint32_t i = 0; i < aRotations.size(); i++)
                    {
                        AnimFrame& animationFrame = aAnimationFrames[i];

                        animationFrame.mRotation = aRotations[i];
                        animationFrame.mTranslation = (i >= aTranslations.size()) ? aTranslations[aTranslations.size() - 1] : aTranslations[i];
                        animationFrame.mScaling = float4(1.0f, 1.0f, 1.0f, 1.0f); //aScalings[i];
                        animationFrame.mfTime = aafRotationTimes[i];
                        animationFrame.miNodeIndex = aiNodeIndices[i];
                    }

                    aaAnimationFrames.push_back(aAnimationFrames);

                    aTranslations.clear();
                    aScalings.clear();
                    aRotations.clear();
                    afTime.clear();
                    aiNodeIndices.clear();
                }

            }   // for channel

        }   // for animation

    }   // animation 

    uint32_t iNumMeshes = (uint32_t)aaMeshPositions.size();
    uint32_t iNumAnimations = (uint32_t)aaGlobalInverseBindMatrices.size();
   
    // save out mesh data
    FILE* fp = fopen(outputFilePath.c_str(), "wb");
    fwrite(&iNumMeshes, sizeof(uint32_t), iNumMeshes, fp);
    fwrite(&iNumAnimations, sizeof(uint32_t), iNumAnimations, fp);
    for(uint32_t i = 0; i < aaMeshPositions.size(); i++)
    {
        std::vector<float3> const& aMeshPositions = aaMeshPositions[i];
        uint32_t iNumMeshPositions = (uint32_t)aMeshPositions.size();
        fwrite(&iNumMeshPositions, sizeof(uint32_t), 1, fp);
        fwrite(aMeshPositions.data(), sizeof(float3), iNumMeshPositions, fp);

        std::vector<float3> const& aMeshNormals = aaMeshNormals[i];
        uint32_t iNumMeshNormals = (uint32_t)aMeshNormals.size();
        fwrite(&iNumMeshNormals, sizeof(uint32_t), 1, fp);
        fwrite(aMeshNormals.data(), sizeof(float3), iNumMeshNormals, fp);

        std::vector<float2> const& aMeshTexCoords = aaMeshTexCoords[i];
        uint32_t iNumMeshTexCoords = (uint32_t)aMeshTexCoords.size();
        fwrite(&iNumMeshTexCoords, sizeof(uint32_t), 1, fp);
        fwrite(aMeshTexCoords.data(), sizeof(float2), iNumMeshTexCoords, fp);

        std::vector<uint32_t> const& aiMeshTriangleIndices = aaiMeshTriangleIndices[i];
        uint32_t iNumTriangleIndices = (uint32_t)aiMeshTriangleIndices.size();
        fwrite(&iNumTriangleIndices, sizeof(uint32_t), 1, fp);
        fwrite(aiMeshTriangleIndices.data(), sizeof(uint32_t), iNumTriangleIndices, fp);
    }

    // save out animations
    for(uint32_t i = 0; i < iNumAnimations; i++)
    {
        uint32_t iNumTotalJointWeights = (uint32_t)aaaiJointInfluences[i].size();
        std::vector<uint32_t> const& aaiJointInfluences = aaaiJointInfluences[i];
        std::vector<float> const& aafJointInfluences = aaafJointWeights[i];

        assert(iNumTotalJointWeights == aaMeshPositions[i].size() * 4);

        fwrite(&iNumTotalJointWeights, sizeof(uint32_t), 1, fp);
        fwrite(aaiJointInfluences.data(), sizeof(uint32_t), iNumTotalJointWeights, fp);
        fwrite(&iNumTotalJointWeights, sizeof(uint32_t), 1, fp);
        fwrite(aafJointInfluences.data(), sizeof(float), iNumTotalJointWeights, fp);

        std::vector<uint32_t> const& aiSkinJointIndices = aaiSkinJointIndices[i];
        uint32_t iNumJoints = (uint32_t)aiSkinJointIndices.size();
        fwrite(&iNumJoints, sizeof(uint32_t), 1, fp);
        fwrite(aiSkinJointIndices.data(), sizeof(uint32_t), iNumJoints, fp);

        std::vector<float4x4> const& aInverseBindMatrices = aaGlobalInverseBindMatrices[i];
        iNumJoints = (uint32_t)aInverseBindMatrices.size();
        fwrite(&iNumJoints, sizeof(uint32_t), 1, fp);
        fwrite(aInverseBindMatrices.data(), sizeof(float4x4), iNumJoints, fp);
    }
    
    // animation hierarchy
    uint32_t iNumAnimHierarchies = (uint32_t)aaJoints.size();
    assert(iNumAnimHierarchies == aaJointLocalBindMatrices.size());
    fwrite(&iNumAnimHierarchies, sizeof(uint32_t), 1, fp);
    for(uint32_t i = 0; i < iNumAnimHierarchies; i++)
    {
        std::vector<Joint> const& aJoints = aaJoints[i];
        uint32_t iNumJoints = (uint32_t)aJoints.size();
        fwrite(&iNumJoints, sizeof(uint32_t), 1, fp);
        fwrite(aJoints.data(), sizeof(Joint), iNumJoints, fp);
        fwrite(aaJointLocalBindMatrices[i].data(), sizeof(float4x4), iNumJoints, fp);
    }

    // joint to array mapping
    uint32_t iNumJointMapIndices = (uint32_t)aaiJointMapIndices.size();
    fwrite(&iNumJointMapIndices, sizeof(uint32_t), 1, fp);
    for(uint32_t i = 0; i < (uint32_t)aaiJointMapIndices.size(); i++)
    {
        uint32_t iNumJoints = (uint32_t)aaiJointMapIndices[i].size();
        fwrite(&iNumJoints, sizeof(uint32_t), 1, fp);
        fwrite(aaiJointMapIndices[i].data(), sizeof(uint32_t), iNumJoints, fp);
    }

    // save out joint mapping
    uint32_t iNumJointMappings = (uint32_t)aaJointMapping.size();
    fwrite(&iNumJointMappings, sizeof(uint32_t), 1, fp);
    for(auto const& aJointMappingKeyValue : aaJointMapping)
    {
        uint32_t iNumJoints = (uint32_t)aJointMappingKeyValue.second.size();
        fwrite(&iNumJoints, sizeof(uint32_t), 1, fp);

        for(auto const& keyValue : aJointMappingKeyValue.second)
        {
            fwrite(&keyValue.first, sizeof(uint32_t), 1, fp);
            uint32_t iLength = (uint32_t)keyValue.second.length();
            fwrite(&iLength, sizeof(uint32_t), 1, fp);
            for(uint32_t j = 0; j < iLength; j++)
            {
                fwrite(&keyValue.second.at(j), sizeof(char), 1, fp);
            }
        }
    }

    fclose(fp);

    
    std::map<uint32_t, float4x4> aGlobalJointAnimatedMatrices;
    float4x4 rootMatrix;
    traverseRig(
        aGlobalJointAnimatedMatrices,
        aaAnimHierarchy[0].front(),
        rootMatrix,
        aaJoints[0],
        aaAnimationFrames,
        aaiSkinJointIndices[0],
        aaJointLocalBindMatrices[0],
        aaGlobalInverseBindMatrices[0],
        aaiJointMapIndices[0],
        aaJointMapping["Armature"],
        0
    );

    std::map<uint32_t, float4x4> aGlobalJointBindMatrices;
    for(uint32_t i = 0; i < (uint32_t)aaGlobalInverseBindMatrices[0].size(); i++)
    {
        Joint const& joint = aaJoints[0][i];
        float4x4 m = invert(transpose(aaGlobalInverseBindMatrices[0][i]));
        aGlobalJointBindMatrices[joint.miIndex] = m;
    }

    // Assuming you have a material and it references a texture
    for(uint32_t iMaterial = 0; iMaterial < model.materials.size(); iMaterial++)
    {
        OutputMaterialInfo outputMaterial;

        const tinygltf::Material& material = model.materials[iMaterial];
        if(material.pbrMetallicRoughness.baseColorTexture.index >= 0)
        {
            const tinygltf::Texture& texture = model.textures[material.pbrMetallicRoughness.baseColorTexture.index];
            const tinygltf::Image& image = model.images[texture.source];

            aTotalAlbedoImageURI.push_back(image.uri);
            outputMaterial.miAlbedoTextureID = (uint32_t)aTotalAlbedoImageURI.size() - 1;
        }

        if(material.normalTexture.index >= 0)
        {
            const tinygltf::Texture& texture = model.textures[material.normalTexture.index];
            const tinygltf::Image& image = model.images[texture.source];

            aTotalNormalImageURI.push_back(image.uri);
            outputMaterial.miNormalTextureID = (uint32_t)aTotalNormalImageURI.size() - 1;
        }

        if(material.emissiveTexture.index >= 0)
        {
            const tinygltf::Texture& texture = model.textures[material.normalTexture.index];
            const tinygltf::Image& image = model.images[texture.source];

            aTotalEmissiveImageURI.push_back(image.uri);
            outputMaterial.miEmissiveTextureID = (uint32_t)aTotalEmissiveImageURI.size() - 1;
        }

        if(material.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0)
        {
            const tinygltf::Texture& texture = model.textures[material.normalTexture.index];
            const tinygltf::Image& image = model.images[texture.source];

            aTotalMetallicRoughnessImageURI.push_back(image.uri);
            outputMaterial.miRoughnessMetallicTextureID = (uint32_t)aTotalMetallicRoughnessImageURI.size() - 1;
        }

        outputMaterial.mDiffuse = float4(
            (float)material.pbrMetallicRoughness.baseColorFactor[0],
            (float)material.pbrMetallicRoughness.baseColorFactor[1],
            (float)material.pbrMetallicRoughness.baseColorFactor[2],
            (float)material.pbrMetallicRoughness.baseColorFactor[3]
        );

        outputMaterial.mEmissive = float4(
            (float)material.emissiveFactor[0],
            (float)material.emissiveFactor[1],
            (float)material.emissiveFactor[2],
            1.0f
        );

        outputMaterial.mRoughnessMetallic = float4(
            (float)material.pbrMetallicRoughness.roughnessFactor,
            (float)material.pbrMetallicRoughness.metallicFactor,
            0.0f,
            1.0f
        );

        aOutputMaterials.push_back(outputMaterial);
    }

    for(auto mesh : model.meshes)
    {
        aiMeshMaterialID.push_back(mesh.primitives[0].material);
    }
}

/*
**
*/
void traverseRig(
    std::map<uint32_t, float4x4>& aGlobalJointPositions,
    Joint const& joint,
    float4x4 const& matrix,
    std::vector<Joint> const& aJoints,
    std::vector<std::vector<AnimFrame>> const& aaKeyFrames,
    std::vector<uint32_t> const& aiSkinJointIndices,
    std::vector<float4x4> const& aJointLocalBindMatrices,
    std::vector<float4x4> const& aJointGlobalInverseBindMatrices,
    std::vector<uint32_t> const& aiJointIndexMapping,
    std::map<uint32_t, std::string>& aJointMapping,
    uint32_t iStack)
{
    std::string jointName = aJointMapping[joint.miIndex];

#if 0
    for(uint32_t i = 0; i < iStack; i++)
    {
        DEBUG_PRINTF("     ");
    }
    DEBUG_PRINTF("%s ", jointName.c_str());
    DEBUG_PRINTF("(%.4f, %.4f, %.4f)\n",
        matrix.mafEntries[3],
        matrix.mafEntries[7],
        matrix.mafEntries[11]
    );
#endif // #if 0

    float const fTime = 3.0f;

    uint32_t iArrayIndex = aiJointIndexMapping[joint.miIndex];
    std::vector<AnimFrame> const& aKeyFrames = aaKeyFrames[iArrayIndex];

    AnimFrame keyFrame;
    {
        uint32_t iKeyFrame = 0, iPrevKeyFrame = 0;
        for(uint32_t i = 0; i < (uint32_t)aKeyFrames.size(); i++)
        {
            if(aKeyFrames[i].mfTime > fTime)
            {
                iKeyFrame = i;
                if(i > 0)
                {
                    iPrevKeyFrame = i - 1;
                }

                break;
            }
        }

        AnimFrame const& prevKeyFrame = aKeyFrames[iPrevKeyFrame];
        AnimFrame const& currKeyFrame = aKeyFrames[iKeyFrame];

        float fDuration = currKeyFrame.mfTime - prevKeyFrame.mfTime;
        float fPct = (fTime - prevKeyFrame.mfTime) / fDuration;
        keyFrame.mTranslation = prevKeyFrame.mTranslation + (currKeyFrame.mTranslation - prevKeyFrame.mTranslation) * fPct;
        keyFrame.mScaling = prevKeyFrame.mScaling + (currKeyFrame.mScaling - prevKeyFrame.mScaling) * fPct;
        keyFrame.mRotation.x = prevKeyFrame.mRotation.x + (currKeyFrame.mRotation.x - prevKeyFrame.mRotation.x) * fPct;
        keyFrame.mRotation.y = prevKeyFrame.mRotation.y + (currKeyFrame.mRotation.y - prevKeyFrame.mRotation.y) * fPct;
        keyFrame.mRotation.z = prevKeyFrame.mRotation.z + (currKeyFrame.mRotation.z - prevKeyFrame.mRotation.z) * fPct;
        keyFrame.mRotation.w = prevKeyFrame.mRotation.w + (currKeyFrame.mRotation.w - prevKeyFrame.mRotation.w) * fPct;
    }

    float4x4 translateMatrix = translate(keyFrame.mTranslation);
    float4x4 scaleMatrix = scale(keyFrame.mScaling);
    quaternion q = quaternion(keyFrame.mRotation.x, keyFrame.mRotation.y, keyFrame.mRotation.z, keyFrame.mRotation.w);
    float4x4 rotationMatrix = q.matrix();
    float4x4 localAnimMatrix = translateMatrix * rotationMatrix * scaleMatrix;
    float4x4 totalMatrix = matrix * localAnimMatrix;

    aGlobalJointPositions[joint.miIndex] = totalMatrix;

    float4x4 globalBindMatrix = invert(transpose(aJointGlobalInverseBindMatrices[iArrayIndex]));

    //DEBUG_PRINTF("draw_sphere([%.4f, %.4f, %.4f], 0.01, 255, 0, 0, 255, \"%s\")\n",
    //    globalBindMatrix.mafEntries[3],
    //    globalBindMatrix.mafEntries[7],
    //    globalBindMatrix.mafEntries[11],
    //    jointName.c_str()
    //);

    //DEBUG_PRINTF("draw_sphere([%.4f, %.4f, %.4f], 0.01, 255, 0, 0, 255, \"%s\")\n",
    //    totalMatrix.mafEntries[3],
    //    totalMatrix.mafEntries[7],
    //    totalMatrix.mafEntries[11],
    //    jointName.c_str()
    //);

    for(uint32_t i = 0; i < joint.miNumChildren; i++)
    {
        uint32_t iChildJointIndex = joint.maiChildren[i];
        uint32_t iChildArrayIndex = aiJointIndexMapping[iChildJointIndex];

        Joint const* pChildJoint = &aJoints[iChildArrayIndex];

        if(pChildJoint != nullptr)
        {
            traverseRig(
                aGlobalJointPositions,
                *pChildJoint,
                totalMatrix,
                aJoints,
                aaKeyFrames,
                aiSkinJointIndices,
                aJointLocalBindMatrices,
                aJointGlobalInverseBindMatrices,
                aiJointIndexMapping,
                aJointMapping,
                iStack + 1);
        }
    }
}

/*
**
*/
void updateHierarchy(
    Joint const& joint,
    float4x4 const& parentMatrix,
    std::vector<float4x4>& aTotalMatrices,
    std::vector<float4x4> const& aDstAnimationMatchingMatrices,
    std::vector<float4> const& aDstAxisAngle,
    std::vector<float4x4> const& aDstGlobalBindMatrices,
    std::vector<Joint> const& aDstJoints,
    std::vector<uint32_t> const& aiDstJointMapIndices)
{
    uint32_t iArrayIndex = aiDstJointMapIndices[joint.miIndex];

    float4x4& totalMatrix = aTotalMatrices[iArrayIndex];
    totalMatrix = aDstGlobalBindMatrices[iArrayIndex] * aDstAnimationMatchingMatrices[iArrayIndex];

    for(uint32_t i = 0; i < joint.miNumChildren; i++)
    {
        uint32_t iChildJointIndex = joint.maiChildren[i];
        uint32_t iChildArrayIndex = aiDstJointMapIndices[iChildJointIndex];
        Joint const& childJoint = aDstJoints[iChildArrayIndex];

        updateHierarchy(
            childJoint,
            totalMatrix,
            aTotalMatrices,
            aDstAnimationMatchingMatrices,
            aDstAxisAngle,
            aDstGlobalBindMatrices,
            aDstJoints,
            aiDstJointMapIndices
        );
    }
}

/*
**
*/
void testTraverse(
    Joint const& joint,
    float4x4 const& parentMatrix,
    std::vector<Joint> const& aJoints,
    std::vector<float4x4> const& aLocalBindMatrices,
    std::vector<uint32_t> const& aiJointToArrayIndices,
    std::map<uint32_t, std::string>& aJointMapping)
{
    uint32_t iArrayIndex = aiJointToArrayIndices[joint.miIndex];
    float4x4 const& localMatrix = aLocalBindMatrices[iArrayIndex];
    float4x4 totalMatrix = parentMatrix * localMatrix;
    std::string jointName = aJointMapping[joint.miIndex];
    /*DEBUG_PRINTF("draw_sphere([%.4f, %.4f, %.4f], 0.01, 255, 0, 0, 255, \"%s\")\n",
        totalMatrix.mafEntries[3],
        totalMatrix.mafEntries[7],
        totalMatrix.mafEntries[11],
        jointName.c_str()
    );*/
    
    for(uint32_t iChild = 0; iChild < joint.miNumChildren; iChild++)
    {
        uint32_t iChildArrayIndex = aiJointToArrayIndices[joint.maiChildren[iChild]];
        Joint const& childJoint = aJoints[iChildArrayIndex];
        testTraverse(
            childJoint,
            totalMatrix,
            aJoints,
            aLocalBindMatrices,
            aiJointToArrayIndices,
            aJointMapping
        );
    }
}

/*
**
*/
void computeLocalBindMatrices(
    std::vector<float4x4>& aLocalBindMatrices,
    std::vector<float4x4> const& aGlobalBindMatrices,
    std::vector<Joint> const& aJoints,
    std::vector<uint32_t> const& aiJointToArrayIndices,
    std::map<uint32_t, std::string>& aJointMapping)
{
    for(uint32_t iJoint = 0; iJoint < (uint32_t)aJoints.size(); iJoint++)
    {
        if(aJoints[iJoint].miParent >= aJoints.size())
        {
            aLocalBindMatrices.push_back(aGlobalBindMatrices[iJoint]);
        }
        else
        {
            uint32_t iParentArrayIndex = aiJointToArrayIndices[aJoints[iJoint].miParent];
            float4x4 const& parentGlobalBindMatrix = aGlobalBindMatrices[iParentArrayIndex];
            
            float4x4 localMatrix = invert(parentGlobalBindMatrix) * aGlobalBindMatrices[iJoint];
            aLocalBindMatrices.push_back(localMatrix);
        }
    }

    testTraverse(
        aJoints[0],
        float4x4(),
        aJoints,
        aLocalBindMatrices,
        aiJointToArrayIndices, 
        aJointMapping
    );
}

/*
**
*/
void computeLocalAnimationMatrices(
    std::vector<std::vector<float4x4>>& aaLocalAnimationMatrices,
    std::vector<std::vector<AnimFrame>>& aaLocalAnimationKeyFrames,
    std::vector<float4x4> const& aLocalBindMatrices,
    std::vector<AnimFrame> const& aKeyFrames,
    std::vector<Joint> const& aJoints,
    std::vector<uint32_t> const& aiJointToArrayIndices,
    std::map<uint32_t, std::string>& aJointMapping)
{
    uint32_t iJointIndex = aKeyFrames[0].miNodeIndex;
    uint32_t iArrayIndex = aiJointToArrayIndices[iJointIndex];
    std::string jointName = aJointMapping[iJointIndex];
    
    for(uint32_t iKeyFrame = 0; iKeyFrame < aKeyFrames.size(); iKeyFrame++)
    {
        quaternion q = quaternion(aKeyFrames[iKeyFrame].mRotation.x, aKeyFrames[iKeyFrame].mRotation.y, aKeyFrames[iKeyFrame].mRotation.z, aKeyFrames[iKeyFrame].mRotation.w);
        float4x4 rotationMatrix = q.matrix();
        float4x4 translationMatrix = translate(aKeyFrames[iKeyFrame].mTranslation);
        float4x4 scaleMatrix = scale(aKeyFrames[iKeyFrame].mScaling);

        float4x4 localMatrix = translationMatrix * rotationMatrix * scaleMatrix;
        float4x4 const& localBindMatrix = aLocalBindMatrices[iArrayIndex];

        float4x4 localAnimationMatrix = invert(localBindMatrix) * localMatrix;
        aaLocalAnimationMatrices[iArrayIndex].push_back(localAnimationMatrix);

        float fScaleX = length(float3(localAnimationMatrix.mafEntries[0], localAnimationMatrix.mafEntries[1], localAnimationMatrix.mafEntries[2]));
        float fScaleY = length(float3(localAnimationMatrix.mafEntries[4], localAnimationMatrix.mafEntries[5], localAnimationMatrix.mafEntries[6]));
        float fScaleZ = length(float3(localAnimationMatrix.mafEntries[8], localAnimationMatrix.mafEntries[9], localAnimationMatrix.mafEntries[10]));

        float fOneOverScaleX = 1.0f / fScaleX;
        float fOneOverScaleY = 1.0f / fScaleY;
        float fOneOverScaleZ = 1.0f / fScaleZ;
        localAnimationMatrix.mafEntries[0] *= fOneOverScaleX; localAnimationMatrix.mafEntries[1] *= fOneOverScaleX; localAnimationMatrix.mafEntries[2] *= fOneOverScaleX;
        localAnimationMatrix.mafEntries[4] *= fOneOverScaleX; localAnimationMatrix.mafEntries[5] *= fOneOverScaleX; localAnimationMatrix.mafEntries[6] *= fOneOverScaleX;
        localAnimationMatrix.mafEntries[8] *= fOneOverScaleX; localAnimationMatrix.mafEntries[9] *= fOneOverScaleX; localAnimationMatrix.mafEntries[10] *= fOneOverScaleX;

        float3 translation = float3(localAnimationMatrix.mafEntries[3], localAnimationMatrix.mafEntries[7], localAnimationMatrix.mafEntries[11]);
        localAnimationMatrix.mafEntries[3] = localAnimationMatrix.mafEntries[7] = localAnimationMatrix.mafEntries[11] = 0.0f;
        quaternion newQ = q.fromMatrix(localAnimationMatrix);
        
        AnimFrame localAnimFrame = aKeyFrames[iKeyFrame];
        localAnimFrame.mRotation = float4(newQ.x, newQ.y, newQ.z, newQ.w);
        localAnimFrame.mTranslation = float4(translation, 1.0f);
        localAnimFrame.mScaling = float4(fScaleX, fScaleY, fScaleZ, 1.0f);
        aaLocalAnimationKeyFrames[iArrayIndex].push_back(localAnimFrame);

        float4x4 verifyScaleMatrix = scale(fScaleX, fScaleY, fScaleZ);
        float4x4 verifyRotationMatrix = newQ.matrix();
        float4x4 verifyTranslationMatrix = translate(translation.x, translation.y, translation.z);
        float4x4 verifyAnimMatrix = verifyTranslationMatrix * verifyRotationMatrix * verifyScaleMatrix;
        float4x4 verifyLocalMatrix = localBindMatrix * verifyAnimMatrix;
        bool bIdentical = verifyLocalMatrix.identical(localMatrix, 0.1f);
        assert(bIdentical);
    }
    
}

/*
**
*/
void testTraverseAnimation(
    //std::vector<float3>& aGlobalJointPositions,
    std::vector<float4x4>& aGlobalJointMatrices,
    Joint const& joint,
    float4x4 const& parentMatrix,
    std::vector<Joint> const& aJoints,
    std::vector<std::vector<AnimFrame>> const& aaLocalAnimationKeyFrames,
    std::vector<float4x4> const& aLocalBindMatrices,
    std::vector<uint32_t> const& aiJointToArrayIndices,
    std::map<uint32_t, std::string>& aJointMapping,
    float fTime)
{
    uint32_t iJointIndex = joint.miIndex;
    uint32_t iArrayIndex = aiJointToArrayIndices[iJointIndex];
    std::string jointName = aJointMapping[iJointIndex];
    
    uint32_t iJointKeyFrameIndex = UINT32_MAX;
    for(uint32_t i = 0; i < aaLocalAnimationKeyFrames.size(); i++)
    {
        if(aaLocalAnimationKeyFrames[i][0].miNodeIndex == iJointIndex)
        {
            iJointKeyFrameIndex = i;
            break;
        }
    }
    assert(iJointKeyFrameIndex != UINT32_MAX);
    assert(iJointKeyFrameIndex == iArrayIndex);

    quaternion q;

    AnimFrame keyFrame;
    std::vector<AnimFrame> const& aKeyFrames = aaLocalAnimationKeyFrames[iJointKeyFrameIndex];
    for(uint32_t iFrameIndex = 0; iFrameIndex < aaLocalAnimationKeyFrames[iJointKeyFrameIndex].size(); iFrameIndex++)
    {
        if(aKeyFrames[iFrameIndex].mfTime >= fTime)
        {
            uint32_t iPrevFrameIndex = (iFrameIndex > 0) ? iFrameIndex - 1 : 0;
            AnimFrame const& currFrame = aKeyFrames[iFrameIndex];
            AnimFrame const& prevFrame = aKeyFrames[iPrevFrameIndex];
            
            float fTimePct = (currFrame.mfTime - prevFrame.mfTime > 0.0f) ? (fTime - prevFrame.mfTime) / (currFrame.mfTime - prevFrame.mfTime) : 0.0f;
            keyFrame.mScaling = prevFrame.mScaling + (currFrame.mScaling - prevFrame.mScaling) * fTimePct;
            keyFrame.mTranslation = prevFrame.mTranslation + (currFrame.mTranslation - prevFrame.mTranslation) * fTimePct;
           
            quaternion prevQ = quaternion(prevFrame.mRotation);
            quaternion currQ = quaternion(currFrame.mRotation);
            q = quaternion::slerp(prevQ, currQ, fTimePct);

            break;
        }
    }

    float4x4 translationMatrix = translate(keyFrame.mTranslation);
    float4x4 scaleMatrix = scale(keyFrame.mScaling);
    float4x4 rotationMatrix = q.matrix();
    float4x4 localAnimationMatrix = translationMatrix * rotationMatrix * scaleMatrix;
    
    float4x4 const& localBindMatrix = aLocalBindMatrices[iArrayIndex];
    float4x4 localMatrix = localBindMatrix * localAnimationMatrix;
    float4x4 totalMatrix = parentMatrix * localMatrix;
    /*DEBUG_PRINTF("draw_sphere([%.4f, %.4f, %.4f], 0.01, 255, 0, 0, 255, \"%s\")\n",
        totalMatrix.mafEntries[3],
        totalMatrix.mafEntries[7],
        totalMatrix.mafEntries[11],
        jointName.c_str()
    );*/

    //aGlobalJointPositions.push_back(float3(totalMatrix.mafEntries[3], totalMatrix.mafEntries[7], totalMatrix.mafEntries[11]));
    aGlobalJointMatrices.push_back(totalMatrix);

    for(uint32_t iChild = 0; iChild < joint.miNumChildren; iChild++)
    {
        uint32_t iChildArrayIndex = aiJointToArrayIndices[joint.maiChildren[iChild]];
        Joint childJoint = aJoints[iChildArrayIndex];
        testTraverseAnimation(
            //aGlobalJointPositions,
            aGlobalJointMatrices,
            childJoint,
            totalMatrix,
            aJoints,
            aaLocalAnimationKeyFrames,
            aLocalBindMatrices,
            aiJointToArrayIndices,
            aJointMapping,
            fTime
        );
    }
}

/*
**
*/
void testTraverseAnimationNoLocalBind(
    Joint const& joint,
    float4x4 const& parentMatrix,
    std::vector<Joint> const& aJoints,
    std::vector<std::vector<AnimFrame>> const& aaKeyFrames,
    std::vector<uint32_t> const& aiJointToArrayIndices,
    std::map<uint32_t, std::string>& aJointMapping,
    float fTime)
{
    uint32_t iJointIndex = joint.miIndex;
    uint32_t iArrayIndex = aiJointToArrayIndices[iJointIndex];
    std::string jointName = aJointMapping[iJointIndex];

    uint32_t iJointKeyFrameIndex = UINT32_MAX;
    for(uint32_t i = 0; i < aaKeyFrames.size(); i++)
    {
        if(aaKeyFrames[i][0].miNodeIndex == iJointIndex)
        {
            iJointKeyFrameIndex = i;
            break;
        }
    }
    assert(iJointKeyFrameIndex != UINT32_MAX);
    assert(iJointKeyFrameIndex == iArrayIndex);

    quaternion q;
    AnimFrame keyFrame;
    std::vector<AnimFrame> const& aKeyFrames = aaKeyFrames[iJointKeyFrameIndex];
    for(uint32_t iFrameIndex = 0; iFrameIndex < aaKeyFrames[iJointKeyFrameIndex].size(); iFrameIndex++)
    {
        if(aKeyFrames[iFrameIndex].mfTime >= fTime)
        {
            uint32_t iPrevFrameIndex = (iFrameIndex > 0) ? iFrameIndex - 1 : 0;
            AnimFrame const& currFrame = aKeyFrames[iFrameIndex];
            AnimFrame const& prevFrame = aKeyFrames[iPrevFrameIndex];

            float fTimePct = (currFrame.mfTime - prevFrame.mfTime > 0.0f) ? (fTime - prevFrame.mfTime) / (currFrame.mfTime - prevFrame.mfTime) : 0.0f;
            keyFrame.mScaling = prevFrame.mScaling + (currFrame.mScaling - prevFrame.mScaling) * fTimePct;
            keyFrame.mTranslation = prevFrame.mTranslation + (currFrame.mTranslation - prevFrame.mTranslation) * fTimePct;
            
            q = quaternion::slerp(quaternion(prevFrame.mRotation), quaternion(currFrame.mRotation), fTimePct);

            break;
        }
    }

    float4x4 translationMatrix = translate(keyFrame.mTranslation);
    float4x4 scaleMatrix = scale(keyFrame.mScaling);
    float4x4 rotationMatrix = q.matrix();
    float4x4 localAnimationMatrix = translationMatrix * rotationMatrix * scaleMatrix;
    float4x4 totalMatrix = parentMatrix * localAnimationMatrix;
    /*DEBUG_PRINTF("draw_sphere([%.4f, %.4f, %.4f], 0.01, 255, 0, 0, 255, \"%s\")\n",
        totalMatrix.mafEntries[3],
        totalMatrix.mafEntries[7],
        totalMatrix.mafEntries[11],
        jointName.c_str()
    );*/

    for(uint32_t iChild = 0; iChild < joint.miNumChildren; iChild++)
    {
        uint32_t iChildArrayIndex = aiJointToArrayIndices[joint.maiChildren[iChild]];
        Joint childJoint = aJoints[iChildArrayIndex];
        testTraverseAnimationNoLocalBind(
            childJoint,
            totalMatrix,
            aJoints,
            aaKeyFrames,
            aiJointToArrayIndices,
            aJointMapping,
            fTime
        );
    }
}

/*
**
*/
void testTraverseMatchingAnimation(
    Joint const& joint,
    float4x4 const& parentMatrix,
    std::vector<Joint> const& aJoints,
    std::vector<std::vector<AnimFrame>> const& aaSrcLocalAnimationKeyFrames,
    std::vector<float4x4> const& aLocalBindMatrices,
    std::vector<uint32_t> const& aiJointToArrayIndices,
    std::map<uint32_t, std::string>& aJointMapping,
    std::map<std::string, std::string>& aDstToSrcJointMapping,
    std::map<uint32_t, std::string>& aSrcJointMapping,
    float fTime)
{
    uint32_t iJointIndex = joint.miIndex;
    uint32_t iArrayIndex = aiJointToArrayIndices[iJointIndex];
    std::string jointName = aJointMapping[iJointIndex];

    std::string const& srcJointName = aDstToSrcJointMapping[jointName];
    uint32_t iSrcJointIndex = 0;
    bool bFoundSrcJoint = false;
    for(auto const& keyValue : aSrcJointMapping)
    {
        if(keyValue.second == srcJointName)
        {
            bFoundSrcJoint = true;
            break;
        }
        iSrcJointIndex += 1;
    }
    
    uint32_t iSrcJointKeyFrameIndex = UINT32_MAX;
    for(uint32_t i = 0; i < aaSrcLocalAnimationKeyFrames.size(); i++)
    {
        if(aaSrcLocalAnimationKeyFrames[i][0].miNodeIndex == iSrcJointIndex)
        {
            iSrcJointKeyFrameIndex = i;
            break;
        }
    }
    
    quaternion q;
    AnimFrame keyFrame;

    if(bFoundSrcJoint)
    {
        std::vector<AnimFrame> const& aKeyFrames = aaSrcLocalAnimationKeyFrames[iSrcJointKeyFrameIndex];
        for(uint32_t iFrameIndex = 0; iFrameIndex < aaSrcLocalAnimationKeyFrames[iSrcJointKeyFrameIndex].size(); iFrameIndex++)
        {
            if(aKeyFrames[iFrameIndex].mfTime >= fTime)
            {
                uint32_t iPrevFrameIndex = (iFrameIndex > 0) ? iFrameIndex - 1 : 0;
                AnimFrame const& currFrame = aKeyFrames[iFrameIndex];
                AnimFrame const& prevFrame = aKeyFrames[iPrevFrameIndex];

                float fTimePct = (currFrame.mfTime - prevFrame.mfTime > 0.0f) ? (fTime - prevFrame.mfTime) / (currFrame.mfTime - prevFrame.mfTime) : 0.0f;
                keyFrame.mScaling = prevFrame.mScaling + (currFrame.mScaling - prevFrame.mScaling) * fTimePct;
                keyFrame.mTranslation = prevFrame.mTranslation + (currFrame.mTranslation - prevFrame.mTranslation) * fTimePct;

                quaternion prevQ = quaternion(prevFrame.mRotation);
                quaternion currQ = quaternion(currFrame.mRotation);
                q = quaternion::slerp(prevQ, currQ, fTimePct);

if(srcJointName == "left_upper_arm")
{
    int iDebug = 1;
}

                break;
            }
        }
    }
    
    float4x4 translationMatrix = translate(keyFrame.mTranslation);
    float4x4 scaleMatrix = scale(keyFrame.mScaling);
    float4x4 rotationMatrix = q.matrix();
    float4x4 localAnimationMatrix = translationMatrix * rotationMatrix * scaleMatrix;

    float4x4 const& localBindMatrix = aLocalBindMatrices[iArrayIndex];
    float4x4 localMatrix = localBindMatrix * localAnimationMatrix;
    float4x4 totalMatrix = parentMatrix * localMatrix;
    /*DEBUG_PRINTF("draw_sphere([%.4f, %.4f, %.4f], 0.01, 255, 0, 0, 255, \"%s\")\n",
        totalMatrix.mafEntries[3],
        totalMatrix.mafEntries[7],
        totalMatrix.mafEntries[11],
        jointName.c_str()
    );*/

    for(uint32_t iChild = 0; iChild < joint.miNumChildren; iChild++)
    {
        uint32_t iChildArrayIndex = aiJointToArrayIndices[joint.maiChildren[iChild]];
        Joint childJoint = aJoints[iChildArrayIndex];
        testTraverseMatchingAnimation(
            childJoint,
            totalMatrix,
            aJoints,
            aaSrcLocalAnimationKeyFrames,
            aLocalBindMatrices,
            aiJointToArrayIndices,
            aJointMapping,
            aDstToSrcJointMapping,
            aSrcJointMapping,
            fTime
        );
    }
}

/*
**
*/
void testTraverseWithLocalJointRotation(
    std::vector<float3>& aGlobalJointPositions,
    std::vector<float4x4>& aAnimMatchingLocalBindMatrices,
    std::vector<float4x4>& aAnimMatchingGlobalBindMatrices,
    Joint const& joint,
    float4x4 const& parentMatrix,
    std::vector<Joint> const& aJoints,
    std::vector<uint32_t> const& aiJointToArrayIndices,
    std::map<uint32_t, std::string>& aJointMapping,
    std::string const& replaceJoint,
    float4x4 const& replaceLocalAnimationRotationMatrix)
{
    uint32_t iJointIndex = joint.miIndex;
    uint32_t iArrayIndex = aiJointToArrayIndices[iJointIndex];
    std::string jointName = aJointMapping[iJointIndex];

    float4x4 const& localBindMatrix = aAnimMatchingLocalBindMatrices[iArrayIndex];
    float4x4 localMatrix = localBindMatrix;

    if(jointName == replaceJoint)
    {
        localMatrix = replaceLocalAnimationRotationMatrix;
    }

    aAnimMatchingLocalBindMatrices[iArrayIndex] = localMatrix;

    float4x4 totalMatrix = parentMatrix * localMatrix;
    aAnimMatchingGlobalBindMatrices[iArrayIndex] = totalMatrix;
    
    //DEBUG_PRINTF("draw_sphere([%.4f, %.4f, %.4f], 0.01, 255, 0, 0, 255, \"%s\")\n",
    //    totalMatrix.mafEntries[3],
    //    totalMatrix.mafEntries[7],
    //    totalMatrix.mafEntries[11],
    //    jointName.c_str()
    //);

    aGlobalJointPositions[iArrayIndex] = float3(totalMatrix.mafEntries[3], totalMatrix.mafEntries[7], totalMatrix.mafEntries[11]);

    for(uint32_t iChild = 0; iChild < joint.miNumChildren; iChild++)
    {
        uint32_t iChildArrayIndex = aiJointToArrayIndices[joint.maiChildren[iChild]];
        Joint childJoint = aJoints[iChildArrayIndex];
        testTraverseWithLocalJointRotation(
            aGlobalJointPositions,
            aAnimMatchingLocalBindMatrices,
            aAnimMatchingGlobalBindMatrices,
            childJoint,
            totalMatrix,
            aJoints,
            aiJointToArrayIndices,
            aJointMapping,
            replaceJoint,
            replaceLocalAnimationRotationMatrix
        );
    }
}

/*
**
*/
void testTraverseMatchingKeyFrames(
    Joint const& joint,
    float4x4 const& parentMatrix,
    std::vector<Joint> const& aJoints,
    std::vector<float4x4> const& aLocalBindMatrices,
    std::vector<AnimFrame> const& aAnimFrames,
    std::vector<uint32_t> const& aiJointToArrayIndices,
    std::map<uint32_t, std::string>& aJointMapping)
{
    uint32_t iArrayIndex = aiJointToArrayIndices[joint.miIndex];
    float4x4 const& localMatrix = aLocalBindMatrices[iArrayIndex];
    float4x4 localAnimMatrix = localMatrix;
    float4x4 rotationMatrix;
    for(auto const& animFrame : aAnimFrames)
    {
        if(animFrame.miNodeIndex == joint.miIndex)
        {
            rotationMatrix = makeFromAngleAxis(float3(animFrame.mRotation), animFrame.mRotation.w);
            localAnimMatrix = aLocalBindMatrices[iArrayIndex] * rotationMatrix;
                
            break;
        }
    }

    float4x4 totalMatrix = parentMatrix * localAnimMatrix;
    std::string jointName = aJointMapping[joint.miIndex];
    /*DEBUG_PRINTF("draw_sphere([%.4f, %.4f, %.4f], 0.01, 255, 0, 0, 255, \"%s\")\n",
        totalMatrix.mafEntries[3],
        totalMatrix.mafEntries[7],
        totalMatrix.mafEntries[11],
        jointName.c_str()
    );*/

    for(uint32_t iChild = 0; iChild < joint.miNumChildren; iChild++)
    {
        uint32_t iChildArrayIndex = aiJointToArrayIndices[joint.maiChildren[iChild]];
        Joint const& childJoint = aJoints[iChildArrayIndex];
        testTraverseMatchingKeyFrames(
            childJoint,
            totalMatrix,
            aJoints,
            aLocalBindMatrices,
            aAnimFrames,
            aiJointToArrayIndices,
            aJointMapping
        );
    }
}

/*
**
*/
uint32_t getArrayIndex(
    std::string const& jointName,
    std::map<uint32_t, std::string> const& aJointIndexMapping,
    std::vector<uint32_t> const& aiJointMapIndices,
    std::vector<Joint> const& aJoints)
{
    // src joint and array indices
    uint32_t iSrcJointIndex = UINT32_MAX;
    for(auto const& keyValue : aJointIndexMapping)
    {
        if(keyValue.second == jointName)
        {
            iSrcJointIndex = keyValue.first;
            break;
        }
    }
    uint32_t iSrcArrayIndex = aiJointMapIndices[iSrcJointIndex];
    return iSrcArrayIndex;
}

/*
**
*/
void testUpdateAnimation(
    std::vector<float4x4>& aTotalMatrices,
    std::vector<float4x4> const& aLocalBindMatrices,
    std::vector<float4x4> const& aLocalAnimMatrices,
    std::vector<DebugJoint> const& aJoints,
    float4x4 const& parentMatrix,
    DebugJoint const& joint,
    std::map<uint32_t, std::string>& aJointMapping,
    std::vector<uint32_t> const& aiJointMapIndices,
    uint32_t iLevel)
{
    uint32_t iJointArrayIndex = aiJointMapIndices[joint.miIndex];
    uint32_t iParentArrayIndex = (joint.miParent != UINT32_MAX) ? aiJointMapIndices[joint.miParent] : UINT32_MAX;

    float4x4 const& localMatrix = aLocalBindMatrices[iJointArrayIndex];
    float4x4 const& animMatrix = aLocalAnimMatrices[iJointArrayIndex];
    float4x4 totalMatrix = parentMatrix * localMatrix * animMatrix;
    aTotalMatrices.push_back(totalMatrix);

    float3 pos = float3(totalMatrix.mafEntries[3], totalMatrix.mafEntries[7], totalMatrix.mafEntries[11]);
    DEBUG_PRINTF("draw_sphere([%.4f, %.4f, %.4f], 0.01, 255, 0, 0, 255, \"%s\") # joint index: %d array index: %d\n",
        pos.x,
        pos.y,
        pos.z,
        joint.mName.c_str(),
        joint.miIndex,
        iJointArrayIndex
    );

    for(uint32_t iChild = 0; iChild < joint.miNumChildren; iChild++)
    {
        uint32_t iChildArrayIndex = aiJointMapIndices[joint.maiChildren[iChild]];
        DebugJoint const& childJoint = aJoints[iChildArrayIndex];
        testUpdateAnimation(
            aTotalMatrices,
            aLocalBindMatrices,
            aLocalAnimMatrices,
            aJoints,
            totalMatrix,
            childJoint,
            aJointMapping,
            aiJointMapIndices,
            iLevel + 1
        );
    }
}

/*
**
*/
void updateGlobalBindMatrices(
    std::vector<float4x4>& aGlobalBindMatrices,
    float4x4 const& parentGlobalBindMatrix,
    std::vector<float4x4> const& aLocalBindMatrices,
    std::vector<DebugJoint> const& aJoints,
    std::map<uint32_t, std::string>& aJointMapping,
    std::vector<uint32_t> const& aiJointToArrayIndices,
    DebugJoint const& joint,
    uint32_t iLevel)
{
    uint32_t iJointArrayIndex = aiJointToArrayIndices[joint.miIndex];
    float4x4 newGloblBindMatrix = parentGlobalBindMatrix * aLocalBindMatrices[iJointArrayIndex];
    aGlobalBindMatrices[iJointArrayIndex] = newGloblBindMatrix;

    float3 pos = float3(newGloblBindMatrix.mafEntries[3], newGloblBindMatrix.mafEntries[7], newGloblBindMatrix.mafEntries[11]);
    DEBUG_PRINTF("draw_sphere([%.4f, %.4f, %.4f], 0.01, 255, 0, 0, 255, \"%s\") # joint index: %d array index: %d\n",
        pos.x,
        pos.y,
        pos.z,
        joint.mName.c_str(),
        joint.miIndex,
        iJointArrayIndex
    );

    for(uint32_t i = 0; i < 32; i++)
    {
        if(joint.maiChildren[i] == UINT32_MAX)
        {
            break;
        }

        uint32_t iChildArrayIndex = aiJointToArrayIndices[joint.maiChildren[i]];
        DebugJoint const& childJoint = aJoints[iChildArrayIndex];
        
        updateGlobalBindMatrices(
            aGlobalBindMatrices,
            newGloblBindMatrix,
            aLocalBindMatrices,
            aJoints,
            aJointMapping,
            aiJointToArrayIndices,
            childJoint,
            iLevel + 1
        );
    }
}

/*
**
*/
void getMatchingAnimationFrames2(
    std::vector<AnimFrame>& aDstMatchingAnimFrames,
    std::vector<float4>& aAnimMatchRotations,
    std::vector<float4x4> const& aDstGlobalBindMatrices,
    std::map<uint32_t, std::string>& aDstJointMapping,
    std::vector<uint32_t> const& aiDstJointMapIndices,
    std::vector<Joint> const& aDstJoints,

    std::vector<float4x4> const& aSrcGlobalBindMatrices,
    std::vector<float4x4> const& aSrcGlobalAnimatedJointMatrices,
    std::map<uint32_t, std::string>& aSrcJointMapping,
    std::vector<uint32_t> const& aiSrcJointMapIndices,
    std::vector<Joint> const& aSrcJoints,

    std::string const& dir,
    std::string const& jointMappingFileName,

    std::vector<float4x4> const& aSrcLocalBindMatrices,
    std::vector<float4x4> const& aDstLocalBindMatrices,

    std::vector<DebugJoint> const& aSrcDebugJoints,
    std::vector<DebugJoint> const& aDstDebugJoints,

    std::vector<float4x4> const& aSrcLocalAnimMatrices,

    float fTime
)
{
    for(uint32_t i = 0; i < aSrcGlobalBindMatrices.size(); i++)
    {
        DebugJoint const& joint = aSrcDebugJoints[i];

        uint32_t iJointArrayIndex = 0;
        float3 pos = float3(aSrcGlobalBindMatrices[i].mafEntries[3], aSrcGlobalBindMatrices[i].mafEntries[7], aSrcGlobalBindMatrices[i].mafEntries[11]);
        DEBUG_PRINTF("draw_sphere([%.4f, %.4f, %.4f], 0.01, 255, 0, 0, 255, \"%s\") # joint index: %d array index: %d\n",
            pos.x,
            pos.y,
            pos.z,
            joint.mName.c_str(),
            joint.miIndex,
            iJointArrayIndex
        );
    }

    // get the mapping of dst to src
    std::vector<std::pair<std::string, std::string>> aJointMatchingMap;
    {
        std::string fullPath = dir + "/" + jointMappingFileName;
        FILE* fp = fopen(fullPath.c_str(), "rb");
        fseek(fp, 0, SEEK_END);
        uint64_t iFileSize = (uint64_t)ftell(fp);
        fseek(fp, 0, SEEK_SET);
        std::vector<char> acFileContent(iFileSize + 1);
        acFileContent[iFileSize] = 0;
        fread(acFileContent.data(), sizeof(char), iFileSize, fp);
        fclose(fp);

        rapidjson::Document doc;
        doc.Parse(acFileContent.data());
        auto jointMapping = doc["JointMapping"].GetArray();
        for(auto& map : jointMapping)
        {
            auto jointName = map["name"].GetString();
            auto mapJointName = map["joint"].GetString();
            aJointMatchingMap.push_back(std::pair(std::string(jointName), std::string(mapJointName)));
        }
    }

    // convert the global dst bind matrix to default world space at the origin 
    float4x4 dstRootGlobalBindMatrix = aDstGlobalBindMatrices[0];
    float3 dstScaleEntries = float3(
        length(float3(dstRootGlobalBindMatrix.mafEntries[0], dstRootGlobalBindMatrix.mafEntries[4], dstRootGlobalBindMatrix.mafEntries[8])),
        length(float3(dstRootGlobalBindMatrix.mafEntries[1], dstRootGlobalBindMatrix.mafEntries[5], dstRootGlobalBindMatrix.mafEntries[9])),
        length(float3(dstRootGlobalBindMatrix.mafEntries[2], dstRootGlobalBindMatrix.mafEntries[6], dstRootGlobalBindMatrix.mafEntries[10]))
    );
    dstRootGlobalBindMatrix.mafEntries[3] = dstRootGlobalBindMatrix.mafEntries[7] = dstRootGlobalBindMatrix.mafEntries[11] = 0.0f;
    float4x4 scaleMatrix = scale(1.0f / dstScaleEntries.x, 1.0f / dstScaleEntries.y, 1.0f / dstScaleEntries.z);
    float4x4 dstRootGlobalBindMatrixScaled = dstRootGlobalBindMatrix * scaleMatrix;

    // convert dst rig to default space
    float4x4 invDstGlobalBindMatrix = invert(dstRootGlobalBindMatrix);
    std::vector<float4x4> aConvertedDstGlobalBindMatrices = aDstGlobalBindMatrices;
    for(uint32_t i = 0; i < aDstGlobalBindMatrices.size(); i++)
    {
        aConvertedDstGlobalBindMatrices[i] = aDstGlobalBindMatrices[i] * invDstGlobalBindMatrix;
    }
    
    // convert src rig to default space
    float4x4 invSrcGlobalBindMatrix = invert(aSrcGlobalBindMatrices[0]);
    std::vector<float4x4> aConvertedSrcGlobalAnimMatrices = aSrcGlobalAnimatedJointMatrices;
    for(uint32_t i = 0; i < aSrcGlobalAnimatedJointMatrices.size(); i++)
    {
        aConvertedSrcGlobalAnimMatrices[i] = aSrcGlobalAnimatedJointMatrices[i] * invSrcGlobalBindMatrix;
    }

    float3 dstTangent = mul(float4(1.0f, 0.0f, 0.0f, 1.0f), dstRootGlobalBindMatrixScaled);
    float3 srcTangent = mul(float4(1.0f, 0.0f, 0.0f, 1.0f), aSrcGlobalBindMatrices[0]);

    std::vector<float4x4> aCurrSrcGlobalBindMatrices = aSrcGlobalBindMatrices;
    std::vector<float4x4> aCurrDstGlobalBindMatrices = aDstGlobalBindMatrices;
    std::vector<float4x4> aCurrSrcLocalBindMatrices = aSrcLocalBindMatrices;
    std::vector<float4x4> aLocalBindRotationMatrices(aDstLocalBindMatrices.size());

    DebugJoint const& dstRootJoint = aDstDebugJoints[0];

    // scale back to 1.0f
    for(uint32_t i = 0; i < aCurrDstGlobalBindMatrices.size(); i++)
    {
        float4x4 dstGlobalBindMatrix = aCurrDstGlobalBindMatrices[i];
        float3 scaling = extractScale(dstGlobalBindMatrix);
        aCurrDstGlobalBindMatrices[i] = scale(1.0f / scaling.x, 1.0f / scaling.y, 1.0f / scaling.z) * dstGlobalBindMatrix;
    }

    // dst local matrices
    std::vector<float4x4> aDstGlobalBindMatrixScaled = aCurrDstGlobalBindMatrices;
    std::vector<float4x4> aCurrDstLocalBindMatrices;
    computeLocalBindMatrices(
        aCurrDstLocalBindMatrices,
        aCurrDstGlobalBindMatrices,
        aDstJoints,
        aiDstJointMapIndices,
        aDstJointMapping
    );
    std::vector<float4x4> aDstLocalBindMatrixScaled = aCurrDstLocalBindMatrices;

    // get the current source animation matrices
    DebugJoint const& srcRootJoint = aSrcDebugJoints[0];
    std::vector<float4x4> aSrcGlobalAnimMatrices;
    testUpdateAnimation(
        aSrcGlobalAnimMatrices,
        aSrcLocalBindMatrices,
        aSrcLocalAnimMatrices,
        aSrcDebugJoints,
        float4x4(),
        srcRootJoint,
        aSrcJointMapping,
        aiSrcJointMapIndices,
        0
    );

    // matching
    std::vector<float4x4> aDstLocalAnimMatrices(aDstDebugJoints.size());
    std::vector<float4x4> aDstGlobalAnimMatrices(aDstDebugJoints.size());
    std::vector<float4x4> aDstLocalAnimMatrixNew(aDstLocalAnimMatrices.size());
    for(uint32_t i = 0; i < aJointMatchingMap.size(); i++)
    {
        // src joint and array indices
        std::string const& srcJointName = aJointMatchingMap[i].first;
        uint32_t iSrcJointArrayIndex = getArrayIndex(
            srcJointName,
            aSrcJointMapping,
            aiSrcJointMapIndices,
            aSrcJoints
        );
        DebugJoint const& srcJoint = aSrcDebugJoints[iSrcJointArrayIndex];

        // dst joint and array indices
        std::string const& dstJointName = aJointMatchingMap[i].second;
        uint32_t iDstJointArrayIndex = getArrayIndex(
            dstJointName,
            aDstJointMapping,
            aiDstJointMapIndices,
            aDstJoints
        );
        DebugJoint const& dstJoint = aDstDebugJoints[iDstJointArrayIndex];

        float4x4 dstGlobalAnimMatrix = aDstGlobalAnimMatrices[iDstJointArrayIndex];

        // get the relative matrix from src bind to dst bind
        // apply the relative matrix to src anim matrix, this is the src global anim matrix in dst global space
        // Mrelative = invert(Msrc_bind) * Mdst_bind
        // Msrc_anim_in_dst_global = Msrc_anim * Mrelative
        // Mrelative = invert(Msrc_anim_in_dst_global) * Mdst_bind_local
        // Mdst_anim_local = Msrc_anim_in_dst_global * Mrelative

        // no translations, just rotations
        float4x4 srcGlobalBindMatrixNoTranslation = aSrcGlobalBindMatrices[iSrcJointArrayIndex];
        float4x4 dstLocalBindMatrixNoTranslation = aDstLocalBindMatrixScaled[iDstJointArrayIndex];
        float4x4 srcGlobalAnimMatrixNoTranslation = aSrcGlobalAnimMatrices[iSrcJointArrayIndex];
        float4x4 dstGlobalBindMatrixNoTranslation = aDstGlobalBindMatrixScaled[iDstJointArrayIndex];
        srcGlobalBindMatrixNoTranslation.mafEntries[3] = srcGlobalBindMatrixNoTranslation.mafEntries[7] = srcGlobalBindMatrixNoTranslation.mafEntries[11] = 0.0f;
        dstLocalBindMatrixNoTranslation.mafEntries[3] = dstLocalBindMatrixNoTranslation.mafEntries[7] = dstLocalBindMatrixNoTranslation.mafEntries[11] = 0.0f;
        srcGlobalAnimMatrixNoTranslation.mafEntries[3] = srcGlobalAnimMatrixNoTranslation.mafEntries[7] = srcGlobalAnimMatrixNoTranslation.mafEntries[11] = 0.0f;
        dstGlobalBindMatrixNoTranslation.mafEntries[3] = dstGlobalBindMatrixNoTranslation.mafEntries[7] = dstGlobalBindMatrixNoTranslation.mafEntries[11] = 0.0f;

        float4x4 srcBindToDstBindMatrix = invert(srcGlobalBindMatrixNoTranslation) * dstGlobalBindMatrixNoTranslation;

        float4x4 verifyMatrix = srcGlobalBindMatrixNoTranslation * srcBindToDstBindMatrix;
        assert(verifyMatrix == dstGlobalBindMatrixNoTranslation);

        float4x4 dstGlobalAnimFromSrcGlobalAnimMatrix = srcGlobalAnimMatrixNoTranslation * srcBindToDstBindMatrix;
        float4x4 dstGlobalBindToDstLocalBindMatrix = invert(dstGlobalBindMatrixNoTranslation) * dstLocalBindMatrixNoTranslation;

        verifyMatrix = dstGlobalBindMatrixNoTranslation * dstGlobalBindToDstLocalBindMatrix;
        assert(verifyMatrix == dstLocalBindMatrixNoTranslation);

        // get animation matrix 
        // Mtotal = Mparent_total * Mlocal * Manimation
        // invert(Mparent_total) * Mtotal = Mlocal * Manimation
        // invert(Mlocal) * invert(Mparent_total) * Mtotal = Manimation
        float4x4 dstParentTotalMatrixNoTranslation;
        if(dstJoint.miParent != UINT32_MAX)
        {
            uint32_t iParentArrayIndex = aiDstJointMapIndices[dstJoint.miParent];
            DebugJoint parentJoint = aDstDebugJoints[iParentArrayIndex];
            dstParentTotalMatrixNoTranslation = aDstGlobalAnimMatrices[iParentArrayIndex];
            dstParentTotalMatrixNoTranslation.mafEntries[3] = dstParentTotalMatrixNoTranslation.mafEntries[7] = dstParentTotalMatrixNoTranslation.mafEntries[11] = 0.0f;
        }
        aDstLocalAnimMatrixNew[iDstJointArrayIndex] = invert(dstLocalBindMatrixNoTranslation) * invert(dstParentTotalMatrixNoTranslation) * dstGlobalAnimFromSrcGlobalAnimMatrix;

        float4 localAxis = extractAxisAngle(aDstLocalAnimMatrixNew[iDstJointArrayIndex]);

        // new local animation axis angle rotation key frame
        AnimFrame matchingAnimFrame;
        matchingAnimFrame.mfTime = fTime;
        matchingAnimFrame.miNodeIndex = dstJoint.miIndex;
        matchingAnimFrame.mRotation = float4(localAxis.x, localAxis.y, localAxis.z, localAxis.w);

        if(srcJoint.miParent == UINT32_MAX)
        {
            // convert src anim position to dst anim position, difference between the animation position and bind position is the animated translation
            float4 srcAnimToDstTranslation = mul(
                float4(aSrcGlobalAnimMatrices[iSrcJointArrayIndex].mafEntries[3], aSrcGlobalAnimMatrices[iSrcJointArrayIndex].mafEntries[7], aSrcGlobalAnimMatrices[iSrcJointArrayIndex].mafEntries[11], 1.0f), 
                srcBindToDstBindMatrix
            );
            float3 translation = float3(srcAnimToDstTranslation) - float3(aDstGlobalBindMatrices[iSrcJointArrayIndex].mafEntries[3], aDstGlobalBindMatrices[iSrcJointArrayIndex].mafEntries[7], aDstGlobalBindMatrices[iSrcJointArrayIndex].mafEntries[11]);
            matchingAnimFrame.mTranslation = float4(translation, 1.0f);
        }

        aDstMatchingAnimFrames.push_back(matchingAnimFrame);

        // update dst global total matrices
        aDstGlobalAnimMatrices.clear();
        testUpdateAnimation(
            aDstGlobalAnimMatrices,
            aDstLocalBindMatrixScaled,
            aDstLocalAnimMatrixNew,
            aDstDebugJoints,
            float4x4(),
            dstRootJoint,
            aDstJointMapping,
            aiDstJointMapIndices,
            0
        );
    }
}

/*
**
*/
void getMatchingAnimationFrames(
    std::vector<AnimFrame>& aDstMatchingAnimFrames,
    std::vector<float3>& aDstGlobalBindJointPositions,

    std::vector<float4x4> const& aDstLocalBindMatrices,
    std::vector<float4x4> const& aDstGlobalBindMatrices,

    std::map<uint32_t, std::string>& aDstJointMapping,
    std::vector<uint32_t> const& aiDstJointMapIndices,
    std::vector<Joint> const& aDstJoints,

    std::vector<float3> const& aSrcGlobalAnimatedJointPositions,
    std::map<uint32_t, std::string> const& aSrcJointMapping,
    std::vector<uint32_t> const& aiSrcJointMapIndices,
    std::vector<Joint> const& aSrcJoints,
    
    float fTime,
    
    std::string const& dir,
    std::string const& jointMappingFileName,
    
    std::vector<float4x4> const& aSrcGlobalAnimatedJointMatrices,
    std::vector<float4x4> const& aSrcGlobalBindMatrices)
{
    std::vector<float4x4> aAnimMatchingLocalBindMatrices = aDstLocalBindMatrices;
    std::vector<float4x4> aAnimMatchingGlobalBindMatrices = aDstGlobalBindMatrices;

    // get the mapping of dst to src
    std::vector<std::pair<std::string, std::string>> aJointMapping;
    {
        std::string fullPath = dir + "/" + jointMappingFileName;
        FILE* fp = fopen(fullPath.c_str(), "rb");
        fseek(fp, 0, SEEK_END);
        uint64_t iFileSize = (uint64_t)ftell(fp);
        fseek(fp, 0, SEEK_SET);
        std::vector<char> acFileContent(iFileSize + 1);
        acFileContent[iFileSize] = 0;
        fread(acFileContent.data(), sizeof(char), iFileSize, fp);
        fclose(fp);

        rapidjson::Document doc;
        doc.Parse(acFileContent.data());
        auto jointMapping = doc["JointMapping"].GetArray();
        for(auto& map: jointMapping)
        {
            auto jointName = map["name"].GetString();
            auto mapJointName = map["joint"].GetString();
            aJointMapping.push_back(std::pair(std::string(jointName), std::string(mapJointName)));
        }
    }

    for(uint32_t i = 0; i < aJointMapping.size(); i++)
    {
        // src joint and array indices
        uint32_t iSrcJointIndex = UINT32_MAX;
        std::string const& srcJointName = aJointMapping[i].first;
        for(auto const& keyValue : aSrcJointMapping)
        {
            if(keyValue.second == srcJointName)
            {
                iSrcJointIndex = keyValue.first;
                break;
            }
        }
        uint32_t iSrcArrayIndex = aiSrcJointMapIndices[iSrcJointIndex];
        Joint const& srcJoint = aSrcJoints[iSrcArrayIndex];
        uint32_t iSrcChildJointIndex = srcJoint.maiChildren[0];
        uint32_t iSrcChildArrayIndex = UINT32_MAX; 
        if(iSrcChildJointIndex < UINT32_MAX)
        {
            iSrcChildArrayIndex = aiSrcJointMapIndices[iSrcChildJointIndex];
        }

// test test test
uint32_t iTest = 0;
float3 dominantLocalAxis = float3(0.0f, 1.0f, 0.0f);
float3 dominantLocalTangent = float3(1.0f, 0.0f, 0.0f);
float fDominantLocalAngle = 0.0f;
float3 testGlobalBindAxis = float3(0.0f, 0.0f, 0.0f);
float fTestGlobalBindAngle = 0.0f;
//if(srcJointName == "pelvis" || srcJointName == "neck" || srcJointName == "spine0" || srcJointName == "spine1" || srcJointName == "left_upper_arm" || srcJointName == "right_upper_arm")
//if(srcJointName == "left_foot" || srcJointName == "left_leg" || srcJointName == "left_thigh")
{
    iTest = 1;

    float3 pos = float3(
        aSrcGlobalBindMatrices[iSrcArrayIndex].mafEntries[3],
        aSrcGlobalBindMatrices[iSrcArrayIndex].mafEntries[7],
        aSrcGlobalBindMatrices[iSrcArrayIndex].mafEntries[11]
    );
    float3 childPos = pos + float3(0.0f, 1.0f, 0.0f); 
    if(iSrcChildArrayIndex < UINT32_MAX)
    {
        childPos = float3(
            aSrcGlobalBindMatrices[iSrcChildArrayIndex].mafEntries[3],
            aSrcGlobalBindMatrices[iSrcChildArrayIndex].mafEntries[7],
            aSrcGlobalBindMatrices[iSrcChildArrayIndex].mafEntries[11]
        );
    }
    
    // get local space joint positions and calculate the joint to child direction in local space
    float4x4 inverseSrcGlobalBindMatrix = invert(aSrcGlobalBindMatrices[iSrcArrayIndex]);
    float4 localJointPos = mul(
        float4(pos.x, pos.y, pos.z, 1.0f),
        inverseSrcGlobalBindMatrix
    );
    assert(float3(localJointPos) == float3(0.0f, 0.0f, 0.0f));
    float4 localChildJointPos = mul(
        float4(childPos.x, childPos.y, childPos.z, 1.0f),
        inverseSrcGlobalBindMatrix
    );
    float3 localSrcToChildDiff = 
        float3(localChildJointPos.x, localChildJointPos.y, localChildJointPos.z) - 
        float3(localJointPos.x, localJointPos.y, localJointPos.z);
    float3 localSrcToChildDir = normalize(localSrcToChildDiff);
    dominantLocalAxis = localSrcToChildDir;

    // local tangent use to apply animate rotation 
    float3 up = (fabsf(dominantLocalAxis.y) > 0.99f) ? float3(0.0f, 0.0f, 1.0f) : float3(0.0f, 1.0f, 0.0f);
    dominantLocalTangent = normalize(cross(dominantLocalAxis, up));
    
    // rotation only animated matrix
    float4x4 rotationOnlyAnimMatrix = aSrcGlobalAnimatedJointMatrices[iSrcArrayIndex];
    rotationOnlyAnimMatrix.mafEntries[3] = rotationOnlyAnimMatrix.mafEntries[7] = rotationOnlyAnimMatrix.mafEntries[11] = 0.0f;
    
    // anim matrix * inverse global bind matrix = local anim matrix
    float4x4 inverseSrcGlobalRotationMatrix = inverseSrcGlobalBindMatrix;
    inverseSrcGlobalRotationMatrix.mafEntries[3] = inverseSrcGlobalRotationMatrix.mafEntries[7] = inverseSrcGlobalRotationMatrix.mafEntries[11] = 0.0f;
    float4x4 localRotationOnlyAnimMatrix = inverseSrcGlobalRotationMatrix * rotationOnlyAnimMatrix;

    // local anim matrix * local tangent = local space animated bone direction
    float4 srcAnimLocalTangent = mul(
        float4(dominantLocalTangent.x, dominantLocalTangent.y, dominantLocalTangent.z, 1.0f),
        localRotationOnlyAnimMatrix
    );

    float3 srcAnimatedLocalTangentXYZ = normalize(float3(srcAnimLocalTangent.x, 0.0f, srcAnimLocalTangent.z));
    float fDP = dot(srcAnimatedLocalTangentXYZ, dominantLocalTangent);
    fDominantLocalAngle = acosf(fDP);

float3 eulerAngles = extractEulerAngles2(localRotationOnlyAnimMatrix);
fDominantLocalAngle = eulerAngles.y;
//dominantLocalAxis = float3(0.0f, 1.0f, 0.0f);


    /*DEBUG_PRINTF("time: %.4f joint \"%s\" dominant axis (%.4f, %.4f, %.4f) dominant tangent (%.4f, %.4f, %.4f) dominant angle: %.4f\n",
        fTime,
        srcJointName.c_str(),
        dominantLocalAxis.x, dominantLocalAxis.y, dominantLocalAxis.z,
        dominantLocalTangent.x, dominantLocalTangent.y, dominantLocalTangent.z,
        fDominantLocalAngle
    );*/

    DEBUG_PRINTF("%.4f \"%s\" euler angle (%.4f, %.4f, %.4f) axis (%.4f, %.4f, %.4f)\n",
        fTime,
        srcJointName.c_str(),
        eulerAngles.x, eulerAngles.y, eulerAngles.z,
        dominantLocalAxis.x, dominantLocalAxis.y, dominantLocalAxis.z);
}

        // dst joint and array indices
        uint32_t iDstJointIndex = UINT32_MAX;
        std::string const& dstJointName = aJointMapping[i].second;
        for(auto const& keyValue : aDstJointMapping)
        {
            if(keyValue.second == dstJointName)
            {
                iDstJointIndex = keyValue.first;
                break;
            }
        }
        uint32_t iDstArrayIndex = aiDstJointMapIndices[iDstJointIndex];
        Joint const& dstJoint = aDstJoints[iDstArrayIndex];
        uint32_t iDstChildJointIndex = dstJoint.maiChildren[0];
        uint32_t iDstChildArrayIndex = aiDstJointMapIndices[iDstChildJointIndex];

        // global current and child joint positions at the current time 
        float3 srcGlobalAnimatedJointPosition = aSrcGlobalAnimatedJointPositions[iSrcArrayIndex];
        float3 srcGlobalAnimatedChildJointPosition = srcGlobalAnimatedJointPosition + float3(0.0f, 1.0f, 0.0f); 
        if(iSrcChildArrayIndex < UINT32_MAX)
        {
            srcGlobalAnimatedChildJointPosition = aSrcGlobalAnimatedJointPositions[iSrcChildArrayIndex];
        }
        float3 dstGlobalBindJointPosition = aDstGlobalBindJointPositions[iDstArrayIndex];
        float3 dstGlobalBindChildJointPosition = aDstGlobalBindJointPositions[iDstChildArrayIndex];

        float3 srcGlobalBindJointPosition = float3(
            aSrcGlobalBindMatrices[iSrcArrayIndex].mafEntries[3],
            aSrcGlobalBindMatrices[iSrcArrayIndex].mafEntries[7],
            aSrcGlobalBindMatrices[iSrcArrayIndex].mafEntries[11]
        );

        // transform src's child joint into dst joint's local space by applying the inverse of the dst global bind matrix with the src joint's position to bring the src child joint to dst local space
        float4x4 dstGlobalBindMatrix = aAnimMatchingGlobalBindMatrices[iDstArrayIndex];
        dstGlobalBindMatrix.mafEntries[3] = srcGlobalAnimatedJointPosition.x; dstGlobalBindMatrix.mafEntries[7] = srcGlobalAnimatedJointPosition.y; dstGlobalBindMatrix.mafEntries[11] = srcGlobalAnimatedJointPosition.z;
        float4x4 inverseDstToSrcGlobalBindMatrix = invert(dstGlobalBindMatrix);
        float4 srcChildLocalPosition = mul(float4(srcGlobalAnimatedChildJointPosition, 1.0f), inverseDstToSrcGlobalBindMatrix);
        float3 srcChildLocalPositionNormalized = normalize(float3(srcChildLocalPosition));

        // verify correct bind matrix, src joint position should be at the origin
        float4 verifyInverse = inverseDstToSrcGlobalBindMatrix * float4(srcGlobalAnimatedJointPosition, 1.0f);
        assert(length(float3(verifyInverse)) <= 1.0e-3f);

        // apply the inverse current global bind matrix to the dst child joint
        // get the normalized vector, origin is the dst joint position in local space
        dstGlobalBindMatrix.mafEntries[3] = aAnimMatchingGlobalBindMatrices[iDstArrayIndex].mafEntries[3];
        dstGlobalBindMatrix.mafEntries[7] = aAnimMatchingGlobalBindMatrices[iDstArrayIndex].mafEntries[7];
        dstGlobalBindMatrix.mafEntries[11] = aAnimMatchingGlobalBindMatrices[iDstArrayIndex].mafEntries[11];
        float4x4 inverseDstGlobalBindMatrix = invert(dstGlobalBindMatrix);
        float4 dstChildLocalPosition = mul(float4(dstGlobalBindChildJointPosition, 1.0f), inverseDstGlobalBindMatrix);
        float3 dstChildLocalPositionNormalized = normalize(float3(dstChildLocalPosition));

        // verify correct bind matrix, dst joint position should be at the origin
        verifyInverse = mul(float4(dstGlobalBindJointPosition, 1.0f), inverseDstGlobalBindMatrix);
        assert(length(float3(verifyInverse)) <= 1.0e-3f);

        // src and dst child joint are at the dst joint's local space, we can compute the axis angle between the two to get the rotation
        // needed for the dst child joint to be at src child joint
        // this is in local coordinate space
        //float fAngle = acosf(minf(maxf(dot(srcChildLocalPositionNormalized, dstChildLocalPositionNormalized), -1.0f), 1.0f));
        //float3 axis = normalize(cross(dstChildLocalPositionNormalized, srcChildLocalPositionNormalized));
        float3 localAxis = cross(dstChildLocalPositionNormalized, srcChildLocalPositionNormalized);
        float fCos = dot(srcChildLocalPositionNormalized, dstChildLocalPositionNormalized);
        float fSin = length(localAxis);
        float fLocalAngle = atan2f(fSin, fCos);
        if(fabs(fSin) > 0.0f)
        {
            localAxis = normalize(localAxis);
        }
        float4x4 r = makeFromAngleAxis(localAxis, fLocalAngle);

        // verify src and dst child joint's position, they should be nearly identical
        float4 verify = mul(float4(dstChildLocalPositionNormalized, 1.0f), r);
        assert(length(float3(verify) - srcChildLocalPositionNormalized) <= 1.0e-4f);


        

if(iTest > 0)
{
    quaternion q0 = quaternion::fromAngleAxis(localAxis, fLocalAngle);
    quaternion q1 = quaternion::fromAngleAxis(dominantLocalAxis, fDominantLocalAngle);

    quaternion totalQ = q0 * q1;
    r = totalQ.matrix();
    makeAngleAxis(localAxis, fLocalAngle, r);

#if 0
    float4 totalAxisAngle = totalQ.toAngleAxis();
    r = makeFromAngleAxis(
        float3(totalAxisAngle.x, totalAxisAngle.y, totalAxisAngle.z),
        totalAxisAngle.w
    ); 
    localAxis = float3(totalAxisAngle.x, totalAxisAngle.y, totalAxisAngle.z);
    fLocalAngle = totalAxisAngle.w;
#endif // #if 0

    float3 dstGlobalBindJointPosition = aDstGlobalBindJointPositions[iDstArrayIndex];
    float3 dstGlobalBindChildJointPosition = aDstGlobalBindJointPositions[iDstChildArrayIndex];

}

        // new local animation matrix
        float4x4 localAnimationMatrix = r;
        float4x4 newLocalMatrix = aDstLocalBindMatrices[iDstArrayIndex] * localAnimationMatrix;

        // new local animation axis angle rotation key frame
        AnimFrame matchingAnimFrame;
        matchingAnimFrame.mfTime = fTime;
        matchingAnimFrame.miNodeIndex = iDstJointIndex;
        matchingAnimFrame.mRotation = float4(localAxis, fLocalAngle);
        aDstMatchingAnimFrames.push_back(matchingAnimFrame);

        //DEBUG_PRINTF("**************************\n");

        aDstGlobalBindJointPositions.clear();
        aDstGlobalBindJointPositions.resize(aDstJoints.size());
        testTraverseWithLocalJointRotation(
            aDstGlobalBindJointPositions,
            aAnimMatchingLocalBindMatrices,
            aAnimMatchingGlobalBindMatrices,
            aDstJoints[0],
            float4x4(),
            aDstJoints,
            aiDstJointMapIndices,
            aDstJointMapping,
            aJointMapping[i].second,
            newLocalMatrix
        );
    }

    testTraverseMatchingKeyFrames(
        aDstJoints[0],
        float4x4(),
        aDstJoints,
        aDstLocalBindMatrices,
        aDstMatchingAnimFrames,
        aiDstJointMapIndices,
        aDstJointMapping
    );
}

/*
**
*/
int main(int argc, char** argv)
{
    PrintOptions printOptions;
    printOptions.mbDisplayTime = false;
    printOptions.mbDisplayTime = false;
    DEBUG_PRINTF_SET_OPTIONS(printOptions);

    std::string dir = "D:\\test\\github-projects\\test_assets";
    //std::string baseSrcName = "spider-man-bind-new-rig-pitching-2";
    std::string baseSrcName = "spider-man-bind-new-rig-ik-batting-with-bat";
    std::string srcGTLFFilePath = dir + "/" + baseSrcName + ".gltf";
    std::string srcWADFilePath = dir + "/" + baseSrcName + ".wad";

    // NOTE: animation frames are in global range

    std::vector<std::vector<float3>> aaSrcMeshPositions;
    std::vector<std::vector<float3>> aaSrcMeshNormals;
    std::vector<std::vector<float2>> aaSrcMeshTexCoords;
    std::vector<std::vector<uint32_t>> aaiSrcMeshTriangleIndices;
    std::vector<std::vector<Joint>> aaSrcJoints;
    std::vector<std::vector<uint32_t>> aaaiSrcJointInfluences;
    std::vector<std::vector<float>> aaafSrcJointWeights;
    std::vector<std::vector<float4x4>> aaSrcGlobalInverseBindMatrices;
    std::vector<std::vector<float4x4>> aaSrcGlobalBindMatrices;
    std::vector<std::vector<uint32_t>> aaiSrcSkinJointIndices;
    std::vector<std::vector<Joint>> aaSrcAnimHierarchy;
    std::vector<std::vector<float4x4>> aaSrcJointLocalBindMatrices;
    std::vector<std::vector<AnimFrame>> aaSrcAnimationFrames;
    std::vector<std::vector<uint32_t>> aaiSrcJointMapIndices;
    std::map<std::string, std::map<uint32_t, std::string>> aaSrcJointMapping;
    std::vector<std::vector<DebugJoint>> aaSrcDebugJoints;

    std::vector<OutputMaterialInfo> aSrcMaterials;
    std::vector<std::string>        aSrcAlbedoImageURI;
    std::vector<std::string>        aSrcNormalImageURI;
    std::vector<std::string>        aSrcMetallicRoughnessImageURI;
    std::vector<std::string>        aSrcEmissiveImageURI;
    std::vector<uint32_t>           aiSrcMeshMaterialID;

    loadGLTF(
        srcGTLFFilePath, //"/Users/dingwings/Downloads/assets/spider-man-bind-new-rig-ik-batting.gltf",
        srcWADFilePath, //"/Users/dingwings/Downloads/assets/spider-man-batting.wad",
        aaSrcMeshPositions,
        aaSrcMeshNormals,
        aaSrcMeshTexCoords,
        aaiSrcMeshTriangleIndices,
        aaSrcJoints,
        aaaiSrcJointInfluences,
        aaafSrcJointWeights,
        aaSrcGlobalInverseBindMatrices,
        aaSrcGlobalBindMatrices,
        aaiSrcSkinJointIndices,
        aaSrcAnimHierarchy,
        aaSrcJointLocalBindMatrices,
        aaSrcAnimationFrames,
        aaiSrcJointMapIndices,
        aaSrcJointMapping,
        aaSrcDebugJoints,
        aSrcMaterials,
        aSrcAlbedoImageURI,
        aSrcNormalImageURI,
        aSrcMetallicRoughnessImageURI,
        aSrcEmissiveImageURI,
        aiSrcMeshMaterialID
    );

    std::vector<float4> aSrcGlobalAxisAngle;
    for(uint32_t i = 0; i < aaSrcGlobalBindMatrices[0].size(); i++)
    {
        float3 axis = float3(0.0f, 0.0f, 0.0f);
        float fAngle = 0.0f;
        makeAngleAxis(axis, fAngle, aaSrcGlobalBindMatrices[0][i]);
        aSrcGlobalAxisAngle.push_back(float4(axis.x, axis.y, axis.z, fAngle));
    }

    std::vector<float4x4> aSrcLocalBindMatrices;
    computeLocalBindMatrices(
        aSrcLocalBindMatrices,
        aaSrcGlobalBindMatrices[0],
        aaSrcJoints[0],
        aaiSrcJointMapIndices[0],
        aaSrcJointMapping["Armature"]
    );

    std::vector<std::vector<float4x4>> aaSrcLocalAnimationMatrices(aaSrcAnimationFrames.size());
    std::vector<std::vector<AnimFrame>> aaSrcLocalAnimationKeyFrames(aaSrcAnimationFrames.size());
    for(uint32_t iJoint = 0; iJoint < aaSrcAnimationFrames.size(); iJoint++)
    {
        uint32_t iJointIndex = aaSrcAnimationFrames[iJoint][0].miNodeIndex;

        if(iJointIndex >= aaiSrcJointMapIndices[0].size())
        {
            continue;
        }

        computeLocalAnimationMatrices(
            aaSrcLocalAnimationMatrices,
            aaSrcLocalAnimationKeyFrames, 
            aSrcLocalBindMatrices,
            aaSrcAnimationFrames[iJoint],
            aaSrcJoints[0],
            aaiSrcJointMapIndices[0],
            aaSrcJointMapping["Armature"]
        );
    }

    saveMatrices(
        dir,
        baseSrcName,
        aSrcLocalBindMatrices,
        aaSrcGlobalBindMatrices[0]
    );

    testTraverseAnimationNoLocalBind(
        aaSrcJoints[0][0],
        float4x4(),
        aaSrcJoints[0],
        aaSrcAnimationFrames,
        aaiSrcJointMapIndices[0],
        aaSrcJointMapping["Armature"],
        6.52f
    );

    std::vector<float3> aSrcGlobalAnimatedJointPositions;
    std::vector<float4x4> aSrcGlobalAnimatedJointMatrices;
    testTraverseAnimation(
        //aSrcGlobalAnimatedJointPositions,
        aSrcGlobalAnimatedJointMatrices,
        aaSrcJoints[0][0],
        float4x4(),
        aaSrcJoints[0],
        aaSrcLocalAnimationKeyFrames,
        aSrcLocalBindMatrices,
        aaiSrcJointMapIndices[0],
        aaSrcJointMapping["Armature"],
        0.0f
    );

    for(uint32_t i = 0; i < aSrcGlobalAnimatedJointMatrices.size(); i++)
    {
        aSrcGlobalAnimatedJointPositions.push_back(
            float3(
                aSrcGlobalAnimatedJointMatrices[i].mafEntries[3],
                aSrcGlobalAnimatedJointMatrices[i].mafEntries[7],
                aSrcGlobalAnimatedJointMatrices[i].mafEntries[11]
            )
        );
    }

    DEBUG_PRINTF("******************* SRC *********************\n");
    for(uint32_t i = 0; i < (uint32_t)aaSrcGlobalBindMatrices.size(); i++)
    {
        for(uint32_t j = 0; j < (uint32_t)aaSrcGlobalBindMatrices[i].size(); j++)
        {
            uint32_t iJointIndex = aaSrcJoints[i][j].miIndex;
            std::string jointName = aaSrcJointMapping["Armature"][iJointIndex];
            uint32_t iArrayIndex = aaiSrcJointMapIndices[i][iJointIndex];

            float3 pos = float3(aaSrcGlobalBindMatrices[i][iArrayIndex].mafEntries[3], aaSrcGlobalBindMatrices[i][iArrayIndex].mafEntries[7], aaSrcGlobalBindMatrices[i][iArrayIndex].mafEntries[11]);
            /*DEBUG_PRINTF("draw_sphere([%.4f, %.4f, %.4f], 0.01, 255, 0, 0, 255, \"%s\") # joint index: %d array index: %d\n",
                pos.x,
                pos.y,
                pos.z,
                jointName.c_str(),
                iJointIndex,
                iArrayIndex
            );*/
        }
    }

    std::vector<std::vector<float3>> aaDstMeshPositions;
    std::vector<std::vector<float3>> aaDstMeshNormals;
    std::vector<std::vector<float2>> aaDstMeshTexCoords;
    std::vector<std::vector<uint32_t>> aaiDstMeshTriangleIndices;
    std::vector<std::vector<Joint>> aaDstJoints;
    std::vector<std::vector<uint32_t>> aaaiDstJointInfluences;
    std::vector<std::vector<float>> aaafDstJointWeights;
    std::vector<std::vector<float4x4>> aaDstGlobalInverseBindMatrices;
    std::vector<std::vector<float4x4>> aaDstGlobalBindMatrices;
    std::vector<std::vector<uint32_t>> aaiDstSkinJointIndices;
    std::vector<std::vector<Joint>> aaDstAnimHierarchy;
    std::vector<std::vector<float4x4>> aaDstJointLocalBindMatrices;
    std::vector<std::vector<AnimFrame>> aaDstAnimationFrames;
    std::vector<std::vector<uint32_t>> aaiDstJointMapIndices;
    std::map<std::string, std::map<uint32_t, std::string>> aaDstJointMapping;
    std::vector<std::vector<DebugJoint>> aaDstDebugJoints;

    std::vector<OutputMaterialInfo> aDstMaterials;
    std::vector<std::string>        aDstAlbedoImageURI;
    std::vector<std::string>        aDstNormalImageURI;
    std::vector<std::string>        aDstMetallicRoughnessImageURI;
    std::vector<std::string>        aDstEmissiveImageURI;
    std::vector<uint32_t>           aiDstMeshMaterialID;

    std::string baseDstName = "chun-li-rotated-2-texture";
    //std::string baseDstName = "ryu";

    std::string dstGLTFFilePath = dir + "/" + baseDstName + ".gltf";
    std::string dstWADFilePath = dir + "/" + baseDstName + ".wad";
    loadGLTF(
        dstGLTFFilePath,
        dstWADFilePath,
        aaDstMeshPositions,
        aaDstMeshNormals,
        aaDstMeshTexCoords,
        aaiDstMeshTriangleIndices,
        aaDstJoints,
        aaaiDstJointInfluences,
        aaafDstJointWeights,
        aaDstGlobalInverseBindMatrices,
        aaDstGlobalBindMatrices,
        aaiDstSkinJointIndices,
        aaDstAnimHierarchy,
        aaDstJointLocalBindMatrices,
        aaDstAnimationFrames,
        aaiDstJointMapIndices,
        aaDstJointMapping,
        aaDstDebugJoints,
        aDstMaterials,
        aDstAlbedoImageURI,
        aDstNormalImageURI,
        aDstMetallicRoughnessImageURI,
        aDstEmissiveImageURI,
        aiDstMeshMaterialID
    );


#if 0
    float4x4 matchingRootMatrix = rotateMatrixY(3.14159f) * rotateMatrixX(3.14159f * 0.5f) * scale(10.0f, 10.0f, 10.0f);

    // apply root joint's to identity matrix to all joints
    for(uint32_t i = 0; i < aaDstGlobalBindMatrices[0].size(); i++)
    {
        aaDstGlobalBindMatrices[0][i] = matchingRootMatrix * aaDstGlobalBindMatrices[0][i];
    }
#endif // #if 0

    std::vector<float4x4> aDstLocalBindMatrices;
    computeLocalBindMatrices(
        aDstLocalBindMatrices,
        aaDstGlobalBindMatrices[0],
        aaDstJoints[0],
        aaiDstJointMapIndices[0],
        aaDstJointMapping["Armature"]);

    std::vector<float3> aDstGlobalBindJointPositions(aaDstJoints[0].size());
    
    DEBUG_PRINTF("******************* DST *********************\n");
    for(uint32_t i = 0; i < (uint32_t)aaDstGlobalBindMatrices.size(); i++)
    {
        for(uint32_t j = 0; j < (uint32_t)aaDstGlobalBindMatrices[i].size(); j++)
        {
            uint32_t iJointIndex = aaDstJoints[i][j].miIndex;
            std::string jointName = aaDstJointMapping["Armature"][iJointIndex];
            uint32_t iArrayIndex = aaiDstJointMapIndices[i][iJointIndex];

            float3 pos = float3(aaDstGlobalBindMatrices[i][iArrayIndex].mafEntries[3], aaDstGlobalBindMatrices[i][iArrayIndex].mafEntries[7], aaDstGlobalBindMatrices[i][iArrayIndex].mafEntries[11]);
            /*DEBUG_PRINTF("draw_sphere([%.4f, %.4f, %.4f], 0.01, 255, 0, 0, 255, \"%s\") # joint index: %d array index: %d\n",
                pos.x,
                pos.y,
                pos.z,
                jointName.c_str(),
                iJointIndex,
                iArrayIndex
            );*/

            aDstGlobalBindJointPositions[iArrayIndex] = pos;
        }
    }

    std::vector<float4x4> aBatGlobalTransformMatrices;
    std::vector<float> afBatGlobalTransformTimes;

    std::vector< std::vector<AnimFrame>> aaDstMatchingAnimFrames;
    std::vector<float3> aDstGlobalBindJointPositionsCopy = aDstGlobalBindJointPositions;
    for(uint32_t i = 0; i < aaSrcLocalAnimationKeyFrames[0].size(); i++)
    {
        float fTime = aaSrcLocalAnimationKeyFrames[0][i].mfTime;
        std::vector<float3> aSrcGlobalAnimatedJointPositions;
        std::vector<float4x4> aSrcGlobalAnimatedJointMatrices;
        testTraverseAnimation(
            aSrcGlobalAnimatedJointMatrices,
            aaSrcJoints[0][0],
            float4x4(),
            aaSrcJoints[0],
            aaSrcLocalAnimationKeyFrames,
            aSrcLocalBindMatrices,
            aaiSrcJointMapIndices[0],
            aaSrcJointMapping["Armature"],
            fTime
        );

        uint32_t iJointIndex = aaSrcJoints[0][10].miIndex;
        uint32_t iBatArrayIndex = aaiSrcJointMapIndices[0][iJointIndex];
        AnimFrame& animFrame = aaSrcAnimationFrames[iBatArrayIndex][i];

        float4x4 const& batGlobalTransformMatrix = aSrcGlobalAnimatedJointMatrices[iBatArrayIndex];
        aBatGlobalTransformMatrices.push_back(batGlobalTransformMatrix);
        afBatGlobalTransformTimes.push_back(fTime);

        // get global animated positions
        for(uint32_t j = 0; j < aSrcGlobalAnimatedJointMatrices.size(); j++)
        {
            aSrcGlobalAnimatedJointPositions.push_back(
                float3(
                    aSrcGlobalAnimatedJointMatrices[j].mafEntries[3],
                    aSrcGlobalAnimatedJointMatrices[j].mafEntries[7],
                    aSrcGlobalAnimatedJointMatrices[j].mafEntries[11]
                )
            );
        }

        // tangent and binormal of joints
        std::vector<float4x4> const& aSrcGlobalBindMatrices = aaSrcGlobalBindMatrices[0];
        std::vector<float4x4> const& aDstGlobalBindMatrices = aaDstGlobalBindMatrices[0];
        std::map<uint32_t, std::string>& aSrcJointMapping = aaSrcJointMapping["Armature"];
        std::map<uint32_t, std::string>& aDstJointMapping = aaDstJointMapping["Armature"];

        // use the current local animation matrices
        std::vector<float4x4> aSrcLocalAnimationMatrices;
        for(uint32_t j = 0; j < aaSrcLocalAnimationMatrices.size(); j++)
        {
            if(i >= aaSrcLocalAnimationMatrices[j].size())
            {
                continue;
            }

            aSrcLocalAnimationMatrices.push_back(aaSrcLocalAnimationMatrices[j][i]);
        }

        std::vector<AnimFrame> aAnimMatchingKeyFrames;
        std::vector<float4> aAnimMatchingKeyFrameRotations;
        getMatchingAnimationFrames2(
            aAnimMatchingKeyFrames,
            aAnimMatchingKeyFrameRotations,
            aDstGlobalBindMatrices,
            aDstJointMapping,
            aaiDstJointMapIndices[0],
            aaDstJoints[0],

            aSrcGlobalBindMatrices,
            aSrcGlobalAnimatedJointMatrices,
            aSrcJointMapping,
            aaiSrcJointMapIndices[0],
            aaSrcJoints[0],

            dir,
            "chun-li-joint-mapping.json",

            aSrcLocalBindMatrices,
            aDstLocalBindMatrices,

            aaSrcDebugJoints[0],
            aaDstDebugJoints[0],

            aSrcLocalAnimationMatrices,

            fTime
        );
        aaDstMatchingAnimFrames.push_back(aAnimMatchingKeyFrames);

#if 0
        std::vector<AnimFrame> aDstMatchingAnimFrames;
        aDstGlobalBindJointPositions = aDstGlobalBindJointPositionsCopy;
        getMatchingAnimationFrames(
            aDstMatchingAnimFrames,
            aDstGlobalBindJointPositions,

            aDstLocalBindMatrices,
            aaDstGlobalBindMatrices[0],

            aaDstJointMapping["Armature"],
            aaiDstJointMapIndices[0],
            aaDstJoints[0],

            aSrcGlobalAnimatedJointPositions,
            aaSrcJointMapping["Armature"],
            aaiSrcJointMapIndices[0],
            aaSrcJoints[0],
            fTime,

            dir,
            "chun-li-joint-mapping.json",
            
            aSrcGlobalAnimatedJointMatrices,
            aaSrcGlobalBindMatrices[0]
        );
#endif // #if 0

        //aaDstMatchingAnimFrames.push_back(aDstMatchingAnimFrames);
        int iDebug = 1;
    }

    saveMatrices(
        dir,
        baseDstName,
        aDstLocalBindMatrices,
        aaDstGlobalBindMatrices[0]
    );

    // save out animation frames
    {
        std::string dstMatchingAnimationFramePath = dir + "/" + baseDstName + "-" + baseSrcName + "-matching-animation-frames.anm";
        FILE* fp = fopen(dstMatchingAnimationFramePath.c_str(), "wb");
        uint32_t iTotalAnimFrames = (uint32_t)aaDstMatchingAnimFrames.size();
        fwrite(&iTotalAnimFrames, sizeof(uint32_t), 1, fp);
        for(uint32_t i = 0; i < (uint32_t)aaDstMatchingAnimFrames.size(); i++)
        {
            uint32_t iNumMatchingJointFrames = (uint32_t)aaDstMatchingAnimFrames[i].size();
            fwrite(&iNumMatchingJointFrames, sizeof(uint32_t), 1, fp);
            fwrite(aaDstMatchingAnimFrames[i].data(), sizeof(AnimFrame), iNumMatchingJointFrames, fp);
        }
        fclose(fp);

        DEBUG_PRINTF("wrote to: \"%s\"\n", dstMatchingAnimationFramePath.c_str());
    }

    // save materials 
    {
        std::string dstMaterialPath = dir + "/" + baseDstName + ".mat";
        FILE* fp = fopen(dstMaterialPath.c_str(), "wb");
        uint32_t iNumMaterials = (uint32_t)aDstMaterials.size();
        fwrite(&iNumMaterials, sizeof(uint32_t), 1, fp);
        fwrite(aDstMaterials.data(), sizeof(OutputMaterialInfo), iNumMaterials, fp);
        
        char cNewLine = '\n';
        uint32_t iNumAlbedoTextures = (uint32_t)aDstAlbedoImageURI.size();
        fwrite(&iNumAlbedoTextures, sizeof(uint32_t), 1, fp);
        for(auto const& uri : aDstAlbedoImageURI)
        {
            fwrite(uri.c_str(), 1, uri.length(), fp);
            fwrite(&cNewLine, sizeof(char), 1, fp);
        }

        uint32_t iNumNormalTextures = (uint32_t)aDstNormalImageURI.size();
        fwrite(&iNumNormalTextures, sizeof(uint32_t), 1, fp);
        for(auto const& uri : aDstNormalImageURI)
        {
            fwrite(uri.c_str(), 1, uri.length(), fp);
            fwrite(&cNewLine, sizeof(char), 1, fp);
        }

        uint32_t iNumMetallicTextures = (uint32_t)aDstMetallicRoughnessImageURI.size();
        fwrite(&iNumMetallicTextures, sizeof(uint32_t), 1, fp);
        for(auto const& uri : aDstMetallicRoughnessImageURI)
        {
            fwrite(uri.c_str(), 1, uri.length(), fp);
            fwrite(&cNewLine, sizeof(char), 1, fp);
        }

        uint32_t iNumEmissiveTextures = (uint32_t)aDstEmissiveImageURI.size();
        fwrite(&iNumEmissiveTextures, sizeof(uint32_t), 1, fp);
        for(auto const& uri : aDstEmissiveImageURI)
        {
            fwrite(uri.c_str(), 1, uri.length(), fp);
            fwrite(&cNewLine, sizeof(char), 1, fp);
        }
        fclose(fp);

        DEBUG_PRINTF("wrote to %s\n", dstMaterialPath.c_str());

        std::string dstMaterialIDPath = dir + "/" + baseDstName + ".mid";
        fp = fopen(dstMaterialIDPath.c_str(), "wb");
        iNumMaterials = (uint32_t)aiDstMeshMaterialID.size();
        fwrite(&iNumMaterials, sizeof(uint32_t), 1, fp);
        fwrite(aiDstMeshMaterialID.data(), sizeof(uint32_t), iNumMaterials, fp);
        fclose(fp);

        DEBUG_PRINTF("wrote to %s\n", dstMaterialIDPath.c_str());

        // texture names
        // diffuse
        std::string diffuseTextureNameFilePath = dir + "/" + baseDstName + "-texture-names.tex";
        fp = fopen(diffuseTextureNameFilePath.c_str(), "wb");
        uint32_t iNumTextures = (uint32_t)aDstAlbedoImageURI.size();
        char acTextureType[] = {'D', 'F', 'S', 'E'};
        fwrite(acTextureType, sizeof(char), 4, fp);
        fwrite(&iNumTextures, sizeof(uint32_t), 1, fp);
        for(auto const& albedoTextureName : aDstAlbedoImageURI)
        {
            fwrite(albedoTextureName.c_str(), sizeof(char), albedoTextureName.length(), fp);

            char acTerminate[] = {0};
            fwrite(&acTerminate, sizeof(char), 1, fp);
        }

        // emissive
        iNumTextures = (uint32_t)aDstEmissiveImageURI.size();
        acTextureType[0] = 'E'; acTextureType[1] = 'M'; acTextureType[2] = 'S'; acTextureType[3] = 'V';
        fwrite(acTextureType, sizeof(char), 4, fp);
        fwrite(&iNumTextures, sizeof(uint32_t), 1, fp);
        for(auto const& emissiveTextureName : aDstEmissiveImageURI)
        {
            fwrite(emissiveTextureName.c_str(), sizeof(char), emissiveTextureName.length(), fp);

            char acTerminate[] = {0};
            fwrite(&acTerminate, sizeof(char), 1, fp);
        }

        // normal
        iNumTextures = (uint32_t)aDstNormalImageURI.size();
        acTextureType[0] = 'N'; acTextureType[1] = 'R'; acTextureType[2] = 'M'; acTextureType[3] = 'L';
        fwrite(acTextureType, sizeof(char), 4, fp);
        fwrite(&iNumTextures, sizeof(uint32_t), 1, fp);
        for(auto const& normalTextureName : aDstNormalImageURI)
        {
            fwrite(normalTextureName.c_str(), sizeof(char), normalTextureName.length(), fp);

            char acTerminate[] = {0};
            fwrite(&acTerminate, sizeof(char), 1, fp);
        }

        // metal roughness
        iNumTextures = (uint32_t)aDstNormalImageURI.size();
        acTextureType[0] = 'M'; acTextureType[1] = 'L'; acTextureType[2] = 'R'; acTextureType[3] = 'H';
        fwrite(acTextureType, sizeof(char), 4, fp);
        fwrite(&iNumTextures, sizeof(uint32_t), 1, fp);
        for(auto const& metalRoughnessName : aDstMetallicRoughnessImageURI)
        {
            fwrite(metalRoughnessName.c_str(), sizeof(char), metalRoughnessName.length(), fp);

            char acTerminate[] = {0};
            fwrite(&acTerminate, sizeof(char), 1, fp);
        }
        fclose(fp);

        DEBUG_PRINTF("wrote to %s\n", diffuseTextureNameFilePath.c_str());
    }

    {
        std::string filePath = dir + "/" + baseSrcName + "-bat-global-transform-matrices.bin";
        FILE* fp = fopen(filePath.c_str(), "wb");
        uint32_t iNumFrames = (uint32_t)afBatGlobalTransformTimes.size();
        fwrite(&iNumFrames, sizeof(uint32_t), 1, fp);
        fwrite(aBatGlobalTransformMatrices.data(), sizeof(float4x4), aBatGlobalTransformMatrices.size(), fp);
        fwrite(afBatGlobalTransformTimes.data(), sizeof(float), afBatGlobalTransformTimes.size(), fp);
        fclose(fp);
        DEBUG_PRINTF("wrote to %s\n", filePath.c_str());
    }

    return 0;
}

/*
**
*/
void saveMatrices(
    std::string const& dir,
    std::string const& baseName,
    std::vector<float4x4> const& aLocalBindMatrices,
    std::vector<float4x4> const& aGlobalBindMatrices)
{
    // save out dst local bind matrices
    {
        std::string dstLocalBindMatrixFilePath = dir + "/" + baseName + "-local-bind-matrices.bin";
        FILE* fp = fopen(dstLocalBindMatrixFilePath.c_str(), "wb");
        uint32_t iNumJoints = (uint32_t)aLocalBindMatrices.size();
        fwrite(&iNumJoints, sizeof(uint32_t), 1, fp);
        fwrite(aLocalBindMatrices.data(), sizeof(float4x4), iNumJoints, fp);
        fclose(fp);

        DEBUG_PRINTF("wrote to: \"%s\"\n", dstLocalBindMatrixFilePath.c_str());
    }

    // save out dst global bind matrices
    {
        std::string dstGlobalBindMatrixFilePath = dir + "/" + baseName + "-global-bind-matrices.bin";
        FILE* fp = fopen(dstGlobalBindMatrixFilePath.c_str(), "wb");
        uint32_t iNumJoints = (uint32_t)aGlobalBindMatrices.size();
        fwrite(&iNumJoints, sizeof(uint32_t), 1, fp);
        fwrite(aGlobalBindMatrices.data(), sizeof(float4x4), iNumJoints, fp);
        fclose(fp);

        DEBUG_PRINTF("wrote to: \"%s\"\n", dstGlobalBindMatrixFilePath.c_str());
    }

    // save out dst inverse global bind matrices
    {
        std::vector<float4x4> aDstInverseGlobalBindMatrices(aGlobalBindMatrices.size());
        for(uint32_t i = 0; i < (uint32_t)aGlobalBindMatrices.size(); i++)
        {
            aDstInverseGlobalBindMatrices[i] = invert(aGlobalBindMatrices[i]);
            float4x4 verify = aGlobalBindMatrices[i] * aDstInverseGlobalBindMatrices[i];
            assert(verify.identical(float4x4(), 1.0e-4f));
        }

        std::string dstInverseGlobalBindMatrixPath = dir + "/" + baseName + "-inverse-global-bind-matrices.bin";
        FILE* fp = fopen(dstInverseGlobalBindMatrixPath.c_str(), "wb");
        uint32_t iNumJoints = (uint32_t)aDstInverseGlobalBindMatrices.size();
        fwrite(&iNumJoints, sizeof(uint32_t), 1, fp);
        fwrite(aDstInverseGlobalBindMatrices.data(), sizeof(float4x4), iNumJoints, fp);
        fclose(fp);

        DEBUG_PRINTF("wrote to: \"%s\"\n", dstInverseGlobalBindMatrixPath.c_str());
    }
}