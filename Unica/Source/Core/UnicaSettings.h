#pragma once

#include <vector>

#include "UnicaMinimal.h"

namespace UnicaSettings
{
	static const uint32 WindowWidth = 1270;
	static const uint32 WindowHeight = 900;
	static const std::string EngineName = "Unica Engine";
	static const std::string ApplicationName = "Sandbox";
	const std::vector<const char*> RequestedValidationLayers = { "VK_LAYER_KHRONOS_validation" };

#ifndef NDEBUG
	static const bool bValidationLayersEnabled = true;
#else
	static const bool bValidationLayersEnabled = false;
#endif // NDEBUG
}