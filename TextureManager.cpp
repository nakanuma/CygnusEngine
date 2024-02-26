#include "TextureManager.h"

DirectX::ScratchImage Cygnus::TextureManager::LoadTexture(const std::string& filePath)
{
    // テクスチャファイルを読んでプログラムで扱えるようにする
    DirectX::ScratchImage image{};
    std::wstring filePathW = ConvertString(filePath);
    HRESULT hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_LINEAR, nullptr, image);
    assert(SUCCEEDED(hr));

    // ミップマップの作成
    DirectX::ScratchImage mipImages{};
    hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);
    assert(SUCCEEDED(hr));

    // ミップマップ付きのデータを返す
    return mipImages;
}

ID3D12Resource* Cygnus::TextureManager::CreateTextureResource(ID3D12Device* device, const DirectX::TexMetadata& metadata)
{
    // metadataを基にResourceの設定
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Width = UINT(metadata.width); // Textureの幅
    resourceDesc.Height = UINT(metadata.height); // Textureの高さ
    resourceDesc.MipLevels = UINT16(metadata.mipLevels); // mipmapの数
    resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize); // 奥行き or 配列Textureの配列数
    resourceDesc.Format = metadata.format; // TextureのFormat
    resourceDesc.SampleDesc.Count = 1; // サンプリングカウント。1固定
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension); // Textureの次元数

    // 利用するHeapの設定
    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_CUSTOM; // 細かい設定を行う
    heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK; // WriteBackポリシーでCPUアクセス可能
    heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0; // プロセッサの近くに配置

    // Resourceを生成する
    ID3D12Resource* resource = nullptr;
    HRESULT hr = device->CreateCommittedResource(
        &heapProperties, // Heapの設定
        D3D12_HEAP_FLAG_NONE, // Heapの特殊な設定
        &resourceDesc, // Resourceの設定
        D3D12_RESOURCE_STATE_GENERIC_READ, // 初回のResourceState
        nullptr, // Clear最適値
        IID_PPV_ARGS(&resource)); // 作成するResourceポインタへのポインタ
    assert(SUCCEEDED(hr));

    return resource;
}

void Cygnus::TextureManager::UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages)
{
    // Meta情報を取得
    const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
    // 全MipMapについて
    for (size_t mipLevel = 0; mipLevel < metadata.mipLevels; ++mipLevel) {
        // MipMapLevelを指定して各Imageを取得
        const DirectX::Image* img = mipImages.GetImage(mipLevel, 0, 0);
        // Textureに転送
        HRESULT hr = texture->WriteToSubresource(
            UINT(mipLevel),
            nullptr, // 全領域へコピー
            img->pixels, // 元データアドレス
            UINT(img->rowPitch), // 1ラインサイズ
            UINT(img->slicePitch) // 1枚サイズ
        );
        assert(SUCCEEDED(hr));
    }
}

int Cygnus::TextureManager::Load(const std::string& filePath, ID3D12Device* device, ID3D12DescriptorHeap* srvHeap)
{
    DirectX::ScratchImage mipImages = GetInstance().LoadTexture(filePath);
    const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
    ID3D12Resource* textureResource = Cygnus::TextureManager::GetInstance().CreateTextureResource(device, metadata);
    Cygnus::TextureManager::GetInstance().UploadTextureData(textureResource, mipImages);

    // metaDataを基にSRVの設定
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = metadata.format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
    srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

    // SRVを作成するDescriptorHeapの場所を決める
    GetInstance().textureSrvHandleCPU = srvHeap->GetCPUDescriptorHandleForHeapStart();
    GetInstance().textureSrvHandleGPU = srvHeap->GetGPUDescriptorHandleForHeapStart();
    // 先頭はImGuiが使っているのでその次を使う
    GetInstance().textureSrvHandleCPU.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    GetInstance().textureSrvHandleGPU.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    // SRVの生成
    device->CreateShaderResourceView(textureResource, &srvDesc, GetInstance().textureSrvHandleCPU);

    return 0;
}

Cygnus::TextureManager& Cygnus::TextureManager::GetInstance()
{
    static TextureManager ins;

    return ins;
}

void Cygnus::TextureManager::SetDescriptorTable(UINT rootParamIndex, ID3D12GraphicsCommandList* commandList)
{
    commandList->SetGraphicsRootDescriptorTable(rootParamIndex, GetInstance().textureSrvHandleGPU);
}
