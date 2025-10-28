#include "BeRenderGraph.h"
#include "BeRenderPass.h"
#include "BeRenderResource.h"
#include <iostream>

BeRenderGraph::BeRenderGraph(
    const ComPtr<ID3D11Device>& device,
    uint32_t width,
    uint32_t height
)
    : _device(device), _width(width), _height(height) {
}

BeRenderGraph::~BeRenderGraph() {
    for (auto& resource : _resources) {
        resource.second->Release();
    }
    _resources.clear();
    _passes.clear();
}

auto BeRenderGraph::GetOrCreateResource(
    const std::string& name,
    const BeRenderResource::BeResourceDesc& desc
) -> BeRenderResource* {
    auto it = _resources.find(name);
    if (it != _resources.end()) {
        return it->second.get();
    }

    auto resource = std::make_unique<BeRenderResource>(name, desc);
    BeRenderResource* resourcePtr = resource.get();
    _resources[name] = std::move(resource);
    return resourcePtr;
}

auto BeRenderGraph::GetResource(const std::string& name) const -> BeRenderResource* {
    auto it = _resources.find(name);
    if (it != _resources.end()) {
        return it->second.get();
    }
    return nullptr;
}

auto BeRenderGraph::Build() -> void {
    if (_isBuilt) {
        return; //!< Already built
    }

    //!< Setup all passes
    for (auto& pass : _passes) {
        pass->Setup(_device, this);
    }

    //!< Create GPU resources for all declared resources
    for (auto& resource : _resources) {
        resource.second->CreateGPUResources(_device);
    }

    //!< Register resource dependencies
    for (size_t passIdx = 0; passIdx < _passes.size(); ++passIdx) {
        auto& pass = _passes[passIdx];

        //!< Inputs (consumers)
        auto inputs = pass->GetInputResources();
        for (const auto& inputName : inputs) {
            auto resource = GetResource(inputName);
            if (resource) {
                resource->RegisterConsumer(pass.get());
            }
        }

        //!< Outputs (producers)
        auto outputs = pass->GetOutputResources();
        for (const auto& outputName : outputs) {
            auto resource = GetResource(outputName);
            if (resource) {
                resource->ProducerPass = pass.get();
            }
        }
    }

    validateGraph();
    applyResourceAliasing();

    _isBuilt = true;
}

auto BeRenderGraph::Execute(const ComPtr<ID3D11DeviceContext>& context) -> void {
    if (!_isBuilt) {
        Build();
    }

    for (auto& pass : _passes) {
        if (DebugVisualizationEnabled) {
            std::string passName = pass->GetName();
            std::cout << "Executing pass: " << passName << std::endl;
        }
        pass->Execute(context);
    }
}

auto BeRenderGraph::validateGraph() -> void {
    //!< Simple validation: ensure all input resources exist
    for (const auto& pass : _passes) {
        auto inputs = pass->GetInputResources();
        for (const auto& inputName : inputs) {
            if (!GetResource(inputName)) {
                std::cerr << "Warning: Pass '" << pass->GetName()
                         << "' requires input '" << inputName << "' which doesn't exist" << std::endl;
            }
        }
    }
}

auto BeRenderGraph::applyResourceAliasing() -> void {
    //!< For now, no aliasing logic (straightforward sequential execution)
    //!< In future: could alias resources that don't overlap in time
}