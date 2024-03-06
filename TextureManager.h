#pragma once
#include <d3d12.h>
#include "externals/DirectXTex/DirectXTex.h"
#include "StringUtil.h"
#include "DescriptorHeap.h"

namespace Cygnus {
	class TextureManager final
	{
	public:
		Cygnus::DescriptorHeap srvHeap;

		static void Initialize(ID3D12Device* device);

		static int Load(const std::string& filePath, ID3D12Device* device);

		static TextureManager& GetInstance();

		static void SetDescriptorTable(UINT rootParamIndex, ID3D12GraphicsCommandList* commandList, uint32_t textureHandle);

	private:
		DirectX::ScratchImage LoadTexture(const std::string& filePath);

		ID3D12Resource* CreateTextureResource(ID3D12Device* device, const DirectX::TexMetadata& metadata);

		void UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages);

		uint32_t index = 1;
	};
}

