#include "ShaderPasses/SceneTextureCS.h"

IMPLEMENT_SHADER_TYPE(, FSomeTextureCS, TEXT("/Plugin/ShaderDirectory/TestShader.usf"), TEXT("AccumulateSomeTextureCompute"), SF_Compute);