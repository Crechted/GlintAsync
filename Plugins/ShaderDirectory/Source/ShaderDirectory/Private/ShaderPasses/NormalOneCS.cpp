#include "ShaderPasses/NormalOneCS.h"

IMPLEMENT_SHADER_TYPE(, FNormalOneCS, TEXT("/Plugin/ShaderDirectory/NormalCompute.usf"), TEXT("NormalOneCS"), SF_Compute);