#pragma once

#include "BeRenderPass.h"
#include <glm.hpp>

class BeShader;

//!Final composition pass that combines lighting and G-Buffers to screen
class BeCompositionPass : public BeRenderPass {
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

    //!< Get the backbuffer RTV (set externally)
    auto SetBackbufferRTV(ID3D11RenderTargetView* rtv) -> void { _backbufferRTV = rtv; }

    //!< Set clear color
    auto SetClearColor(const glm::vec3& color) -> void { _clearColor = color; }

    //private logic///////////////////////////////////////////////////////////////////////////////////////

private:
    std::string _name = "CompositionPass";
    glm::vec3 _clearColor = {0.f, 0.f, 0.f};
    ID3D11RenderTargetView* _backbufferRTV = nullptr;

    ComPtr<ID3D11Device> _device;
    ComPtr<ID3D11SamplerState> _pointSampler;
    std::unique_ptr<BeShader> _shader;

    BeRenderGraph* _graph = nullptr;
};
