#include "MyWindow.h"
#include "StringUtil.h"
#include "TextureManager.h"
#include "Logger.h"
#include "Transform.h"
#include "Camera.h"
#include "ImguiWrapper.h"
#include "CygnusMath.h"
#include "DescriptorHeap.h"
#include "ModelManager.h"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cassert>
#include <wrl.h>
#include <dxcapi.h>
#include <time.h>
#include <mmsystem.h>

struct Material {
	Cygnus::Float4 color;
	int32_t enableLighting;
};

struct TransformationMatrix {
	Cygnus::Matrix WVP;
	Cygnus::Matrix World;
};

struct DirectionalLight {
	Cygnus::Float4 color; // ライトの色
	Cygnus::Float3 direction; // ライトの向き
	float intensity; // 輝度
};

IDxcBlob* CompileShader(
	// CompilerするShaderファイルへのパス
	const std::wstring& filePath,
	// Compilerに使用するProfile
	const wchar_t* profile,
	// 初期化で生成したものを3つ
	IDxcUtils* dxcUtils,
	IDxcCompiler3* dxcCompiler,
	IDxcIncludeHandler* includeHandler)
{
	// これからシェーダーをコンパイルする旨をログに出す
	Cygnus::Log(std::format(L"Begin CompileShader, path:{}, profile:{}\n", filePath, profile));
	// hlslファイルを読む
	IDxcBlobEncoding* shaderSource = nullptr;
	HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);
	// 読めなかったら止める
	assert(SUCCEEDED(hr));
	// 読み込んだファイルの内容を設定する
	DxcBuffer shaderSourceBuffer;
	shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
	shaderSourceBuffer.Size = shaderSource->GetBufferSize();
	shaderSourceBuffer.Encoding = DXC_CP_UTF8; // UTF8の文字コードであることを通知

	LPCWSTR arguments[] = {
		filePath.c_str(), // コンパイル対象のhlslファイル名
		L"-E", L"main", // エントリーポイントの指定。基本的にmain以外にはしない
		L"-T", profile, // ShaderProfileの設定
		L"-Zi", L"-Qembed_debug", // デバッグ用の情報を埋め込む
		L"-Od", // 最適化を外しておく
		L"-Zpr", // メモリレイアウトは行優先
	};
	// 実際にShaderをコンパイルする
	IDxcResult* shaderResult = nullptr;
	hr = dxcCompiler->Compile(
		&shaderSourceBuffer, // 読み込んだファイル
		arguments, // コンパイルオプション
		_countof(arguments), // コンパイルオプションの数
		includeHandler, // includeが含まれた諸々
		IID_PPV_ARGS(&shaderResult) // コンパイル結果
	);
	//コンパイルエラーではなくdxcが起動できないなど致命的な状況
	assert(SUCCEEDED(hr));

	// 警告・エラーが出てたらログに出して止める
	IDxcBlobUtf8* shaderError = nullptr;
	shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
	if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
		Cygnus::Log(shaderError->GetStringPointer());
		assert(false);
	}

	// コンパイル結果から実行用のバイナリ部分を取得
	IDxcBlob* shaderBlob = nullptr;
	hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
	assert(SUCCEEDED(hr));
	// 成功したログを出す
	Cygnus::Log(std::format(L"Compile Succeeded, path:{}, profile:{}\n", filePath, profile));
	// もう使わないリソースを解放
	shaderSource->Release();
	shaderResult->Release();
	// 実行用のバイナリを返却
	return shaderBlob;
}

Microsoft::WRL::ComPtr < ID3D12Resource> CreateBufferResource(ID3D12Device* device, size_t sizeInBytes) {
	// 頂点リソース用のヒープの設定
	D3D12_HEAP_PROPERTIES uploadHeapProperties{};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD; // UploadHeapを使う
	// 頂点リソースの設定
	D3D12_RESOURCE_DESC vertexResourcesDesc{};
	// バッファリソース。テクスチャの場合はまた別の設定をする
	vertexResourcesDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	vertexResourcesDesc.Width = sizeInBytes; // リソースのサイズ
	// バッファの場合はこれらは1にする決まり
	vertexResourcesDesc.Height = 1;
	vertexResourcesDesc.DepthOrArraySize = 1;
	vertexResourcesDesc.MipLevels = 1;
	vertexResourcesDesc.SampleDesc.Count = 1;
	// バッファの場合はこれにする決まり
	vertexResourcesDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	// 実際に頂点リソースを作る
	Microsoft::WRL::ComPtr < ID3D12Resource> vertexResource = nullptr;
	HRESULT hr = device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE,
		&vertexResourcesDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&vertexResource));
	assert(SUCCEEDED(hr));

	return std::move(vertexResource); // デストラクタを呼ばないように返す
}

