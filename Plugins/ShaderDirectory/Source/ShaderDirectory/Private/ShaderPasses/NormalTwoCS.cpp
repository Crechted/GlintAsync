#include "ShaderPasses/NormalTwoCS.h"

IMPLEMENT_SHADER_TYPE(, FNormalTwoCS, TEXT("/Plugin/ShaderDirectory/NormalTwoCompute.usf"), TEXT("NormalTwoCS"), SF_Compute);