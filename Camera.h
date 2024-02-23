#pragma once
#include "Transform.h"
#include <DirectXMath.h>

namespace Cygnus {
	class Camera
	{
	public:
		Camera(DirectX::XMFLOAT3 translate, DirectX::XMFLOAT3 rotate = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f), float fov = 3.1415926f / 2.0f); // 回転角と視野角にデフォルト値を設定

		Transform transform;

		float fov; // 視野角(ラジアン)

		float nearZ = 0.1f, farZ = 1000.0f; // クリップの設定

		DirectX::XMMATRIX MakeViewMatrix();
		DirectX::XMMATRIX MakePerspectiveFovMatrix();
	};
}