Microsoft::WRL::ComPtr < ID3D12Resource> CreateDepthStencilTextureResource(ID3D12Device* device, int32_t width, int32_t height) {
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = width; // Textureの幅
	resourceDesc.Height = height; // Textureの高さ
	resourceDesc.MipLevels = 1; // mipmapの数
	resourceDesc.DepthOrArraySize = 1; // 奥行き or 配列Textureの配列数
	resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // DepthStencilとして利用可能なフォーマット
	resourceDesc.SampleDesc.Count = 1; // サンプリングカウント。1固定
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; // 2次元
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; /// DepthStencilとして使う通知

	// 利用するHeapの設定
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // VRAM上に作る

	// 深度値のクリア設定
	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.0f; // 1.0f(最大値)でクリア
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // フォーマット。Resourceと合わせる

	// Resourceの設定
	Microsoft::WRL::ComPtr <ID3D12Resource> resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties, // Heapの設定
		D3D12_HEAP_FLAG_NONE, // Heapの特殊な設定。特になし
		&resourceDesc, // Resourceの設定
		D3D12_RESOURCE_STATE_DEPTH_WRITE, // 深度値を書き込む状態にしておく
		&depthClearValue, // Clear最適値
		IID_PPV_ARGS(&resource)); // 作成するResourceポインタへのポインタ
	assert(SUCCEEDED(hr));

	return std::move(resource);
}

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {

	CoInitializeEx(0, COINIT_MULTITHREADED);

	Cygnus::Window::Create(L"DirectX", 1280, 720);

#ifdef _DEBUG
	Microsoft::WRL::ComPtr <ID3D12Debug1> debugController = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
		// デバッグレイヤーを有効化する
		debugController->EnableDebugLayer();
		// さらにGPU側でもチェックを行うようにする
		debugController->SetEnableGPUBasedValidation(TRUE);
	}
#endif

	// DXGIファクトリーの生成
	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory = nullptr;

	HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory));

	assert(SUCCEEDED(hr));

	// 使用するアダプタ用の変数。最初にnullptrを入れておく
	Microsoft::WRL::ComPtr <IDXGIAdapter4> useAdapter = nullptr;

	// 良い順にアダプタを頼む
	for (UINT i = 0; dxgiFactory->EnumAdapterByGpuPreference(i,
		DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter)) !=
		DXGI_ERROR_NOT_FOUND; ++i) {
		// アダプターの情報を取得する
		DXGI_ADAPTER_DESC3 adapterDesc{};
		hr = useAdapter->GetDesc3(&adapterDesc);
		assert(SUCCEEDED(hr));
		// ソフトウェアアダプタでなければ採用
		if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
			// 採用したアダプタの情報をログに出力
			Cygnus::Log(std::format(L"Use Adfapter:{}\n", adapterDesc.Description));
			break;
		}
		useAdapter = nullptr; // ソフトウェアアダプタの場合は見なかったことにする
	}
	// 適切なアダプタが見つからなかったので起動できない
	assert(useAdapter != nullptr);


	Microsoft::WRL::ComPtr <ID3D12Device> device = nullptr;
	// 機能レベルとログ出力用の文字列
	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_12_2,D3D_FEATURE_LEVEL_12_1,D3D_FEATURE_LEVEL_12_0
	};
	const char* featureLevelStrings[] = { "12.2","12.1","12.0" };
	// 高い順に生成できるか試していく
	for (size_t i = 0; i < _countof(featureLevels); ++i) {
		// 採用したアダプターでデバイスを生成
		hr = D3D12CreateDevice(useAdapter.Get(), featureLevels[i], IID_PPV_ARGS(&device));
		// 指定した機能レベルでデバイスが生成できたかを確認
		if (SUCCEEDED(hr)) {
			// 生成できたのでログ出力を行ってループを抜ける
			Cygnus::Log(std::format("FeatureLevel : {}\n", featureLevelStrings[i]));
			break;
		}
	}
	// デバイスの生成がうまくいかなかったので起動できない
	assert(device != nullptr);
	Cygnus::Log("Complete create D3D12Device!!!\n"); // 初期化完了のログを出す

