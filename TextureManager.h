#pragma once
#include <d3d12.h>
#include "externals/DirectXTex/DirectXTex.h"
#include "StringUtil.h"

namespace Cygnus {
	class TextureManager final
	{
	public:
		static int Load(const std::string& filePath, ID3D12Device* device, ID3D12DescriptorHeap* srvHeap);

		static TextureManager& GetInstance();

		static void SetDescriptorTable(UINT rootParamIndex, ID3D12GraphicsCommandList* commandList);

	private:
		DirectX::ScratchImage LoadTexture(const std::string& filePath);

		ID3D12Resource* CreateTextureResource(ID3D12Device* device, const DirectX::TexMetadata& metadata);

		void UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages);

		D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU;
		D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU;
	};
}

