#pragma once

#include <string>
#include <vector>
#include <memory>
#include <d3d11.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

class BeRenderResource;
class BeRenderGraph;

//!Abstract base class for all render passes
class BeRenderPass {
public:
    virtual ~BeRenderPass() = default;

    //!< Returns human-readable name for debugging
    virtual auto GetName() const -> const std::string& = 0;

    //!< Called once during graph setup to create pass-specific resources
    virtual auto Setup(
        const ComPtr<ID3D11Device>& device,
        BeRenderGraph* graph
    ) -> void = 0;

    //!< Called each frame to execute the pass
    virtual auto Execute(const ComPtr<ID3D11DeviceContext>& context) -> void = 0;

    //!< Returns input resources this pass reads from
    virtual auto GetInputResources() const -> std::vector<std::string> { return {}; }

    //!< Returns output resources this pass writes to
    virtual auto GetOutputResources() const -> std::vector<std::string> { return {}; }

protected:
    std::string _name = "Unnamed";
};