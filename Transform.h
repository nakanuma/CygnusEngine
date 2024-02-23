#pragma once
#include <DirectXMath.h>

namespace Cygnus {
	class Transform
	{
	public:
		DirectX::XMFLOAT3 scale;
		DirectX::XMFLOAT3 rotate;
		DirectX::XMFLOAT3 translate;

		DirectX::XMMATRIX MakeAffineMatrix();
	};
}

