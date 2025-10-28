#include "BeGeometryPass.h"
#include "BeRenderGraph.h"
#include "BeRenderResource.h"
#include "BeShader.h"
#include "BeMaterial.h"
#include "BeBuffers.h"
#include "Utils.h"
#include <glm.hpp>
#include <gtc/type_ptr.hpp>

auto BeGeometryPass::Setup(
    const ComPtr<ID3D11Device>& device,
    BeRenderGraph* graph
) -> void {
    _device = device;
    _graph = graph;

    //!< Create uniform buffer
    D3D11_BUFFER_DESC uniformBufferDescriptor = {};
    uniformBufferDescriptor.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    uniformBufferDescriptor.Usage = D3D11_USAGE_DYNAMIC;
    uniformBufferDescriptor.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    uniformBufferDescriptor.ByteWidth = sizeof(UniformBufferGPU);
    Utils::Check << device->CreateBuffer(&uniformBufferDescriptor, nullptr, &_uniformBuffer);

    //!< Create material buffer
    D3D11_BUFFER_DESC materialBufferDescriptor = {};
    materialBufferDescriptor.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    materialBufferDescriptor.Usage = D3D11_USAGE_DYNAMIC;
    materialBufferDescriptor.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    materialBufferDescriptor.ByteWidth = sizeof(MaterialBufferGPU);
    Utils::Check << device->CreateBuffer(&materialBufferDescriptor, nullptr, &_materialBuffer);

    //!< Create depth/stencil state
    D3D11_DEPTH_STENCIL_DESC depthStencilStateDescriptor = {
        .DepthEnable = true,
        .DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL,
        .DepthFunc = D3D11_COMPARISON_LESS,
        .StencilEnable = false,
    };
    Utils::Check << device->CreateDepthStencilState(&depthStencilStateDescriptor, &_depthStencilState);

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

    //!< Create white fallback texture
    _whiteFallbackTexture = new BeTexture(glm::vec4(1.0f));
    _whiteFallbackTexture->CreateSRV(device);

    //!< Create geometry buffers
    createGeometry();
}

auto BeGeometryPass::createGeometry() -> void {
    if (_objects.empty()) {
        return;
    }

    size_t totalVerticesNumber = 0;
    size_t totalIndicesNumber = 0;
    for (const auto& object : _objects) {
        totalVerticesNumber += object.Model->FullVertices.size();
        totalIndicesNumber += object.Model->Indices.size();
    }

    std::vector<BeFullVertex> fullVertices;
    std::vector<uint32_t> indices;
    fullVertices.reserve(totalVerticesNumber);
    indices.reserve(totalIndicesNumber);
    for (auto& object : _objects) {
        fullVertices.insert(fullVertices.end(), object.Model->FullVertices.begin(), object.Model->FullVertices.end());
        indices.insert(indices.end(), object.Model->Indices.begin(), object.Model->Indices.end());
        for (BeModel::BeDrawSlice slice : object.Model->DrawSlices) {
            slice.BaseVertexLocation += static_cast<int32_t>(fullVertices.size() - object.Model->FullVertices.size());
            slice.StartIndexLocation += static_cast<uint32_t>(indices.size() - object.Model->Indices.size());
            object.DrawSlices.push_back(slice);
        }
    }

    //!< Create vertex buffer
    D3D11_BUFFER_DESC vertexBufferDescriptor = {};
    vertexBufferDescriptor.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vertexBufferDescriptor.Usage = D3D11_USAGE_DEFAULT;
    vertexBufferDescriptor.ByteWidth = static_cast<UINT>(fullVertices.size() * sizeof(BeFullVertex));
    D3D11_SUBRESOURCE_DATA vertexData = {};
    vertexData.pSysMem = fullVertices.data();
    Utils::Check << _device->CreateBuffer(&vertexBufferDescriptor, &vertexData, &_sharedVertexBuffer);

    //!< Create index buffer
    D3D11_BUFFER_DESC indexBufferDescriptor = {};
    indexBufferDescriptor.BindFlags = D3D11_BIND_INDEX_BUFFER;
    indexBufferDescriptor.Usage = D3D11_USAGE_DEFAULT;
    indexBufferDescriptor.ByteWidth = static_cast<UINT>(indices.size() * sizeof(uint32_t));
    D3D11_SUBRESOURCE_DATA indexData = {};
    indexData.pSysMem = indices.data();
    Utils::Check << _device->CreateBuffer(&indexBufferDescriptor, &indexData, &_sharedIndexBuffer);
}

