
// Dx12 common static samplers
// More can be added in RootSignature::RootSignature in Dx12Device.cpp



SamplerState SamplerPointClamp		: register(s0);
SamplerState SamplerLinearClamp		: register(s1);

SamplerState SamplerPointRepeat		: register(s2);
SamplerState SamplerLinearRepeat	: register(s3);


