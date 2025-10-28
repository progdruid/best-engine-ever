#include "BeCompositionPass.h"
#include "BeRenderGraph.h"
#include "BeRenderResource.h"
#include "BeShader.h"
#include "Utils.h"
#include <glm.hpp>

auto BeCompositionPass::Setup(
    const ComPtr<ID3D11Device>& device,
    BeRenderGraph* graph
) -> void {
    _device = device;
    _graph = graph;

    //!< Create point sampler state
    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.MinLOD = 0;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    Utils::Check << device->CreateSamplerState(&samplerDesc, &_pointSampler);

    //!< Load shader
    _shader = std::make_unique<BeShader>(
        device.Get(),
        "assets/shaders/fullscreen",
        std::vector<class BeVertexElementDescriptor>{}
    );
}

auto BeCompositionPass::Execute(const ComPtr<ID3D11DeviceContext>& context) -> void {
    if (!_graph || !_shader || !_backbufferRTV) {
        return;
    }

    //!< Get input resources
    auto depthBuffer = _graph->GetResource("DepthBuffer");
    auto gbuffer0 = _graph->GetResource("GBuffer0");
    auto gbuffer1 = _graph->GetResource("GBuffer1");
    auto gbuffer2 = _graph->GetResource("GBuffer2");
    auto lightingBuffer = _graph->GetResource("LightingBuffer");

    if (!depthBuffer || !gbuffer0 || !gbuffer1 || !gbuffer2 || !lightingBuffer) {
        return;
    }

    //!< Clear and set backbuffer render target
    auto fullClearColor = glm::vec4(_clearColor, 1.0f);
    context->ClearRenderTargetView(_backbufferRTV, reinterpret_cast<FLOAT*>(&fullClearColor));
    context->OMSetRenderTargets(1, &_backbufferRTV, nullptr);

    //!< Bind shader
    _shader->Bind(context.Get());

    //!< Bind input resources
    ID3D11ShaderResourceView* srvs[5] = {
        depthBuffer->SRV.Get(),
        gbuffer0->SRV.Get(),
        gbuffer1->SRV.Get(),
        gbuffer2->SRV.Get(),
        lightingBuffer->SRV.Get()
    };
    context->PSSetShaderResources(0, 5, srvs);

    //!< Bind sampler
    context->PSSetSamplers(0, 1, _pointSampler.GetAddressOf());

    //!< Draw fullscreen quad
    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    context->Draw(4, 0);

    //!< Cleanup
    {
        ID3D11ShaderResourceView* emptyResources[5] = { nullptr, nullptr, nullptr, nullptr, nullptr };
        context->PSSetShaderResources(0, 5, emptyResources);
        ID3D11SamplerState* emptySamplers[1] = { nullptr };
        context->PSSetSamplers(0, 1, emptySamplers);
        ID3D11RenderTargetView* emptyTargets[1] = { nullptr };
        context->OMSetRenderTargets(1, emptyTargets, nullptr);
    }
}

auto BeCompositionPass::GetInputResources() const -> std::vector<std::string> {
    return {"DepthBuffer", "GBuffer0", "GBuffer1", "GBuffer2", "LightingBuffer"};
}
