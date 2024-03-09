#pragma once
#include "Float4.h"
#include "Float3.h"
#include "Float2.h"
#include "TextureManager.h"
#include <vector>
#include <string>
#include <d3d12.h>

namespace Cygnus {

	struct VertexData {
		Cygnus::Float4 position;
		Cygnus::Float2 texcoord;
	};

	struct MaterialData {
		std::string textureFilePath;
		uint32_t textureHandle;
	};

	struct ModelData {
		std::vector<VertexData> vertices;
		Cygnus::MaterialData material;
	};

	class ModelManager
	{
	public:
		static ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename, ID3D12Device* device);

		static MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename, ID3D12Device* device);
	};
}

