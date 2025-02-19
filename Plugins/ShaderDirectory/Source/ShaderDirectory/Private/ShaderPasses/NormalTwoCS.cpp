#include "ShaderPasses/NormalTwoCS.h"

IMPLEMENT_SHADER_TYPE(, FNormalTwoCS, TEXT("/Plugin/ShaderDirectory/NormalCompute.usf"), TEXT("NormalTwoCS"), SF_Compute);