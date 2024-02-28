#include "Camera.h"
#include "MyWindow.h"

Cygnus::Camera::Camera(Cygnus::Float3 argTranslate, Cygnus::Float3 argRotate, float argFov)
{
	transform.translate = argTranslate;
	transform.rotate = argRotate;
	transform.scale = { 1.0f,1.0f,1.0f };
	fov = argFov;
}

Cygnus::Matrix Cygnus::Camera::MakeViewMatrix()
{
	Cygnus::Matrix affine = transform.MakeAffineMatrix();

	return Cygnus::Matrix::Inverse(affine);
}

Cygnus::Matrix Cygnus::Camera::MakePerspectiveFovMatrix()
{
	return Cygnus::Matrix::PerspectiveFovLH(fov, static_cast<float>(Cygnus::Window::GetWidth()) / static_cast<float>(Cygnus::Window::GetHeight()), nearZ, farZ);
}
