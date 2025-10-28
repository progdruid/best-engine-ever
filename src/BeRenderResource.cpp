#include "BeRenderResource.h"
#include "Utils.h"

auto BeRenderResource::CreateGPUResources(const ComPtr<ID3D11Device>& device) -> void {
    if (Descriptor.Type != BeResourceType::Texture2D) {
        return; //!< Buffer resources are typically created separately
    }

    D3D11_TEXTURE2D_DESC textureDesc = {};
    textureDesc.Width = Descriptor.Width;
    textureDesc.Height = Descriptor.Height;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = Descriptor.Format;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = Descriptor.BindFlags;
    textureDesc.CPUAccessFlags = 0;
    textureDesc.MiscFlags = 0;

    //!< Create texture
    Utils::Check << device->CreateTexture2D(&textureDesc, nullptr, Texture.GetAddressOf());

    //!< Create render target view if needed
    if (Descriptor.BindFlags & D3D11_BIND_RENDER_TARGET) {
        Utils::Check << device->CreateRenderTargetView(Texture.Get(), nullptr, RTV.GetAddressOf());
    }

    //!< Create shader resource view if needed
    if (Descriptor.BindFlags & D3D11_BIND_SHADER_RESOURCE) {
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = Descriptor.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = 1;
        Utils::Check << device->CreateShaderResourceView(Texture.Get(), &srvDesc, SRV.GetAddressOf());
    }

    //!< Create depth stencil view if needed
    if (Descriptor.BindFlags & D3D11_BIND_DEPTH_STENCIL) {
        D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = Descriptor.Format;
        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Texture2D.MipSlice = 0;
        Utils::Check << device->CreateDepthStencilView(Texture.Get(), &dsvDesc, DSV.GetAddressOf());
    }
}

auto BeRenderResource::Release() -> void {
    RTV.Reset();
    SRV.Reset();
    DSV.Reset();
    Texture.Reset();
    Buffer.Reset();
}

auto BeRenderResource::RegisterConsumer(BeRenderPass* pass) -> void {
    if (!pass) return;
    ConsumerPasses.push_back(pass);
}

auto BeRenderResource::GetMemorySize() const -> size_t {
    if (Descriptor.Type == BeResourceType::Texture2D) {
        //!< Rough estimate: assumes RGBA or similar
        size_t bytesPerPixel = 4; //!< Most common formats
        if (Descriptor.Format == DXGI_FORMAT_R11G11B10_FLOAT ||
            Descriptor.Format == DXGI_FORMAT_R16G16B16A16_FLOAT) {
            bytesPerPixel = 8;
        }
        return Descriptor.Width * Descriptor.Height * bytesPerPixel;
    }
    return 0;
}