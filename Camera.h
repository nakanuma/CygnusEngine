#pragma once
#include "CygnusMath.h"

namespace Cygnus {
	class Camera
	{
	public:
		Camera(Cygnus::Float3 translate, Cygnus::Float3 rotate = Cygnus::Float3(0.0f, 0.0f, 0.0f), float fov = PIf / 2.0f); // 回転角と視野角にデフォルト値を設定

		Transform transform;

		float fov; // 視野角(ラジアン)

		float nearZ = 0.1f, farZ = 1000.0f; // クリップの設定

		Cygnus::Matrix MakeViewMatrix();
		Cygnus::Matrix MakePerspectiveFovMatrix();
	};
}

