#include "ShaderPasses/WaterShaders.h"

IMPLEMENT_SHADER_TYPE(, FWaterSimulatePassVS, TEXT("/Plugin/ShaderDirectory/WaterShader.usf"), TEXT("MainVS"), SF_Vertex);
IMPLEMENT_SHADER_TYPE(, FWaterSimulatePassPS, TEXT("/Plugin/ShaderDirectory/WaterShader.usf"), TEXT("MainPS"), SF_Pixel);