auto BeGeometryPass::Execute(const ComPtr<ID3D11DeviceContext>& context) -> void {
    if (!_graph || _objects.empty()) {
        return;
    }

    //!< Get G-Buffer resources from graph
    auto gbuffer0 = _graph->GetResource("GBuffer0");
    auto gbuffer1 = _graph->GetResource("GBuffer1");
    auto gbuffer2 = _graph->GetResource("GBuffer2");
    auto depthBuffer = _graph->GetResource("DepthBuffer");

    if (!gbuffer0 || !gbuffer1 || !gbuffer2 || !depthBuffer) {
        return; //!< Resources not available
    }

    //!< Set render targets
    ID3D11RenderTargetView* rtvs[3] = {
        gbuffer0->RTV.Get(),
        gbuffer1->RTV.Get(),
        gbuffer2->RTV.Get()
    };
    context->OMSetRenderTargets(3, rtvs, depthBuffer->DSV.Get());

    //!< Clear render targets
    for (const auto& rtv : rtvs) {
        context->ClearRenderTargetView(rtv, glm::value_ptr(glm::vec4(0.0f)));
    }
    context->ClearDepthStencilView(depthBuffer->DSV.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    //!< Set depth/stencil state
    context->OMSetDepthStencilState(_depthStencilState.Get(), 1);

    //!< Update and bind uniform buffer
    {
        UniformBufferGPU uniformDataGpu(_uniformData);
        D3D11_MAPPED_SUBRESOURCE uniformMappedResource;
        Utils::Check << context->Map(_uniformBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &uniformMappedResource);
        memcpy(uniformMappedResource.pData, &uniformDataGpu, sizeof(UniformBufferGPU));
        context->Unmap(_uniformBuffer.Get(), 0);
        context->VSSetConstantBuffers(0, 1, _uniformBuffer.GetAddressOf());
        context->PSSetConstantBuffers(0, 1, _uniformBuffer.GetAddressOf());
    }

    //!< Set vertex and index buffers
    uint32_t stride = sizeof(BeFullVertex);
    uint32_t offset = 0;
    context->IASetVertexBuffers(0, 1, _sharedVertexBuffer.GetAddressOf(), &stride, &offset);
    context->IASetIndexBuffer(_sharedIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    //!< Set default sampler
    context->PSSetSamplers(0, 1, _pointSampler.GetAddressOf());

    //!< Draw all objects
    for (const auto& object : _objects) {
        object.Shader->Bind(context.Get());

        for (const auto& slice : object.DrawSlices) {
            glm::mat4x4 modelMatrix =
                glm::translate(glm::mat4(1.0f), object.Position) *
                glm::mat4_cast(object.Rotation) *
                glm::scale(glm::mat4(1.0f), object.Scale);
            MaterialBufferGPU materialData(modelMatrix, slice.Material);
            D3D11_MAPPED_SUBRESOURCE materialMappedResource;
            Utils::Check << context->Map(_materialBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &materialMappedResource);
            memcpy(materialMappedResource.pData, &materialData, sizeof(MaterialBufferGPU));
            context->Unmap(_materialBuffer.Get(), 0);
            context->VSSetConstantBuffers(1, 1, _materialBuffer.GetAddressOf());
            context->PSSetConstantBuffers(1, 1, _materialBuffer.GetAddressOf());

            ID3D11ShaderResourceView* materialResources[2] = {
                slice.Material.DiffuseTexture ? slice.Material.DiffuseTexture->SRV.Get() : _whiteFallbackTexture->SRV.Get(),
                slice.Material.SpecularTexture ? slice.Material.SpecularTexture->SRV.Get() : _whiteFallbackTexture->SRV.Get(),
            };
            context->PSSetShaderResources(0, 2, materialResources);

            context->DrawIndexed(slice.IndexCount, slice.StartIndexLocation, slice.BaseVertexLocation);

            ID3D11ShaderResourceView* emptyResources[2] = { nullptr, nullptr };
            context->PSSetShaderResources(0, 2, emptyResources);
        }
    }

    //!< Cleanup
    {
        ID3D11ShaderResourceView* emptyResources[2] = { nullptr, nullptr };
        context->PSSetShaderResources(0, 2, emptyResources);
        ID3D11Buffer* emptyBuffers[1] = { nullptr };
        context->VSSetConstantBuffers(1, 1, emptyBuffers);
        ID3D11SamplerState* emptySamplers[1] = { nullptr };
        context->PSSetSamplers(0, 1, emptySamplers);
        ID3D11RenderTargetView* emptyTargets[3] = { nullptr, nullptr, nullptr };
        context->OMSetRenderTargets(3, emptyTargets, nullptr);
    }
}

auto BeGeometryPass::GetOutputResources() const -> std::vector<std::string> {
    return {"GBuffer0", "GBuffer1", "GBuffer2", "DepthBuffer"};
}