#ifdef _DEBUG
	Microsoft::WRL::ComPtr <ID3D12InfoQueue> infoQueue = nullptr;
	if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
		// ヤバイエラー時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
		// エラー時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
		// 警告時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);

		// 抑制するメッセージのID
		D3D12_MESSAGE_ID denyIds[] = {
			// Windows11でのDXGIデバッグレイヤーとDX12デバッグレイヤーの相互作用バグによるエラーメッセージ
			D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
		};
		// 抑制するレベル
		D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
		D3D12_INFO_QUEUE_FILTER filter{};
		filter.DenyList.NumIDs = _countof(denyIds);
		filter.DenyList.pIDList = denyIds;
		filter.DenyList.NumSeverities = _countof(severities);
		filter.DenyList.pSeverityList = severities;
		// 指定したメッセージの表示を抑制する
		infoQueue->PushStorageFilter(&filter);
	}
#endif

	// コマンドキューを生成する
	Microsoft::WRL::ComPtr <ID3D12CommandQueue> commandQueue = nullptr;
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
	hr = device->CreateCommandQueue(&commandQueueDesc,
		IID_PPV_ARGS(&commandQueue));
	// コマンドキューの生成がうまくいかなかったので起動できない
	assert(SUCCEEDED(hr));


	// コマンドアロケータを生成する
	Microsoft::WRL::ComPtr <ID3D12CommandAllocator> commandAllocator = nullptr;
	hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));
	// コマンドアロケータの生成がうまくいかなかったので起動できない
	assert(SUCCEEDED(hr));

	// コマンドリストを生成する
	Microsoft::WRL::ComPtr <ID3D12GraphicsCommandList> commandList = nullptr;
	hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr,
		IID_PPV_ARGS(&commandList));
	// コマンドリストの生成がうまくいかなかったので残念ながら起動できない
	assert(SUCCEEDED(hr));


	// スワップチェーンを生成する
	Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain = nullptr;
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	swapChainDesc.Width = Cygnus::Window::GetWidth(); // 画面の幅。ウィンドウのクライアント領域を同じものにしておく
	swapChainDesc.Height = Cygnus::Window::GetHeight(); // 画面の高さ。ウィンドウのクライアント領域を同じものにしておく
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // 色の形式
	swapChainDesc.SampleDesc.Count = 1; // マルチサンプルしない
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // 描画のターゲットとして利用する
	swapChainDesc.BufferCount = 2; // ダブルバッファ
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // モニタにうつしたら、中身を破棄
	// コマンドキュー、ウィンドウハンドル、設定を渡して生成する
	hr = dxgiFactory->CreateSwapChainForHwnd(commandQueue.Get(), Cygnus::Window::GetHandle(), &swapChainDesc, nullptr, nullptr,
		reinterpret_cast<IDXGISwapChain1**>(swapChain.GetAddressOf()));
	assert(SUCCEEDED(hr));


	// ディスクリプタヒープの生成
	Cygnus::DescriptorHeap rtvDescriptorHeap;
	rtvDescriptorHeap.Create(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);

	// SwapChainからResourceを引っ張ってくる
	Microsoft::WRL::ComPtr <ID3D12Resource> swapChainResources[2] = { nullptr };
	hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&swapChainResources[0]));
	// うまく取得できなければ遺憾だが起動できない
	assert(SUCCEEDED(hr));
	hr = swapChain->GetBuffer(1, IID_PPV_ARGS(&swapChainResources[1]));
	assert(SUCCEEDED(hr));


	// RTVの設定
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // 出力結果をSRGBに変換して書き込む
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D; // 2dテクスチャとして書き込む
	// ディスクリプタの先頭を取得する
	device->CreateRenderTargetView(swapChainResources[0].Get(), &rtvDesc, rtvDescriptorHeap.GetCPUHandle(0));
	// 2つ目を作る
	device->CreateRenderTargetView(swapChainResources[1].Get(), &rtvDesc, rtvDescriptorHeap.GetCPUHandle(1));

	// 初期値0でFenceを作る
	Microsoft::WRL::ComPtr <ID3D12Fence> fence = nullptr;
	uint64_t fenceValue = 0;
	hr = device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	assert(SUCCEEDED(hr));

	// FenceのSignalを待つためのイベントを作成する
	HANDLE fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(fenceEvent != nullptr);

	// dxcCompilerを初期化
	IDxcUtils* dxcUtils = nullptr;
	IDxcCompiler3* dxcCompiler = nullptr;
	hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
	assert(SUCCEEDED(hr));
	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
	assert(SUCCEEDED(hr));

	// 現時点でincludeはしないが、includeに対応するための設定を行っておく
	IDxcIncludeHandler* includeHandler = nullptr;
	hr = dxcUtils->CreateDefaultIncludeHandler(&includeHandler);
	assert(SUCCEEDED(hr));

	// RootSignature作成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// DescriptorRange
	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0; // 0から始まる
	descriptorRange[0].NumDescriptors = 1; // 数は1つ
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算

	// RootParameter作成
	D3D12_ROOT_PARAMETER rootParameters[4] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	rootParameters[0].Descriptor.ShaderRegister = 0; // レジスタ番号0とバインド
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // VertexShaderで使う
	rootParameters[1].Descriptor.ShaderRegister = 0; // レジスタ番号0とバインド
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // DescriptorTableを使う
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange; // Tableの中身の配列を指定
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange); // Tableで利用する数
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	rootParameters[3].Descriptor.ShaderRegister = 1; // レジスタ番号1を使う
	descriptionRootSignature.pParameters = rootParameters; // ルートパラメータ配列へのポインタ
	descriptionRootSignature.NumParameters = _countof(rootParameters); // 配列の長さ

	// Samplerの設定
	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; // バイリニアフィルタ
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // 0~1の範囲外をリピート
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; // 比較しない
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX; // ありったけのMipmapを使う
	staticSamplers[0].ShaderRegister = 0; // レジスタ番号0を使う
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

	// シリアライズしてバイナリにする
	Microsoft::WRL::ComPtr < ID3DBlob> signatureBlob = nullptr;
	Microsoft::WRL::ComPtr < ID3DBlob> errorBlob = nullptr;
	hr = D3D12SerializeRootSignature(&descriptionRootSignature,
		D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		Cygnus::Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}
	// バイナリを元に生成
	Microsoft::WRL::ComPtr < ID3D12RootSignature> rootSignature = nullptr;
	hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
	assert(SUCCEEDED(hr));

	// InputLayout
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	// BlendStateの設定
	D3D12_BLEND_DESC blendDesc{};
	// すべての色要素を書き込む
	blendDesc.RenderTarget[0].RenderTargetWriteMask =
		D3D12_COLOR_WRITE_ENABLE_ALL;

	// RasterizerStateの設定
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	// 裏面(時計回り)を表示しない
	/*rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;*/
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	// 三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	// Shaderをコンパイルする
	IDxcBlob* vertexShaderBlob = CompileShader(L"Object3D.VS.hlsl",
		L"vs_6_0", dxcUtils, dxcCompiler, includeHandler);
	assert(vertexShaderBlob != nullptr);

	IDxcBlob* pixelShaderBlob = CompileShader(L"Object3D.PS.hlsl",
		L"ps_6_0", dxcUtils, dxcCompiler, includeHandler);
	assert(pixelShaderBlob != nullptr);

	// DepthStencilTextureをウィンドウのサイズで作成
	Microsoft::WRL::ComPtr <ID3D12Resource> depthStencilResource = CreateDepthStencilTextureResource(device.Get(), Cygnus::Window::GetWidth(), Cygnus::Window::GetHeight());

	// DSV用のヒープでディスクリプタの数は1。DSVはShade内で触るものではないので、ShaderVisibleはfalse
	Cygnus::DescriptorHeap dsvDescriptorHeap;
	dsvDescriptorHeap.Create(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);

	// DSVの設定
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // Format。基本的にはResourceに合わせる
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D; // 2dTexture
	// DSVHeapの先頭にDSVをつくる
	device->CreateDepthStencilView(depthStencilResource.Get(), &dsvDesc, dsvDescriptorHeap.GetCPUHandle(0));

	// DepthStencilStateの設定
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	// Depthの機能を有効化する
	depthStencilDesc.DepthEnable = true;
	// 書き込みします
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	// 比較関数はLessEqual
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	// PSOを生成する
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = rootSignature.Get();
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(),
		vertexShaderBlob->GetBufferSize() };
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(),
		pixelShaderBlob->GetBufferSize() };
	graphicsPipelineStateDesc.BlendState = blendDesc;
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;
	// 書き込むRTVの情報
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	// 利用するトポロジ(形状)のタイプ。三角形
	graphicsPipelineStateDesc.PrimitiveTopologyType =
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	// どのように画面に色を打ち込むかの設定
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	// DepthStencilの設定
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	// 実際に生成
	Microsoft::WRL::ComPtr <ID3D12PipelineState> graphicsPipelineState = nullptr;
	hr = device->CreateGraphicsPipelineState(&graphicsPipelineStateDesc,
		IID_PPV_ARGS(&graphicsPipelineState));
	assert(SUCCEEDED(hr));

	// TextureManagerの初期化
	Cygnus::TextureManager::Initialize(device.Get());

	// ImGuiの初期化
	Cygnus::ImguiWrapper::Initialize(device.Get(), swapChainDesc.BufferCount, rtvDesc.Format, Cygnus::TextureManager::GetInstance().srvHeap.heap.Get());

	// ビューポート
	D3D12_VIEWPORT viewport{};
	// クライアント領域のサイズを一緒にして画面全体に表示
	viewport.Width = static_cast<float>(Cygnus::Window::GetWidth());
	viewport.Height = static_cast<float>(Cygnus::Window::GetHeight());
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	// シザー矩形
	D3D12_RECT scissorRect{};
	// 基本的にビューポートと同じ矩形が構成されるようにする
	scissorRect.left = 0;
	scissorRect.right = Cygnus::Window::GetWidth();
	scissorRect.top = 0;
	scissorRect.bottom = Cygnus::Window::GetHeight();

	///
	///	ここから3Dオブジェクト
	/// 

	// モデル読み込み
	Cygnus::ModelData modelData = Cygnus::ModelManager::LoadObjFile("resources", "sphere.obj", device.Get());

	Microsoft::WRL::ComPtr <ID3D12Resource> vertexResource = CreateBufferResource(device.Get(), sizeof(Cygnus::VertexData) * modelData.vertices.size());

	// 頂点バッファビューを作成する
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	// リソースの先頭のアドレスから使う
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	// 使用するリソースのサイズは頂点のサイズ
	vertexBufferView.SizeInBytes = UINT(sizeof(Cygnus::VertexData) * modelData.vertices.size());
	// 1頂点あたりのサイズ
	vertexBufferView.StrideInBytes = sizeof(Cygnus::VertexData);

	// 頂点リソースにデータを書き込む
	Cygnus::VertexData* vertexData = nullptr;
	// 書き込むためのアドレスを取得
	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	std::memcpy(vertexData, modelData.vertices.data(), sizeof(Cygnus::VertexData)* modelData.vertices.size()); // 頂点データをリソースにコピー

	// マテリアル用のリソースを作る
	Microsoft::WRL::ComPtr <ID3D12Resource> materialResource = CreateBufferResource(device.Get(), sizeof(Material));
	// マテリアルにデータを書き込む
	Material* materialData = nullptr;
	// 書き込むためのアドレスを取得
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	// 輝度を白に設定
	materialData->color = Cygnus::Float4(1.0f, 1.0f, 1.0f, 1.0f);
	// Lightingを有効にする
	materialData->enableLighting = true;


	// WVP用のリソースを作る
	Microsoft::WRL::ComPtr <ID3D12Resource> wvpResource = CreateBufferResource(device.Get(), sizeof(TransformationMatrix));
	// データを書き込む
	TransformationMatrix* wvpData = nullptr;
	// 書き込むためのアドレスを取得
	wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));
	// 単位行列を書き込んでおく
	wvpData->WVP = Cygnus::Matrix::Identity();

	///
	///	ここまで3Dオブジェクト
	/// 
	
	///
	///	ここから平行光源
	/// 
	
	// 平行光源のResourceを作成
	Microsoft::WRL::ComPtr <ID3D12Resource> directionalLightResource = CreateBufferResource(device.Get(), sizeof(DirectionalLight));
	// データを書き込む
	DirectionalLight* directionalLightData = nullptr;
	// 書き込むためのアドレスを取得
	directionalLightResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));
	// デフォルト値を書き込む
	directionalLightData->color = { 1.0f,1.0f,1.0f,1.0f };
	directionalLightData->direction = { 0.0f, -1.0f, 0.0f };
	directionalLightData->intensity = 1.0f;

	///
	///	ここまで平行光源
	/// 

	///
	///	ここからスプライト
	/// 
	
	// Sprite用の頂点リソースを作る
	Microsoft::WRL::ComPtr <ID3D12Resource> vertexResourceSprite = CreateBufferResource(device.Get(), sizeof(Cygnus::VertexData) * 4);

	// 頂点バッファビューを作成する
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};
	// リソースの先頭のアドレスから使う
	vertexBufferViewSprite.BufferLocation = vertexResourceSprite->GetGPUVirtualAddress();
	// 使用するリソースのサイズは頂点4つ分のサイズ
	vertexBufferViewSprite.SizeInBytes = sizeof(Cygnus::VertexData) * 4;
	// 1頂点あたりのサイズ
	vertexBufferViewSprite.StrideInBytes = sizeof(Cygnus::VertexData);

	// 頂点データを設定
	Cygnus::VertexData* vertexDataSprite = nullptr;
	// 書き込むためのアドレスを取得
	vertexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataSprite));
	// 左下
	vertexDataSprite[0].position = { 0.0f, 360.0f, 0.0f, 1.0f };
	vertexDataSprite[0].texcoord = { 0.0f, 1.0f };
	// 左上
	vertexDataSprite[1].position = { 0.0f, 0.0f, 0.0f, 1.0f };
	vertexDataSprite[1].texcoord = { 0.0f, 0.0f };
	// 右下
	vertexDataSprite[2].position = { 640.0f, 360.0f, 0.0f, 1.0f };
	vertexDataSprite[2].texcoord = { 1.0f, 1.0f };
	// 右上
	vertexDataSprite[3].position = { 640.0f, 0.0f, 0.0f, 1.0f };
	vertexDataSprite[3].texcoord = { 1.0f, 0.0f };

	// 頂点インデックスの作成
	Microsoft::WRL::ComPtr <ID3D12Resource> indexResourceSprite = CreateBufferResource(device.Get(), sizeof(uint32_t) * 6);

	D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite{};
	// リソースの先頭のアドレスから使う
	indexBufferViewSprite.BufferLocation = indexResourceSprite->GetGPUVirtualAddress();
	// 使用するリソースのサイズはインデックス6つ分のサイズ
	indexBufferViewSprite.SizeInBytes = sizeof(uint32_t) * 6;
	// インデックスはuint32_tとする
	indexBufferViewSprite.Format = DXGI_FORMAT_R32_UINT;

	// インデックスリソースにデータを書き込む
	uint32_t* indexDataSprite = nullptr;
	indexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&indexDataSprite));
	indexDataSprite[0] = 0; indexDataSprite[1] = 1; indexDataSprite[2] = 2;
	indexDataSprite[3] = 1; indexDataSprite[4] = 3; indexDataSprite[5] = 2;

	// Sprite用のTransformationMatrix用のリソースを作る。Matrix 1つ分のサイズを用意する
	Microsoft::WRL::ComPtr <ID3D12Resource> transformationMatrixResourceSprite = CreateBufferResource(device.Get(), sizeof(TransformationMatrix));
	// データを書き込む
	TransformationMatrix* transformationMatrixDataSprite = nullptr;
	// 書き込むためのアドレスを取得
	transformationMatrixResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataSprite));
	// 単位行列を書き込んでおく
	transformationMatrixDataSprite->WVP = Cygnus::Matrix::Identity();

	// CPUで動かす用のTransformを作る
	Cygnus::Transform transformSprite{ {1.0f, 1.0f, 1.0f}, {0.0f,0.0f,0.0f}, {0.0f, 0.0f, 0.0f} };

	// sprite用のマテリアル用のリソースを作る
	Microsoft::WRL::ComPtr <ID3D12Resource> materialResourceSprite = CreateBufferResource(device.Get(), sizeof(Material));
	// マテリアルにデータを書き込む
	Material* materialDataSprite = nullptr;
	// 書き込むためのアドレスを取得
	materialResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&materialDataSprite));
	// 輝度を白に設定
	materialDataSprite->color = Cygnus::Float4(1.0f, 1.0f, 1.0f, 1.0f);
	// Lightingを無効にする
	materialDataSprite->enableLighting = false;

	///
	///	ここまでスプライト
	/// 

	// Transform変数を作る
	Cygnus::Transform transform{ {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };

	// カメラのインスタンスを生成
	Cygnus::Camera camera({ 0.0f,0.0f,-5.0f },{0.0f,0.0f,0.0f},0.45f);

	uint32_t uvCheckerGH = Cygnus::TextureManager::Load("Resources/uvChecker.png", device.Get());
	uint32_t monsterBallGH = Cygnus::TextureManager::Load("Resources/monsterBall.png", device.Get());

	while (!Cygnus::Window::ProcessMessage()) {

		///
		/// メインループ
		/// 

		// これから書き込むバックバッファのインデックスを取得
		UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();

		// TransitionBarrirの設定
		D3D12_RESOURCE_BARRIER barrier{};
		// 今回のバリアはTransition
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		// Noneにしておく
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		// バリアを張る対象のリソース。現在のバックバッファに対して行う
		barrier.Transition.pResource = swapChainResources[backBufferIndex].Get();
		// 遷移前(現在)のResourceState
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		// 遷移後のResourceState
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		// TransitionBarrierを張る
		commandList->ResourceBarrier(1, &barrier);

		// 描画先のRTVとDSVを設定する
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap.GetCPUHandle(0);
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvDescriptorHeap.GetCPUHandle(backBufferIndex);
		commandList->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);
		// 指定した色で画面全体をクリアする
		float clearColor[] = { 0.1f,0.25f,0.5f,1.0f }; // 青っぽい色。RGBAの順
		commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
		// 指定した深度で画面全体をクリアする
		commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		// 描画用のDescriptorHeapの設定
		ID3D12DescriptorHeap* descriptorHeaps[] = { Cygnus::TextureManager::GetInstance().srvHeap.heap.Get() };
		commandList->SetDescriptorHeaps(1, descriptorHeaps);

		// ImGuiにここからフレームが始まる旨を告げる
		Cygnus::ImguiWrapper::NewFrame();

		static bool isRotate = false;

		///
		/// ゲームループ
		/// 

		///
		///	更新処理
		/// 

		// ImGuiのデモ用のUIを表示
		/*ImGui::ShowDemoWindow();*/

		if (ImGui::Begin("Triangle")) {
			// 色を決める
			ImGui::ColorEdit4("TriangleColor", &materialData->color.x);
			// ボタンが押されたら回転
			if (ImGui::Button("rotate")) {
				isRotate = !isRotate;
			}
			ImGui::Checkbox("rotateCheck",&isRotate);
			ImGui::DragFloat3("LightDirection", &directionalLightData->direction.x);
		}
		ImGui::End();

		if (isRotate) {
			transform.rotate.y += 0.03f;
		}

		transform.scale = { 0.5f,0.5f,0.5f };

		if (ImGui::Begin("Sprite")) {
			// 移動させる
			ImGui::DragFloat3("SpritePosition", &transformSprite.translate.x);
		}
		ImGui::End();

		Cygnus::Matrix worldMatrix = transform.MakeAffineMatrix();
		Cygnus::Matrix viewMatrix = camera.MakeViewMatrix();
		Cygnus::Matrix projectionMatrix = camera.MakePerspectiveFovMatrix();
		Cygnus::Matrix worldViewProjectionMatrix = worldMatrix * viewMatrix * projectionMatrix;
		wvpData->WVP = worldViewProjectionMatrix;
		wvpData->World = worldMatrix;

		// Sprite用のWorldViewProjectionMatrixを作る
		Cygnus::Matrix worldMatrixSprite = transformSprite.MakeAffineMatrix();
		Cygnus::Matrix viewMatrixSprite = Cygnus::Matrix::Identity();
		Cygnus::Matrix projectionMatrixSprite = Cygnus::Matrix::Orthographic(static_cast<float>(Cygnus::Window::GetWidth()),
			static_cast<float>(Cygnus::Window::GetHeight()), 0.0f, 100.0f);
		Cygnus::Matrix worldViewProjectionMatrixSprite = worldMatrixSprite * (viewMatrixSprite * projectionMatrixSprite);
		transformationMatrixDataSprite->WVP = worldViewProjectionMatrixSprite;
		transformationMatrixDataSprite->World = worldMatrixSprite;

		///
		///	描画処理
		/// 

		///
		///	フレーム毎の描画コマンド
		/// 
		commandList->RSSetViewports(1, &viewport); // Viewportを設定
		commandList->RSSetScissorRects(1, &scissorRect); // Scissorを設定
		// RootSignatureを設定。PSOに設定しているけど別途設定が必要
		commandList->SetGraphicsRootSignature(rootSignature.Get());
		commandList->SetPipelineState(graphicsPipelineState.Get()); // PSOを設定
		// 平行光源の情報の定数バッファのセット
		commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());

		///
		///	3Dオブジェクトの描画コマンド
		/// 
		commandList->IASetVertexBuffers(0, 1, &vertexBufferView); // VBVを設定
		// 形状を設定。PSOに設定しているものとはまた別
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		// マテリアルCBufferの場所を設定
		commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
		// wvp用のCBufferの場所を設定
		commandList->SetGraphicsRootConstantBufferView(1, wvpResource->GetGPUVirtualAddress());
		// SRVのDescriptorTableの先頭を設定(Textureの設定)
		Cygnus::TextureManager::SetDescriptorTable(2, commandList.Get(), modelData.material.textureHandle);
		// モデルデータから頂点を読み、インスタンスを生成
		commandList->DrawInstanced(UINT(modelData.vertices.size()), 1, 0, 0);

		///
		///	スプライトの描画コマンド
		///
		commandList->IASetVertexBuffers(0, 1, &vertexBufferViewSprite); // VBVを設定
		commandList->IASetIndexBuffer(&indexBufferViewSprite); // IBVを設定
		commandList->SetGraphicsRootConstantBufferView(0, materialResourceSprite->GetGPUVirtualAddress());
		// TransformationMatrixCBufferの場所を設定
		commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResourceSprite->GetGPUVirtualAddress());
		// SRVのDescriptorTableの先頭を設定(Textureの設定)
		Cygnus::TextureManager::SetDescriptorTable(2, commandList.Get(), uvCheckerGH);
		// 描画、6個のインデックスを使用して1つのインスタンスを描画
		/*commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);*/

		///
		/// ゲームループここまで
		/// 

		// ImGuiの内部コマンドを生成する
		Cygnus::ImguiWrapper::Render(commandList.Get());

		// 画面に描く処理はすべて終わり、画面に映すので、状態を遷移
		// 今回はRenderTargetからPresentにする
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		// TransitionBarrierを張る
		commandList->ResourceBarrier(1, &barrier);

		// コマンドリストの内容を確定させる。すべてのコマンドを積んでからCloseすること
		hr = commandList->Close();
		assert(SUCCEEDED(hr));

		// GPUにコマンドリストの実行を行わせる
		ID3D12CommandList* commandLists[] = { commandList.Get() };
		commandQueue->ExecuteCommandLists(1, commandLists);
		// GPUとOSに画面の交換を行うよう通知する
		swapChain->Present(1, 0);

		// Fenceの値を更新
		fenceValue++;
		// GPUがここまでたどり着いたときに、Fenceの値を指定した値に代入するようにSignalを送る
		commandQueue->Signal(fence.Get(), fenceValue);
		// Fenceの値が指定したSignal値にたどり着いているかを確認する
		// GetCompletedValueの初期値はFence作成時に渡した初期値
		if (fence->GetCompletedValue() < fenceValue)
		{
			// 指定したSignalにたどりついていないので、たどり着くまで待つようにイベントを設定する
			fence->SetEventOnCompletion(fenceValue, fenceEvent);
			std::this_thread::sleep_for(std::chrono::microseconds(16000)); // 60fpsに制限
			// イベント待つ
			WaitForSingleObject(fenceEvent, INFINITE);
		}

		// 次のフレーム用のコマンドリストを準備
		hr = commandAllocator->Reset();
		assert(SUCCEEDED(hr));
		hr = commandList->Reset(commandAllocator.Get(), nullptr);
		assert(SUCCEEDED(hr));

	}

	// ImGuiの終了処理
	Cygnus::ImguiWrapper::Finalize();

	CoUninitialize();

	return 0;
}