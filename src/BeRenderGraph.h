#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <d3d11.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

class BeRenderPass;
class BeRenderResource;

//!Orchestrates render pass execution and resource management
class BeRenderGraph {
public:
    //fields///////////////////////////////////////////////////////////////////////////////////////

    bool DebugVisualizationEnabled = false;

    //initialisation///////////////////////////////////////////////////////////////////////////////////////

    BeRenderGraph(
        const ComPtr<ID3D11Device>& device,
        uint32_t width,
        uint32_t height
    );

    ~BeRenderGraph();

    //public interface///////////////////////////////////////////////////////////////////////////////////////

    //!< Builder: Add a new render pass of type T
    template<typename T>
    auto AddPass() -> T* {
        auto pass = std::make_unique<T>();
        T* passPtr = pass.get();
        _passes.push_back(std::move(pass));
        return passPtr;
    }

    //!< Get or create a render resource
    auto GetOrCreateResource(
        const std::string& name,
        const class BeRenderResource::BeResourceDesc& desc
    ) -> BeRenderResource*;

    //!< Get an existing resource by name
    auto GetResource(const std::string& name) const -> BeRenderResource*;

    //!< Build the render graph (validate dependencies, apply aliasing)
    auto Build() -> void;

    //!< Execute all passes in order
    auto Execute(const ComPtr<ID3D11DeviceContext>& context) -> void;

    //!< Get device for creating resources
    auto GetDevice() const -> const ComPtr<ID3D11Device>& { return _device; }

    //!< Get screen dimensions
    auto GetWidth() const -> uint32_t { return _width; }
    auto GetHeight() const -> uint32_t { return _height; }

    //!< Get all passes (for debugging)
    auto GetPasses() const -> const std::vector<std::unique_ptr<BeRenderPass>>& { return _passes; }

    //!< Get all resources (for debugging)
    auto GetResources() const -> const std::unordered_map<std::string, std::unique_ptr<BeRenderResource>>& {
        return _resources;
    }

    //private logic///////////////////////////////////////////////////////////////////////////////////////

private:
    auto validateGraph() -> void;
    auto applyResourceAliasing() -> void;

    ComPtr<ID3D11Device> _device;
    uint32_t _width, _height;

    std::vector<std::unique_ptr<BeRenderPass>> _passes;
    std::unordered_map<std::string, std::unique_ptr<BeRenderResource>> _resources;

    bool _isBuilt = false;
};