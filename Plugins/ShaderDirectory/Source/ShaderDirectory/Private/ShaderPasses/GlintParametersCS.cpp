#include "ShaderPasses/GlintParametersCS.h"

IMPLEMENT_SHADER_TYPE(, FGlintParametersCS, TEXT("/Plugin/ShaderDirectory/GlintParametersCompute.usf"), TEXT("GlintParametersCompute"), SF_Compute);