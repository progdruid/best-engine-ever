#include "BeRenderer.h"

#include <d3dcompiler.h>
#include <glm.hpp>
#include <gtc/quaternion.hpp>
#include <gtc/type_ptr.hpp>

#include "BeShader.h"
#include "BeRenderGraph.h"
#include "BeGeometryPass.h"
#include "BeDirectionalLightPass.h"
#include "BePointLightPass.h"
#include "BeCompositionPass.h"
#include "BeRenderResource.h"
#include "Utils.h"

BeRenderer::BeRenderer(const HWND windowHandle, const uint32_t width, const uint32_t height) {
    _windowHandle = windowHandle;
    _width = width;
    _height = height;
}

auto BeRenderer::LaunchDevice() -> void {

    UINT deviceFlags = 0;
#if defined(_DEBUG)
    deviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    //!< Device and context
    Utils::Check << D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        deviceFlags,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &_device,
        nullptr,
        &_context
    );

    //!< DXGI interfaces
    Utils::Check
    << _device.As(&_dxgiDevice)
    << _dxgiDevice->GetAdapter(&_adapter)
    << _adapter->GetParent(IID_PPV_ARGS(&_factory));

    //!< Swap chain
    DXGI_SWAP_CHAIN_DESC1 scDesc = {
        .Width = _width,
        .Height = _height,
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .SampleDesc = { .Count = 1 },
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = 2,
        .Scaling = DXGI_SCALING_STRETCH,
        .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
        .AlphaMode = DXGI_ALPHA_MODE_IGNORE,
    };

    Utils::Check << _factory->CreateSwapChainForHwnd(_device.Get(), _windowHandle, &scDesc, nullptr, nullptr, &_swapchain);
    Utils::Check << _factory->MakeWindowAssociation(_windowHandle, DXGI_MWA_NO_ALT_ENTER);

    //!< Get backbuffer RTV
    {
        ComPtr<ID3D11Texture2D> backBuffer;
        Utils::Check
        << _swapchain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))
        << _device->CreateRenderTargetView(backBuffer.Get(), nullptr, &_backbufferTarget);
    }

    //!< Create and setup render graph
    _renderGraph = std::make_unique<BeRenderGraph>(_device, _width, _height);

    //!< Register render resources with the graph
    BeRenderResource::BeResourceDesc depthDesc;
    depthDesc.Type = BeRenderResource::BeResourceType::Texture2D;
    depthDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
    depthDesc.Width = _width;
    depthDesc.Height = _height;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    auto depthResource = _renderGraph->GetOrCreateResource("DepthBuffer", depthDesc);

    //!< G-Buffer 0: Diffuse (R8G8B8A8_UNORM)
    BeRenderResource::BeResourceDesc gbuffer0Desc;
    gbuffer0Desc.Type = BeRenderResource::BeResourceType::Texture2D;
    gbuffer0Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    gbuffer0Desc.Width = _width;
    gbuffer0Desc.Height = _height;
    gbuffer0Desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    _renderGraph->GetOrCreateResource("GBuffer0", gbuffer0Desc);

    //!< G-Buffer 1: Normal (R16G16B16A16_FLOAT)
    BeRenderResource::BeResourceDesc gbuffer1Desc;
    gbuffer1Desc.Type = BeRenderResource::BeResourceType::Texture2D;
    gbuffer1Desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    gbuffer1Desc.Width = _width;
    gbuffer1Desc.Height = _height;
    gbuffer1Desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    _renderGraph->GetOrCreateResource("GBuffer1", gbuffer1Desc);

    //!< G-Buffer 2: Specular (R8G8B8A8_UNORM)
    BeRenderResource::BeResourceDesc gbuffer2Desc;
    gbuffer2Desc.Type = BeRenderResource::BeResourceType::Texture2D;
    gbuffer2Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    gbuffer2Desc.Width = _width;
    gbuffer2Desc.Height = _height;
    gbuffer2Desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    _renderGraph->GetOrCreateResource("GBuffer2", gbuffer2Desc);

    //!< Lighting buffer (R11G11B10_FLOAT)
    BeRenderResource::BeResourceDesc lightingDesc;
    lightingDesc.Type = BeRenderResource::BeResourceType::Texture2D;
    lightingDesc.Format = DXGI_FORMAT_R11G11B10_FLOAT;
    lightingDesc.Width = _width;
    lightingDesc.Height = _height;
    lightingDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    _renderGraph->GetOrCreateResource("LightingBuffer", lightingDesc);

    //!< Add render passes to graph
    _geometryPass = _renderGraph->AddPass<BeGeometryPass>();
    _directionalLightPass = _renderGraph->AddPass<BeDirectionalLightPass>();
    _pointLightPass = _renderGraph->AddPass<BePointLightPass>();
    _compositionPass = _renderGraph->AddPass<BeCompositionPass>();

    //!< Build the render graph
    _renderGraph->Build();

    //!< Set viewport
    D3D11_VIEWPORT viewport = {
        .TopLeftX = 0,
        .TopLeftY = 0,
        .Width = static_cast<FLOAT>(_width),
        .Height = static_cast<FLOAT>(_height),
        .MinDepth = 0.0f,
        .MaxDepth = 1.0f
    };
    _context->RSSetViewports(1, &viewport);
}

auto BeRenderer::PushObjects(const std::vector<ObjectEntry>& objects) -> void {
    _objects = objects;

    //!< Convert to geometry pass format and push to the pass
    std::vector<BeGeometryPass::ObjectEntry> geometryPassObjects;
    geometryPassObjects.reserve(_objects.size());

    for (const auto& obj : _objects) {
        BeGeometryPass::ObjectEntry entry;
        entry.Name = obj.Name;
        entry.Position = obj.Position;
        entry.Rotation = obj.Rotation;
        entry.Scale = obj.Scale;
        entry.Model = obj.Model;
        entry.DrawSlices = obj.DrawSlices;
        entry.Shader = obj.Shader;
        geometryPassObjects.push_back(entry);
    }

    if (_geometryPass) {
        _geometryPass->SetObjects(geometryPassObjects);
    }
}

auto BeRenderer::Render() -> void {
    if (!_renderGraph || !_geometryPass || !_directionalLightPass ||
        !_pointLightPass || !_compositionPass) {
        return;
    }

    //!< Set up pass data
    _geometryPass->SetUniformData(UniformData);
    _geometryPass->SetClearColor(ClearColor);

    _directionalLightPass->SetLightData(DirectionalLightData);

    _pointLightPass->SetPointLights(PointLights);

    _compositionPass->SetBackbufferRTV(_backbufferTarget.Get());
    _compositionPass->SetClearColor(ClearColor);

    //!< Execute the render graph
    _renderGraph->Execute(_context);

    //!< Present
    _swapchain->Present(1, 0);
}

auto BeRenderer::TerminateRenderer() -> void {
    _backbufferTarget.Reset();
    _swapchain.Reset();
    _factory.Reset();
    _adapter.Reset();
    _dxgiDevice.Reset();
    _context.Reset();
    _device.Reset();
}
