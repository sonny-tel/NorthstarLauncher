#include "eos_network.h"

#include <algorithm>
#include <cstring>
#include <mutex>
#include <unordered_map>

#include <winsock2.h>
#include <ws2tcpip.h>

#include "eos_layer.h"
#include "eos_threading.h"
#include "core/convar/cvar.h"
#include "net_hooks.h"
#include "util/version.h"

namespace
{

constexpr char kProductId[] = "38a41893d2fa4e73801e0daed2060cb6";
constexpr char kSandboxId[] = "d15ffd5f9b5b479aa7c382a8f6896cee";
constexpr char kDeploymentId[] = "aa85e873e78244909bef018a8532741c";
constexpr char kProductName[] = "ion";

struct Version
{
    int major = 0;
    int minor = 0;
    int patch = 0;
};

using SendToFn = int (WSAAPI*)(SOCKET, const char*, int, int, const sockaddr*, int);
using RecvFromFn = int (WSAAPI*)(SOCKET, char*, int, int, sockaddr*, int*);
using CloseSocketFn = int (WSAAPI*)(SOCKET);

SendToFn g_realSendTo = nullptr;
RecvFromFn g_realRecvFrom = nullptr;
CloseSocketFn g_realCloseSocket = nullptr;
LPVOID g_sendToTarget = nullptr;
LPVOID g_recvFromTarget = nullptr;
LPVOID g_closeSocketTarget = nullptr;
bool g_hooksInstalled = false;
std::mutex g_socketRouteMutex;
std::unordered_map<SOCKET, eos::PacketRoute> g_socketRoutes;
std::unordered_map<SOCKET, bool> g_fakeIpRecvSockets;

bool ExtractFakeEndpoint(const sockaddr* address, eos::FakeEndpoint& outEndpoint)
{
    if (!address || address->sa_family != AF_INET6)
        return false;

    const auto* ipv6 = reinterpret_cast<const sockaddr_in6*>(address);
    if (ntohs(ipv6->sin6_addr.u.Word[0]) != 0x3FFE)
        return false;

    outEndpoint.address = ipv6->sin6_addr;
    outEndpoint.port = ntohs(ipv6->sin6_port);
    return true;
}

void WriteSockaddrForFakeEndpoint(const eos::FakeEndpoint& endpoint,
                                  sockaddr* outAddress,
                                  int* outLength)
{
    if (!outAddress || !outLength)
        return;

    const uint16_t prefix    = ntohs(endpoint.address.u.Word[0]);
    const uint16_t realPort  = endpoint.port;
    uint16_t pretendPort     = eos::GetPretendRemotePort();
    const int    inLen       = *outLength;

    spdlog::info("EOS: WriteSockaddrForFakeEndpoint in: prefix=0x{:04X} port={} pretendPort={} inLen={}",
                 prefix, realPort, pretendPort, inLen);

    if (pretendPort == 0)
        pretendPort = endpoint.port;

    if (*outLength >= static_cast<int>(sizeof(sockaddr_in6)))
    {
        auto* ipv6 = reinterpret_cast<sockaddr_in6*>(outAddress);
        std::memset(ipv6, 0, sizeof(sockaddr_in6));
        ipv6->sin6_family = AF_INET6;
        ipv6->sin6_port   = htons(pretendPort);
        ipv6->sin6_addr   = endpoint.address;
        *outLength        = sizeof(sockaddr_in6);

        char addrStr[INET6_ADDRSTRLEN] = {};
        InetNtopA(AF_INET6, &ipv6->sin6_addr, addrStr, sizeof(addrStr));
        spdlog::info("EOS: wrote IPv6 sockaddr: family={} port={} addr={} outLen={}",
                     ipv6->sin6_family,
                     ntohs(ipv6->sin6_port),
                     addrStr,
                     *outLength);
        return;
    }

    if (*outLength >= static_cast<int>(sizeof(sockaddr_in)))
    {
        auto* ipv4 = reinterpret_cast<sockaddr_in*>(outAddress);
        std::memset(ipv4, 0, sizeof(sockaddr_in));
        ipv4->sin_family           = AF_INET;
        ipv4->sin_port             = htons(pretendPort);
        ipv4->sin_addr.S_un.S_addr = 0;
        *outLength                 = sizeof(sockaddr_in);

        spdlog::info("EOS: wrote IPv4 sockaddr: family={} port={} addr=0.0.0.0 outLen={}",
                     ipv4->sin_family,
                     ntohs(ipv4->sin_port),
                     *outLength);
    }
    else
    {
        spdlog::warn("EOS: WriteSockaddrForFakeEndpoint: buffer too small (inLen={})", inLen);
    }
}


bool IsEndpointValid(const eos::FakeEndpoint& endpoint)
{
    return ntohs(endpoint.address.u.Word[0]) == 0x3FFE;
}

uint16_t GetLocalSocketPort(SOCKET socketHandle)
{
    sockaddr_storage addr{};
    int addrLen = sizeof(addr);
    if (getsockname(socketHandle, reinterpret_cast<sockaddr*>(&addr), &addrLen) == SOCKET_ERROR)
        return 0;

    if (addr.ss_family == AF_INET)
        return ntohs(reinterpret_cast<sockaddr_in*>(&addr)->sin_port);

    if (addr.ss_family == AF_INET6)
        return ntohs(reinterpret_cast<sockaddr_in6*>(&addr)->sin6_port);

    return 0;
}

eos::PacketRoute ClassifySocketRoute(uint16_t port)
{
    if (port == 0)
        return eos::PacketRoute::All;

    const uint16_t clientPort = static_cast<uint16_t>(g_pCVar->FindVar("clientport")->GetInt());
    const uint16_t hostPort = static_cast<uint16_t>(g_pCVar->FindVar("hostport")->GetInt());

    if (clientPort && port == clientPort)
        return eos::PacketRoute::Client;

    if (hostPort && port == hostPort)
        return eos::PacketRoute::Server;

    return eos::PacketRoute::All;
}

eos::PacketRoute DetermineSocketRoute(SOCKET socketHandle)
{
    {
        std::lock_guard lock(g_socketRouteMutex);
        auto it = g_socketRoutes.find(socketHandle);
        if (it != g_socketRoutes.end())
            return it->second;
    }

    const uint16_t port = GetLocalSocketPort(socketHandle);
    const eos::PacketRoute route = ClassifySocketRoute(port);

    {
        std::lock_guard lock(g_socketRouteMutex);
        g_socketRoutes[socketHandle] = route;
    }

    return route;
}

bool ShouldUseFakeIpOnRecv(SOCKET socketHandle)
{
    {
        std::lock_guard lock(g_socketRouteMutex);
        auto it = g_fakeIpRecvSockets.find(socketHandle);
        if (it != g_fakeIpRecvSockets.end())
            return it->second;
    }

    sockaddr_storage addr{};
    int addrLen = sizeof(addr);
    if (getsockname(socketHandle, reinterpret_cast<sockaddr*>(&addr), &addrLen) == SOCKET_ERROR)
        return false;

    if (addr.ss_family != AF_INET6)
    {
        // IPv4 or something else – never consume FakeIp packets
        std::lock_guard lock(g_socketRouteMutex);
        g_fakeIpRecvSockets[socketHandle] = false;
        return false;
    }

    const uint16_t port = ntohs(reinterpret_cast<sockaddr_in6*>(&addr)->sin6_port);
    const eos::PacketRoute route = ClassifySocketRoute(port);
    const bool useFake =
        (route == eos::PacketRoute::Client || route == eos::PacketRoute::Server);

    {
        std::lock_guard lock(g_socketRouteMutex);
        g_fakeIpRecvSockets[socketHandle] = useFake;
        g_socketRoutes[socketHandle]      = route;
    }

    spdlog::info("EOS: ShouldUseFakeIpOnRecv sock={} fam={} port={} route={} useFake={}",
                 (uint64_t)socketHandle,
                 addr.ss_family,
                 port,
                 (int)ClassifySocketRoute(port),
                 useFake);

    return useFake;
}

int WSAAPI HookedSendTo(SOCKET socketHandle,
                        const char* buffer,
                        int length,
                        int flags,
                        const sockaddr* destAddr,
                        int destLen)
{
    eos::FakeEndpoint endpoint{};
    if (!buffer || length <= 0 || !ExtractFakeEndpoint(destAddr, endpoint))
    {
        return g_realSendTo
            ? g_realSendTo(socketHandle, buffer, length, flags, destAddr, destLen)
            : SOCKET_ERROR;
    }

	spdlog::info("EOS: HookedSendTo called on socket {} to FakeEndpoint port={}",
				 (uint64_t)socketHandle,
				 endpoint.port);

    // Lazy initialize EOS when we first try to send to a fakeip
    if (!eos::EnsureEosInitialized())
    {
        WSASetLastError(WSAENOTCONN);
        return SOCKET_ERROR;
    }

	spdlog::info("EOS: Sending to FakeEndpoint port={} length={}", endpoint.port, length);

    auto* layer = eos::EosLayer::Instance().GetFakeIpLayer();
    if (!layer)
    {
        WSASetLastError(WSAENOTCONN);
        return SOCKET_ERROR;
    }

    const eos::PacketRoute route = DetermineSocketRoute(socketHandle);
    const bool sent = layer->SendToPeer(endpoint,
                                        reinterpret_cast<const uint8_t*>(buffer),
                                        static_cast<size_t>(length),
                                        route);
    if (!sent)
    {
		spdlog::error("EOS: Failed to send packet to FakeEndpoint port={}", endpoint.port);
        WSASetLastError(WSAECONNABORTED);
        return SOCKET_ERROR;
    }

    return length;
}

bool IsCalledFromNetReceiveDatagram()
{
    static uintptr_t s_funcStart = 0;
    static uintptr_t s_funcEnd   = 0;

    if (!s_funcStart)
    {
        HMODULE hEngine = GetModuleHandleA("engine.dll");
        if (!hEngine)
            return false;

        s_funcStart = reinterpret_cast<uintptr_t>(hEngine) + 0x21B520;
        s_funcEnd   = s_funcStart + 0x1000; // adjust if you know a better size
    }

    void* frames[8] = {};
    USHORT captured = RtlCaptureStackBackTrace(0, 8, frames, nullptr);
    if (captured == 0)
        return false;

    // frame 0 = this function (IsCalledFromNetReceiveDatagram or HookedRecvFrom,
    // depending where you call it). Start from 1.
    for (USHORT i = 1; i < captured; ++i)
    {
        uintptr_t addr = reinterpret_cast<uintptr_t>(frames[i]);
        if (addr >= s_funcStart && addr < s_funcEnd)
        {
            spdlog::debug("EOS: RecvFrom caller frame {} is inside NET_ReceiveDatagram (0x{:X})",
                          i, addr);
            return true;
        }
    }

    return false;
}

int WSAAPI HookedRecvFrom(SOCKET socketHandle,
                          char* buffer,
                          int length,
                          int flags,
                          sockaddr* from,
                          int* fromLen)
{
    // Only intercept when NET_ReceiveDatagram is somewhere on the call stack
    if (!IsCalledFromNetReceiveDatagram())
    {
        return g_realRecvFrom
            ? g_realRecvFrom(socketHandle, buffer, length, flags, from, fromLen)
            : SOCKET_ERROR;
    }

    auto* layer = eos::EosLayer::Instance().GetFakeIpLayer();
    if (layer)
    {
        eos::PendingPacket packet;
        const eos::PacketRoute desiredRoute = DetermineSocketRoute(socketHandle);
        if (layer->PopPacket(desiredRoute, packet))
        {
            spdlog::info("EOS: Popped packet from FakeIpLayer");
            const int copyLength =
                static_cast<int>(std::min<size_t>(static_cast<size_t>(length),
                                                  packet.payload.size()));
            if (buffer && copyLength > 0)
                std::memcpy(buffer, packet.payload.data(), copyLength);

            if (from && fromLen)
            {
                spdlog::info("EOS: Writing sockaddr for FakeEndpoint on recvfrom");
                WriteSockaddrForFakeEndpoint(packet.sender, from, fromLen);
            }

            return copyLength;
        }
    }

    // Fallback: normal network traffic (or no EOS packet available)
    return g_realRecvFrom
        ? g_realRecvFrom(socketHandle, buffer, length, flags, from, fromLen)
        : SOCKET_ERROR;
}

int WSAAPI HookedCloseSocket(SOCKET s)
{
    {
        std::lock_guard lock(g_socketRouteMutex);
        g_socketRoutes.erase(s);
        g_fakeIpRecvSockets.erase(s);
    }
    return g_realCloseSocket ? g_realCloseSocket(s) : 0;
}

bool InstallSocketHooks()
{
    if (g_hooksInstalled)
        return true;

    void* origSend = HookImportByOrdinal("engine.dll", "WSOCK32.dll", 20, reinterpret_cast<void*>(&HookedSendTo));
    void* origRecv = HookImportByOrdinal("engine.dll", "WSOCK32.dll", 17, reinterpret_cast<void*>(&HookedRecvFrom));

    if (!origSend || !origRecv)
    {
        spdlog::error("EOS: Failed to IAT-hook WSOCK32 sendto/recvfrom");
        return false;
    }

    g_realSendTo   = reinterpret_cast<SendToFn>(origSend);
    g_realRecvFrom = reinterpret_cast<RecvFromFn>(origRecv);

    // Still hook closesocket globally with MinHook
    HMODULE ws2 = GetModuleHandleA("wsock32.dll");
    if (!ws2)
        ws2 = LoadLibraryA("wsock32.dll");
    if (!ws2)
        return false;

    g_closeSocketTarget = reinterpret_cast<LPVOID>(GetProcAddress(ws2, "closesocket"));
    if (!g_closeSocketTarget)
        return false;

    if (MH_CreateHook(g_closeSocketTarget,
                      &HookedCloseSocket,
                      reinterpret_cast<LPVOID*>(&g_realCloseSocket)) != MH_OK)
        return false;

    const MH_STATUS closeStatus = MH_EnableHook(g_closeSocketTarget);
    if (closeStatus != MH_OK && closeStatus != MH_ERROR_ENABLED)
        return false;

    g_hooksInstalled = true;
    return true;
}

void RemoveSocketHooks()
{
    if (!g_hooksInstalled)
        return;

    if (g_sendToTarget)
    {
        MH_DisableHook(g_sendToTarget);
        MH_RemoveHook(g_sendToTarget);
        g_sendToTarget = nullptr;
    }
    if (g_recvFromTarget)
    {
        MH_DisableHook(g_recvFromTarget);
        MH_RemoveHook(g_recvFromTarget);
        g_recvFromTarget = nullptr;
    }
    if (g_closeSocketTarget)
    {
        MH_DisableHook(g_closeSocketTarget);
        MH_RemoveHook(g_closeSocketTarget);
        g_closeSocketTarget = nullptr;
    }

    g_realSendTo = nullptr;
    g_realRecvFrom = nullptr;
    g_realCloseSocket = nullptr;
    g_hooksInstalled = false;
    {
        std::lock_guard lock(g_socketRouteMutex);
        g_socketRoutes.clear();
    }
}

} // namespace

