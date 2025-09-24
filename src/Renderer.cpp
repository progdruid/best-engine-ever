﻿#include "Renderer.h"

#include <d3dcompiler.h>
#include <glm.hpp>
#include <gtc/quaternion.hpp>
#include <gtc/type_ptr.hpp>

#include "BeShader.h"
#include "Utils.h"

Renderer::Renderer(HWND windowHandle, uint32_t width, uint32_t height) {

    _active = false;

    _windowHandle = windowHandle;
    _width = width;
    _height = height;

    UINT deviceFlags = 0;
#if defined(_DEBUG)
    deviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    
    // Device and context
    Utils::ThrowIfFailed(D3D11CreateDevice(
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
    ));

    
    // DXGI interfaces
    Utils::ThrowIfFailed(_device.As(&_dxgiDevice));
    Utils::ThrowIfFailed(_dxgiDevice->GetAdapter(&_adapter));
    Utils::ThrowIfFailed(_adapter->GetParent(IID_PPV_ARGS(&_factory)));
    
    // Swap chain
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
    Utils::ThrowIfFailed(_factory->CreateSwapChainForHwnd(_device.Get(), _windowHandle, &scDesc, nullptr, nullptr, &_swapchain));
    Utils::ThrowIfFailed(_factory->MakeWindowAssociation(_windowHandle, DXGI_MWA_NO_ALT_ENTER));

    
    // Create RTV
    {
        ComPtr<ID3D11Texture2D> backBuffer;
        Utils::ThrowIfFailed(_swapchain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)));
        Utils::ThrowIfFailed(_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &_renderTarget));
    
        D3D11_TEXTURE2D_DESC depthStencilDescriptor = {
            .Width = _width,
            .Height = _height,
            .MipLevels = 1,
            .ArraySize = 1,
            .Format = DXGI_FORMAT_D24_UNORM_S8_UINT, // 24-bit depth, 8-bit stencil
            .SampleDesc = { .Count = 1, .Quality = 0 },
            .Usage = D3D11_USAGE_DEFAULT,
            .BindFlags = D3D11_BIND_DEPTH_STENCIL,
            .CPUAccessFlags = 0,
            .MiscFlags = 0,
        };
        ComPtr<ID3D11Texture2D> depthStencilBuffer;
        Utils::ThrowIfFailed(_device->CreateTexture2D(&depthStencilDescriptor,nullptr, depthStencilBuffer.GetAddressOf()));
        Utils::ThrowIfFailed(_device->CreateDepthStencilView(depthStencilBuffer.Get(), nullptr, _depthStencilView.GetAddressOf()));

        D3D11_DEPTH_STENCIL_DESC depthStencilStateDescriptor = {
            .DepthEnable = true,
            .DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL,
            .DepthFunc = D3D11_COMPARISON_LESS,
            .StencilEnable = false,
        };
        
        Utils::ThrowIfFailed(_device->CreateDepthStencilState(&depthStencilStateDescriptor, _depthStencilState.GetAddressOf()));
        _context->OMSetDepthStencilState(_depthStencilState.Get(), 1);
    }
    
    
    // Compile shaders
    _shader = std::make_shared<BeShader>(
        _device.Get(),
        L"assets/shaders/default",
        std::vector<BeVertexElementDescriptor>{
            {.Name = "Position", .Attribute = BeVertexElementDescriptor::BeVertexSemantic::Position},
            {.Name = "Color", .Attribute = BeVertexElementDescriptor::BeVertexSemantic::Color3}
        }
    );

    
    _objects.push_back({
        .Position = {0, 0, -7},
        .Model = std::make_unique<BeModel>("assets/model.fbx", _device.Get()),
    });
    _objects.push_back({
        .Position = {7.5f, 1, -4},
        .Rotation = glm::quat(glm::vec3(0, glm::radians(150.f), 0)),
        .Model = std::make_unique<BeModel>("assets/floppy-disks.glb", _device.Get()),
    });
    _objects.push_back({
        .Position = {0, 0, 8},
        .Scale = glm::vec3(0.2f),
        .Model = std::make_unique<BeModel>("assets/pagoda.glb", _device.Get()),
        .Shader = std::make_shared<BeShader>(
            _device.Get(),
            L"assets/shaders/textured",
            std::vector<BeVertexElementDescriptor>{
                {.Name = "Position", .Attribute = BeVertexElementDescriptor::BeVertexSemantic::Position},
                {.Name = "UV", .Attribute = BeVertexElementDescriptor::BeVertexSemantic::TexCoord0},
            }
        )
    });
    _objects.push_back({
        .Position = {-3, 0, 5},
        .Scale = glm::vec3(3.f),
        .Model = std::make_unique<BeModel>("assets/witch_items.glb", _device.Get()),
        .Shader = std::make_shared<BeShader>(
            _device.Get(),
            L"assets/shaders/textured",
            std::vector<BeVertexElementDescriptor>{
                {.Name = "Position", .Attribute = BeVertexElementDescriptor::BeVertexSemantic::Position},
                {.Name = "UV", .Attribute = BeVertexElementDescriptor::BeVertexSemantic::TexCoord0},
            }
        )
    });
    _objects.push_back({
        .Position = {7, 0, 5},
        //.Rotation = glm::quat(glm::vec3(glm::radians(-90.f), glm::radians(180.f), 0)),
        .Scale = glm::vec3(0.2f),
        .Model = std::make_unique<BeModel>("assets/lowpoly_pixelart_anvil.glb", _device.Get()),
        .Shader = std::make_shared<BeShader>(
            _device.Get(),
            L"assets/shaders/textured",
            std::vector<BeVertexElementDescriptor>{
                {.Name = "Position", .Attribute = BeVertexElementDescriptor::BeVertexSemantic::Position},
                {.Name = "UV", .Attribute = BeVertexElementDescriptor::BeVertexSemantic::TexCoord0},
            }
        )
    });
    
    size_t totalVerticesNumber = 0;
    size_t totalIndicesNumber = 0;
    size_t totalDrawInstructions = 0;
    for (const auto& object : _objects) {
        totalVerticesNumber += object.Model->FullVertices.size();
        totalIndicesNumber += object.Model->Indices.size();
        totalDrawInstructions += object.Model->MeshInstructions.size();
    }
    
    std::vector<BeFullVertex> fullVertices;
    std::vector<uint32_t> indices;
    fullVertices.reserve(totalVerticesNumber);
    indices.reserve(totalIndicesNumber);
    for (auto& object : _objects) {
        fullVertices.insert(fullVertices.end(), object.Model->FullVertices.begin(), object.Model->FullVertices.end());
        indices.insert(indices.end(), object.Model->Indices.begin(), object.Model->Indices.end());
        for (BeModel::BeMeshInstruction instruction : object.Model->MeshInstructions) {
            instruction.BaseVertexLocation += static_cast<int32_t>(fullVertices.size() - object.Model->FullVertices.size());
            instruction.StartIndexLocation += static_cast<uint32_t>(indices.size() - object.Model->Indices.size());
            object.MeshInstructions.push_back(instruction);
        }
    }
    
    D3D11_BUFFER_DESC vertexBufferDescriptor = {};
    vertexBufferDescriptor.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vertexBufferDescriptor.Usage = D3D11_USAGE_DEFAULT;
    vertexBufferDescriptor.ByteWidth = static_cast<UINT>(fullVertices.size() * sizeof(BeFullVertex));
    D3D11_SUBRESOURCE_DATA vertexData = {};
    vertexData.pSysMem = fullVertices.data();
    Utils::ThrowIfFailed(_device->CreateBuffer(&vertexBufferDescriptor, &vertexData, &_sharedVertexBuffer));
    
    D3D11_BUFFER_DESC indexBufferDescriptor = {};
    indexBufferDescriptor.BindFlags = D3D11_BIND_INDEX_BUFFER;
    indexBufferDescriptor.Usage = D3D11_USAGE_DEFAULT;
    indexBufferDescriptor.ByteWidth = static_cast<UINT>(indices.size() * sizeof(uint32_t));
    D3D11_SUBRESOURCE_DATA indexData = {};
    indexData.pSysMem = indices.data();
    Utils::ThrowIfFailed(_device->CreateBuffer(&indexBufferDescriptor, &indexData, &_sharedIndexBuffer));
    
    D3D11_BUFFER_DESC uniformBufferDescriptor = {};
    uniformBufferDescriptor.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    uniformBufferDescriptor.Usage = D3D11_USAGE_DYNAMIC;
    uniformBufferDescriptor.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    uniformBufferDescriptor.ByteWidth = sizeof(UniformBufferData);
    Utils::ThrowIfFailed(_device->CreateBuffer(&uniformBufferDescriptor, nullptr, &_uniformBuffer));
    
    D3D11_BUFFER_DESC objectBufferDescriptor = {};
    objectBufferDescriptor.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    objectBufferDescriptor.Usage = D3D11_USAGE_DYNAMIC;
    objectBufferDescriptor.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    objectBufferDescriptor.ByteWidth = sizeof(ObjectBufferData);
    Utils::ThrowIfFailed(_device->CreateBuffer(&objectBufferDescriptor, nullptr, &_objectBuffer));

    
    // Create point sampler state
    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.MinLOD = 0;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    Utils::ThrowIfFailed(_device->CreateSamplerState(&samplerDesc, &_pointSampler));

    
    _active = true;
}

