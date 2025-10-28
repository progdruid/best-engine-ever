#include "BePointLightPass.h"
#include "BeRenderGraph.h"
#include "BeRenderResource.h"
#include "BeShader.h"
#include "BeBuffers.h"
#include "Utils.h"

auto BePointLightPass::Setup(
    const ComPtr<ID3D11Device>& device,
    BeRenderGraph* graph
) -> void {
    _device = device;
    _graph = graph;

    //!< Create constant buffer
    D3D11_BUFFER_DESC lightBufferDescriptor = {};
    lightBufferDescriptor.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    lightBufferDescriptor.Usage = D3D11_USAGE_DYNAMIC;
    lightBufferDescriptor.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    lightBufferDescriptor.ByteWidth = sizeof(PointLightBufferGPU);
    Utils::Check << device->CreateBuffer(&lightBufferDescriptor, nullptr, &_lightBuffer);

    //!< Create blend state for additive lighting
    D3D11_BLEND_DESC lightingBlendDesc = {};
    lightingBlendDesc.RenderTarget[0].BlendEnable = TRUE;
    lightingBlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    lightingBlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
    lightingBlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    lightingBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    lightingBlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    lightingBlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    lightingBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    Utils::Check << device->CreateBlendState(&lightingBlendDesc, &_lightingBlendState);

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
        "assets/shaders/pointLight",
        std::vector<class BeVertexElementDescriptor>{}
    );
}

auto BePointLightPass::Execute(const ComPtr<ID3D11DeviceContext>& context) -> void {
    if (!_graph || !_shader || _lights.empty()) {
        return;
    }

    //!< Get input resources
    auto depthBuffer = _graph->GetResource("DepthBuffer");
    auto gbuffer0 = _graph->GetResource("GBuffer0");
    auto gbuffer1 = _graph->GetResource("GBuffer1");
    auto gbuffer2 = _graph->GetResource("GBuffer2");
    auto lightingTarget = _graph->GetResource("LightingBuffer");

    if (!depthBuffer || !gbuffer0 || !gbuffer1 || !gbuffer2 || !lightingTarget) {
        return;
    }

    //!< Set render target
    context->OMSetRenderTargets(1, lightingTarget->RTV.GetAddressOf(), nullptr);
    context->OMSetBlendState(_lightingBlendState.Get(), nullptr, 0xFFFFFFFF);

    //!< Bind shader
    _shader->Bind(context.Get());

    //!< Bind G-Buffer resources
    ID3D11ShaderResourceView* srvs[4] = {
        depthBuffer->SRV.Get(),
        gbuffer0->SRV.Get(),
        gbuffer1->SRV.Get(),
        gbuffer2->SRV.Get()
    };
    context->PSSetShaderResources(0, 4, srvs);

    //!< Bind sampler
    context->PSSetSamplers(0, 1, _pointSampler.GetAddressOf());

    //!< For each point light
    for (const auto& lightData : _lights) {
        //!< Update light constant buffer
        {
            PointLightBufferGPU lightBuffer(lightData);
            D3D11_MAPPED_SUBRESOURCE mappedResource;
            Utils::Check << context->Map(_lightBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
            memcpy(mappedResource.pData, &lightBuffer, sizeof(PointLightBufferGPU));
            context->Unmap(_lightBuffer.Get(), 0);
            context->PSSetConstantBuffers(1, 1, _lightBuffer.GetAddressOf());
        }

        //!< Draw fullscreen quad
        context->IASetInputLayout(nullptr);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        context->Draw(4, 0);
    }

    //!< Cleanup
    {
        ID3D11ShaderResourceView* emptyResources[4] = { nullptr, nullptr, nullptr, nullptr };
        context->PSSetShaderResources(0, 4, emptyResources);
        ID3D11Buffer* emptyBuffers[1] = { nullptr };
        context->PSSetConstantBuffers(1, 1, emptyBuffers);
        ID3D11SamplerState* emptySamplers[1] = { nullptr };
        context->PSSetSamplers(0, 1, emptySamplers);
        ID3D11RenderTargetView* emptyTargets[1] = { nullptr };
        context->OMSetRenderTargets(1, emptyTargets, nullptr);
        context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
    }
}

auto BePointLightPass::GetInputResources() const -> std::vector<std::string> {
    return {"DepthBuffer", "GBuffer0", "GBuffer1", "GBuffer2"};
}

auto BePointLightPass::GetOutputResources() const -> std::vector<std::string> {
    return {"LightingBuffer"};
}
