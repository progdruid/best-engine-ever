#pragma once

#include <d3d11.h>
#include <dxgi1_2.h>
#include <glm.hpp>
#include <gtc/quaternion.hpp>
#include <vector>
#include <wrl/client.h>
#include <memory>

#include "BeModel.h"
#include "BeBuffers.h"

class BeShader;
class BeRenderGraph;
class BeGeometryPass;
class BeDirectionalLightPass;
class BePointLightPass;
class BeCompositionPass;

using Microsoft::WRL::ComPtr;


class BeRenderer {
public:
    struct ObjectEntry {
        std::string Name;
        glm::vec3 Position = {0.f, 0.f, 0.f};
        glm::quat Rotation = glm::quat(glm::vec3(0, 0, 0));
        glm::vec3 Scale = {1.f, 1.f, 1.f};
        BeModel* Model;
        std::vector<BeModel::BeDrawSlice> DrawSlices;
        BeShader* Shader;
    };

public:
    explicit BeRenderer(HWND windowHandle, uint32_t width, uint32_t height);
    ~BeRenderer() = default;

public:
    glm::vec3 ClearColor;
    
    UniformData UniformData;
    DirectionalLightData DirectionalLightData;
    std::vector<PointLightData> PointLights;

private:

    // window
    HWND _windowHandle;
    uint32_t _width;
    uint32_t _height;

    // dx11 core components
    ComPtr<ID3D11Device> _device;
    ComPtr<ID3D11DeviceContext> _context;
    ComPtr<IDXGIDevice> _dxgiDevice;
    ComPtr<IDXGIAdapter> _adapter;
    ComPtr<IDXGIFactory2> _factory;
    ComPtr<IDXGISwapChain1> _swapchain;

    // backbuffer
    ComPtr<ID3D11RenderTargetView> _backbufferTarget;

    // scene objects
    std::vector<ObjectEntry> _objects;

    // render graph system
    std::unique_ptr<BeRenderGraph> _renderGraph;
    BeGeometryPass* _geometryPass = nullptr;
    BeDirectionalLightPass* _directionalLightPass = nullptr;
    BePointLightPass* _pointLightPass = nullptr;
    BeCompositionPass* _compositionPass = nullptr;

public:
    [[nodiscard]] auto GetDevice() const -> ComPtr<ID3D11Device> { return _device; }

    auto LaunchDevice () -> void;
    auto PushObjects (const std::vector<ObjectEntry>& objects) -> void;
    auto Render() -> void;

private:
    void TerminateRenderer();
};
