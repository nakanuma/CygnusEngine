#include "Object3d.hlsli"

struct TransformationMatrix{
    float32_t4x4 WVP;
};
ConstantBuffer<TransformationMatrix> gTransFormationMatrix : register(b0);

struct VertexShaderInput {
	float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
};

VertexShaderOutput main(VertexShaderInput input) {
	VertexShaderOutput output;
    output.position = mul(input.position, gTransFormationMatrix.WVP);
    output.texcoord = input.texcoord;
	return output;
}