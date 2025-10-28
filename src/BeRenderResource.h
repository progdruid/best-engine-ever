#pragma once

#include <string>
#include <memory>
#include <d3d11.h>
#include <wrl/client.h>
#include <glm.hpp>

using Microsoft::WRL::ComPtr;

//!Describes a GPU resource (texture or buffer) used in the render graph
class BeRenderResource {
public:
    enum class BeResourceType {
        Texture2D,
        Buffer,
    };

    enum class BeResourceUsage {
        RenderTarget,      //!< Used as render target output
        ShaderResource,    //!< Used as shader input (texture read)
        DepthStencil,      //!< Used as depth/stencil buffer
        ConstantBuffer,    //!< Used as constant buffer
    };

    struct BeResourceDesc {
        BeResourceType Type = BeResourceType::Texture2D;
        DXGI_FORMAT Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        uint32_t Width = 0;
        uint32_t Height = 0;
        uint32_t BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        bool AllowAliasing = true; //!< Can this resource be aliased with others?
    };

    //fields///////////////////////////////////////////////////////////////////////////////////////

    std::string Name;
    BeResourceDesc Descriptor;

    //!< GPU-side resources
    ComPtr<ID3D11Texture2D> Texture;
    ComPtr<ID3D11RenderTargetView> RTV;
    ComPtr<ID3D11ShaderResourceView> SRV;
    ComPtr<ID3D11DepthStencilView> DSV;

    //!< For constant/index/vertex buffers
    ComPtr<ID3D11Buffer> Buffer;

    //!< Tracks which pass produced this resource and which consume it
    class BeRenderPass* ProducerPass = nullptr;
    std::vector<class BeRenderPass*> ConsumerPasses;

    //initialisation///////////////////////////////////////////////////////////////////////////////////////

    BeRenderResource(const std::string& inName, const BeResourceDesc& inDesc)
        : Name(inName), Descriptor(inDesc) {
    }

    //public interface///////////////////////////////////////////////////////////////////////////////////////

    //!< Create GPU resources based on descriptor
    auto CreateGPUResources(const ComPtr<ID3D11Device>& device) -> void;

    //!< Reset GPU resources
    auto Release() -> void;

    //!< Register a consumer pass for this resource
    auto RegisterConsumer(BeRenderPass* pass) -> void;

    //!< Check if this resource can be aliased (shared memory with another)
    auto CanAlias() const -> bool { return Descriptor.AllowAliasing; }

    //!< Get memory size in bytes (for aliasing calculations)
    auto GetMemorySize() const -> size_t;
};