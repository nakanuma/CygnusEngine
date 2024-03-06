#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <stdint.h>

namespace Cygnus {
	class DescriptorHeap
	{
	public:
		Microsoft::WRL::ComPtr <ID3D12DescriptorHeap> heap;

		void Create(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);

		D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(uint32_t index);
		D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(uint32_t index);

	private:
		D3D12_DESCRIPTOR_HEAP_TYPE type;
		uint32_t size;
	};
}

