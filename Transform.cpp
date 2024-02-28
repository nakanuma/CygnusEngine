#include "Transform.h"

Cygnus::Matrix Cygnus::Transform::MakeAffineMatrix()
{
    Cygnus::Matrix result = Cygnus::Matrix::Identity();
    
    // SRTの順番で行列を生成してかける
    result *= Cygnus::Matrix::Scaling({ scale.x, scale.y, scale.z });
    result *= Cygnus::Matrix::RotationRollPitchYaw(rotate.z, rotate.x, rotate.y);
    result *= Cygnus::Matrix::Translation({ translate.x, translate.y, translate.z });

    return result;
}
