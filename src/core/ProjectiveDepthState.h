#pragma once
#include "backend/api/BackendTypes.h"
#include <cstdint>

struct ProjectiveDepthOptim {
    // Rows hold (camera index + 1, log-range Adam m/v, observations).
    float4* states = nullptr;
    // EMA of dimensionless bearing and magnification information.
    float* information = nullptr;
    const float4* centers = nullptr;
    const int32_t* camera_indices = nullptr;
    float lr = 0;
    float angular_lr = 0;
    bool single_photometric = false;
    bool angular_relative = true;
    bool single_observation = false;
};
