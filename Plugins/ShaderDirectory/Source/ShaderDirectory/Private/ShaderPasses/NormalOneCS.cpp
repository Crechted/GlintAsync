#include "ShaderPasses/NormalOneCS.h"

IMPLEMENT_SHADER_TYPE(, FNormalOneCS, TEXT("/Plugin/ShaderDirectory/NormalOneCompute.usf"), TEXT("NormalOneCS"), SF_Compute);