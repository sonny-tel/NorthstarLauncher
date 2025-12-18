#pragma once

#include <cstdint>

#include "eos_sdk.h"

#include "fake_ip_layer.h"

namespace eos
{

bool Initialize();
bool InitializeNetworking();
void ShutdownNetworking();
bool IsReady();

bool RegisterPeerByString(const char* remoteProductUserId,
                          const char* socketName,
                          uint8_t channel,
                          FakeEndpoint* outEndpoint);
} // namespace eos
