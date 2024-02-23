#include "Camera.h"
#include "MyWindow.h"

Cygnus::Camera::Camera(DirectX::XMFLOAT3 argTranslate, DirectX::XMFLOAT3 argRotate, float argFov)
{
	transform.translate = argTranslate;
	transform.rotate = argRotate;
	fov = argFov;
}

DirectX::XMMATRIX Cygnus::Camera::MakeViewMatrix()
{
	DirectX::XMMATRIX affine = transform.MakeAffineMatrix();

	return DirectX::XMMatrixInverse(nullptr, affine);
}

DirectX::XMMATRIX Cygnus::Camera::MakePerspectiveFovMatrix()
{
	return DirectX::XMMatrixPerspectiveFovLH(fov, static_cast<float>(Cygnus::Window::GetWidth()) / static_cast<float>(Cygnus::Window::GetHeight()), nearZ, farZ);
}