auto Renderer::render() -> void {
    
    // Clear the back buffer
    auto fullClearColor = glm::vec4(ClearColor, 1.0f);
    _context->ClearRenderTargetView(_renderTarget.Get(), reinterpret_cast<FLOAT*>(&fullClearColor));
    _context->ClearDepthStencilView(_depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    _context->OMSetRenderTargets(1, _renderTarget.GetAddressOf(), _depthStencilView.Get());


    // Set viewport
    D3D11_VIEWPORT viewport;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = static_cast<FLOAT>(_width);
    viewport.Height = static_cast<FLOAT>(_height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    _context->RSSetViewports(1, &viewport);
    
    // Bind shaders
    _shader->bind(_context.Get());

    // Update uniform constant buffer
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    Utils::ThrowIfFailed(_context->Map(_uniformBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource));
    memcpy(mappedResource.pData, &_uniformData, sizeof(UniformBufferData));
    _context->Unmap(_uniformBuffer.Get(), 0);
    _context->VSSetConstantBuffers(0, 1, _uniformBuffer.GetAddressOf());

    
    // Draw call
    uint32_t stride = sizeof(BeFullVertex);
    uint32_t offset = 0;
    _context->IASetVertexBuffers(0, 1, _sharedVertexBuffer.GetAddressOf(), &stride, &offset);
    _context->IASetIndexBuffer(_sharedIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    _context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    //here write and send a point sampler
    _context->PSSetSamplers(0, 1, _pointSampler.GetAddressOf());
    
    for (const auto& object : _objects) {
        object.Shader ? object.Shader->bind(_context.Get()) : _shader->bind(_context.Get());
        
        // Update object constant buffer
        ObjectBufferData objData;
        objData.Model =
            glm::translate(glm::mat4(1.0f), object.Position) *
            glm::mat4_cast(object.Rotation) *
            glm::scale(glm::mat4(1.0f), object.Scale);
        Utils::ThrowIfFailed(_context->Map(_objectBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource));
        memcpy(mappedResource.pData, &objData, sizeof(ObjectBufferData));
        _context->Unmap(_objectBuffer.Get(), 0);
        _context->VSSetConstantBuffers(1, 1, _objectBuffer.GetAddressOf());
        
        for (const auto& instruction : object.MeshInstructions) {
            if (instruction.DiffuseTexture)
                _context->PSSetShaderResources(0, 1, instruction.DiffuseTexture.GetAddressOf());
            _context->DrawIndexed(instruction.IndexCount, instruction.StartIndexLocation, instruction.BaseVertexLocation);
            _context->PSSetShaderResources(0, 0, nullptr); // unbind texture
        }
    }
    
    _swapchain->Present(1, 0);
}

auto Renderer::setProjectionView(const glm::mat4x4& projectionView) -> void {
    _uniformData.ProjectionView = projectionView;
}

auto Renderer::terminateRenderer() -> void {
    _active = false;
    
    _renderTarget.Reset();
    _swapchain.Reset();
    _factory.Reset();
    _adapter.Reset();
    _dxgiDevice.Reset();
    _context.Reset();
    _device.Reset();
}
