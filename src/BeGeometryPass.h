#pragma once

#include "BeRenderPass.h"
#include <vector>
#include <glm.hpp>
#include <gtc/quaternion.hpp>
#include "BeModel.h"
#include "BeTexture.h"

class BeShader;
struct UniformData;

//!Render pass that writes geometry data to G-Buffers
class BeGeometryPass : public BeRenderPass {
public:
    //fields///////////////////////////////////////////////////////////////////////////////////////

    struct ObjectEntry {
        std::string Name;
        glm::vec3 Position = {0.f, 0.f, 0.f};
        glm::quat Rotation = glm::quat(glm::vec3(0, 0, 0));
        glm::vec3 Scale = {1.f, 1.f, 1.f};
        BeModel* Model;
        std::vector<BeModel::BeDrawSlice> DrawSlices;
        BeShader* Shader;
    };

    //initialisation///////////////////////////////////////////////////////////////////////////////////////

    auto Setup(
        const ComPtr<ID3D11Device>& device,
        BeRenderGraph* graph
    ) -> void override;

    //public interface///////////////////////////////////////////////////////////////////////////////////////

    auto GetName() const -> const std::string& override { return _name; }

    auto Execute(const ComPtr<ID3D11DeviceContext>& context) -> void override;

    auto GetOutputResources() const -> std::vector<std::string> override;

    //!< Set scene objects to render
    auto SetObjects(const std::vector<ObjectEntry>& objects) -> void { _objects = objects; }

    //!< Set uniform buffer data
    auto SetUniformData(const class UniformData& uniformData) -> void { _uniformData = uniformData; }

    //!< Set clear color
    auto SetClearColor(const glm::vec3& color) -> void { _clearColor = color; }

    //private logic///////////////////////////////////////////////////////////////////////////////////////

private:
    auto createGeometry() -> void;

    std::string _name = "GeometryPass";
    std::vector<ObjectEntry> _objects;
    glm::vec3 _clearColor = {0.f, 0.f, 0.f};
    class UniformData _uniformData;

    ComPtr<ID3D11Device> _device;
    ComPtr<ID3D11Buffer> _uniformBuffer;
    ComPtr<ID3D11Buffer> _materialBuffer;
    ComPtr<ID3D11DepthStencilState> _depthStencilState;
    ComPtr<ID3D11Buffer> _sharedVertexBuffer;
    ComPtr<ID3D11Buffer> _sharedIndexBuffer;
    ComPtr<ID3D11SamplerState> _pointSampler;

    BeTexture* _whiteFallbackTexture = nullptr;
    BeRenderGraph* _graph = nullptr;
};