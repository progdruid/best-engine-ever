#pragma once

#include "BeRenderPass.h"
#include <glm.hpp>
#include <vector>

struct PointLightData;
class BeShader;

//!Render pass that applies point lighting
class BePointLightPass : public BeRenderPass {
public:
    //initialisation///////////////////////////////////////////////////////////////////////////////////////

    auto Setup(
        const ComPtr<ID3D11Device>& device,
        BeRenderGraph* graph
    ) -> void override;

    //public interface///////////////////////////////////////////////////////////////////////////////////////

    auto GetName() const -> const std::string& override { return _name; }

    auto Execute(const ComPtr<ID3D11DeviceContext>& context) -> void override;

    auto GetInputResources() const -> std::vector<std::string> override;

    auto GetOutputResources() const -> std::vector<std::string> override;

    //!< Set point lights to render
    auto SetPointLights(const std::vector<PointLightData>& lights) -> void { _lights = lights; }

    //private logic///////////////////////////////////////////////////////////////////////////////////////

private:
    std::string _name = "PointLightPass";
    std::vector<PointLightData> _lights;

    ComPtr<ID3D11Device> _device;
    ComPtr<ID3D11Buffer> _lightBuffer;
    ComPtr<ID3D11BlendState> _lightingBlendState;
    ComPtr<ID3D11SamplerState> _pointSampler;
    std::unique_ptr<BeShader> _shader;

    BeRenderGraph* _graph = nullptr;
};