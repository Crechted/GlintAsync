#include "ShaderPasses/GlintWaterCS.h"

IMPLEMENT_SHADER_TYPE(, FGlintWaterTextureCS, TEXT("/Plugin/ShaderDirectory/GlintWaterCompute.usf"), TEXT("AccumulateGlintWaterModelCompute"), SF_Compute);