namespace eos
{

static std::mutex g_lazyInitMutex;
static bool g_lazyInitAttempted = false;
static bool g_lazyInitSuccess = false;

bool EnsureEosInitialized()
{
    std::lock_guard lock(g_lazyInitMutex);

    if (g_lazyInitAttempted)
        return g_lazyInitSuccess;

    g_lazyInitAttempted = true;

    auto& layer = EosLayer::Instance();
    if (layer.IsInitialized())
    {
        g_lazyInitSuccess = true;
        return true;
    }

    if (!layer.Initialize(kProductId, kSandboxId, kDeploymentId, kProductName, version))
    {
        spdlog::error("EOS: Failed to initialize networking layer");
        g_lazyInitSuccess = false;
        return false;
    }

    g_lazyInitSuccess = true;
    return true;
}

bool InitializeNetworking()
{
    // Only install hooks - EOS initialization will happen lazily when needed
    if (!InstallSocketHooks())
    {
        spdlog::error("EOS: Failed to install socket hooks");
        return false;
    }

    spdlog::info("EOS: Hooks installed for version {}, will initialize on first fakeip packet", version);
	net_hooks::Initialize();
    return true;
}

void ShutdownNetworking()
{
    RemoveSocketHooks();
    EosLayer::Instance().Shutdown();

    // Reset lazy init state
    std::lock_guard lock(g_lazyInitMutex);
    g_lazyInitAttempted = false;
    g_lazyInitSuccess = false;
}

bool IsReady()
{
    const auto& layer = EosLayer::Instance();
    return layer.IsInitialized() && layer.GetLocalUser() != nullptr;
}

bool RegisterPeerByString(const char* remoteProductUserId,
                          const char* socketName,
                          uint8_t channel,
                          FakeEndpoint* outEndpoint)
{
    auto& layer = EosLayer::Instance();
    if (!IsReady() || !remoteProductUserId || !socketName)
        return false;

    auto* fakeLayer = layer.GetFakeIpLayer();
    if (!fakeLayer)
        return false;

    EOS_ProductUserId remoteUser = nullptr;
    {
        SdkLock lock(GetSdkMutex());
        remoteUser = EOS_ProductUserId_FromString(remoteProductUserId);
    }
    if (!remoteUser)
        return false;

    const FakeEndpoint endpoint = fakeLayer->RegisterPeer(remoteUser, socketName, channel);
    if (!IsEndpointValid(endpoint))
        return false;

    if (outEndpoint)
    {
        *outEndpoint = endpoint;
    }

    return true;
}

EOS_ProductUserId GetLocalProductUserId()
{
    return EosLayer::Instance().GetLocalUser();
}

FakeEndpoint GetLocalFakeEndpoint()
{
    const auto* layer = EosLayer::Instance().GetFakeIpLayer();
    if (layer)
        return layer->GetLocalEndpoint();
    return {};
}

} // namespace eos
