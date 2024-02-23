#include "Transform.h"

using namespace DirectX;

DirectX::XMMATRIX Cygnus::Transform::MakeAffineMatrix()
{
    XMMATRIX result = XMMatrixIdentity();

    // スケール行列を生成してかける
    result *= XMMatrixScaling(scale.x,scale.y,scale.z); 
    
    // Roll Pitch Yawの順番で回転行列を生成してかける(Z -> X -> Y)
    XMMATRIX rotMatrix = XMMatrixIdentity();
    rotMatrix *= XMMatrixRotationZ(rotate.z);
    rotMatrix *= XMMatrixRotationX(rotate.x);
    rotMatrix *= XMMatrixRotationY(rotate.y);

    result *= rotMatrix;

    // Translasion行列を生成してかける
    result *= XMMatrixTranslation(translate.x, translate.y, translate.z);

    return result;
}
