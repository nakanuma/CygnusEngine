#include "DescriptorHeap.h"
#include <cassert>


void Cygnus::DescriptorHeap::Create(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible)
{
	heap = nullptr;
	type = heapType;
	size = device->GetDescriptorHandleIncrementSize(type);
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
	descriptorHeapDesc.Type = heapType;
	descriptorHeapDesc.NumDescriptors = numDescriptors;
	descriptorHeapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	HRESULT hr = device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&heap));
	// ディスクリプタヒープが作れなかったので悲しいが起動できない
	assert(SUCCEEDED(hr));
}

D3D12_CPU_DESCRIPTOR_HANDLE Cygnus::DescriptorHeap::GetCPUHandle(uint32_t index)
{
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = heap->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += (size * index);
	return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE Cygnus::DescriptorHeap::GetGPUHandle(uint32_t index)
{
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = heap->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += (size * index);
	return handleGPU;
}
