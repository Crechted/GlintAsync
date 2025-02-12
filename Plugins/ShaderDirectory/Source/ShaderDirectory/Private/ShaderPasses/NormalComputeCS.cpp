#include "ShaderPasses/NormalComputeCS.h"

IMPLEMENT_SHADER_TYPE(, FNormalComputeCS, TEXT("/Plugin/ShaderDirectory/NormalCompute.usf"), TEXT("NormalACS"), SF_Compute);