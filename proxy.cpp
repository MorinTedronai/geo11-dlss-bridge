#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <bcrypt.h>
#include <d3d11.h>
#include <MinHook.h>

#include "bridge_core.h"
#include "geo11_adapter.h"
#include "ngx_parameter.h"

#include <array>
#include <atomic>
#include <cstdarg>
#include <cstring>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using namespace stereo_ngx;

namespace {
constexpr wchar_t mod_name[] = L"wh3dlss393.dll";
constexpr char mod_hash[] = "A12CA4AA0F3D510454B395A1EDC612A3CC1CF6C1BF46F01F74B29A0572252CA2";
constexpr char geo_hash[] = "17374B9D279BD1B1AD3B6BAB67A7690B6896160F46AAFEB811921A2D38535D94";
constexpr char game_hash[] = "B7315FA718FD84E2E018E2C4DF06600E9DF0076156B474F148D9D558C939AA55";
constexpr char dlss_hash[] = "3975567B8943C53ACCE397F2B72380092F84F162D00B0D2C7D08A1025C563983";
using create_fn = result(__cdecl *)(context, unsigned, parameters, handle *);
using evaluate_fn = result(__cdecl *)(context, handle, parameters, callback);
using release_fn = result(__cdecl *)(handle);
using direct_input_fn = HRESULT(WINAPI *)(HINSTANCE, DWORD, REFIID, LPVOID *, LPUNKNOWN);

HMODULE mod_module = nullptr, geo_module = nullptr;
std::wstring directory;
HANDLE ready_event = nullptr;
create_fn original_create = nullptr;
evaluate_fn original_evaluate_cpp = nullptr, original_evaluate_c = nullptr;
release_fn original_release = nullptr;
direct_input_fn direct_input_original = nullptr;
thread_local evaluate_fn current_evaluator = nullptr;
thread_local evaluate_fn current_partner_evaluator = nullptr;
std::unique_ptr<bridge> stereo_bridge;
std::atomic<bridge *> active_bridge = nullptr;

[[noreturn]] void early_export_failure();
}

extern "C" HRESULT WINAPI DirectInput8Create_bridge(
    HINSTANCE, DWORD, REFIID, LPVOID *, LPUNKNOWN);
extern "C" void *g_DirectInput8Create = reinterpret_cast<void *>(&DirectInput8Create_bridge);
extern "C" void *g_DllCanUnloadNow = reinterpret_cast<void *>(&early_export_failure);
extern "C" void *g_DllGetClassObject = reinterpret_cast<void *>(&early_export_failure);
extern "C" void *g_DllRegisterServer = reinterpret_cast<void *>(&early_export_failure);
extern "C" void *g_DllUnregisterServer = reinterpret_cast<void *>(&early_export_failure);
extern "C" void *g_GetdfDIJoystick = reinterpret_cast<void *>(&early_export_failure);
extern "C" void *g_WH3DLSS_AcquirePostDepthV2 = reinterpret_cast<void *>(&early_export_failure);
extern "C" void *g_WH3DLSS_EndPostDepthFrameV2 = reinterpret_cast<void *>(&early_export_failure);

namespace {
void log_line(const char *format, ...)
{
    FILE *file = nullptr;
    if (fopen_s(&file, "geo11-dlss-bridge.log", "a") != 0 || file == nullptr) return;
    va_list args; va_start(args, format); std::vfprintf(file, format, args); va_end(args);
    std::fputc('\n', file); std::fclose(file);
}

[[noreturn]] void fail_closed(const char *reason, DWORD code)
{
    log_line("fatal: %s code=0x%08lX", reason, code);
    TerminateProcess(GetCurrentProcess(), code); __assume(0);
}

[[noreturn]] void early_export_failure()
{
    fail_closed("export called before bridge readiness", 0xE0111009);
}

std::wstring module_path(HMODULE module)
{
    std::vector<wchar_t> path(1024);
    for (;;) {
        const DWORD size = GetModuleFileNameW(
            module, path.data(), static_cast<DWORD>(path.size()));
        if (size == 0) return {};
        if (size < path.size() - 1) return std::wstring(path.data(), size);
        if (path.size() >= 32768) return {};
        path.resize(path.size() * 2);
    }
}

std::string hash_file(const std::wstring &path)
{
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE |
        FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return {};
    BCRYPT_ALG_HANDLE algorithm = nullptr; BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD object_size = 0, returned = 0; std::vector<unsigned char> object;
    std::array<unsigned char, 32> digest{}; std::vector<unsigned char> buffer(65536);
    bool ok = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0;
    if (ok) ok = BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
        reinterpret_cast<PUCHAR>(&object_size), sizeof(object_size), &returned, 0) >= 0;
    if (ok) { object.resize(object_size); ok = BCryptCreateHash(algorithm, &hash,
        object.data(), object_size, nullptr, 0, 0) >= 0; }
    while (ok) { DWORD read = 0; ok = ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr) != FALSE;
        if (!ok || read == 0) break; ok = BCryptHashData(hash, buffer.data(), read, 0) >= 0; }
    if (ok) ok = BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) >= 0;
    if (hash) BCryptDestroyHash(hash); if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0); CloseHandle(file);
    if (!ok) return {};
    constexpr char digits[] = "0123456789ABCDEF"; std::string text(64, '0');
    for (size_t i = 0; i < digest.size(); ++i) { text[i * 2] = digits[digest[i] >> 4]; text[i * 2 + 1] = digits[digest[i] & 15]; }
    return text;
}

bool image_range(HMODULE module, std::uintptr_t &base, std::uintptr_t &end)
{
    auto bytes = reinterpret_cast<const unsigned char *>(module);
    auto dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(bytes);
    if (!module || dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    auto nt = reinterpret_cast<const IMAGE_NT_HEADERS64 *>(bytes + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
    base = reinterpret_cast<std::uintptr_t>(module); end = base + nt->OptionalHeader.SizeOfImage; return true;
}

HMODULE find_geo11_module()
{
    HANDLE snapshot = CreateToolhelp32Snapshot(
        TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
    if (snapshot == INVALID_HANDLE_VALUE) return nullptr;
    MODULEENTRY32W entry{}; entry.dwSize = sizeof(entry);
    HMODULE found = nullptr;
    if (Module32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szModule, L"d3d11.dll") == 0) {
                if (hash_file(entry.szExePath) == geo_hash) {
                    found = entry.hModule; break;
                }
            }
        } while (Module32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot); return found;
}

create_fn raw_create()
{
    return mod_module ? *reinterpret_cast<create_fn *>(reinterpret_cast<std::uintptr_t>(mod_module) + 0x22FCA8) : nullptr;
}
release_fn raw_release()
{
    return mod_module ? *reinterpret_cast<release_fn *>(reinterpret_cast<std::uintptr_t>(mod_module) + 0x22FCA0) : nullptr;
}
void backend_log(const char *message) { log_line("%s", message); }
result backend_create_primary(context c, unsigned f, parameters p, handle *h) { return original_create(c, f, p, h); }
result backend_create_partner(context c, unsigned f, parameters p, handle *h)
{
    create_fn function = raw_create();
    log_line("partner create: calling raw=%p ctx=%p", function, c);
    const result status = function ? function(c, f, p, h) : fail_invalid_parameter;
    log_line("partner create: returned 0x%08X handle=%p", status,
        (h != nullptr) ? *h : nullptr);
    return status;
}
result backend_evaluate_primary(context c, handle h, parameters p, callback cb)
{
    return current_evaluator ? current_evaluator(c, h, p, cb) : fail_invalid_parameter;
}
result backend_evaluate_partner(context c, handle h, parameters p, callback cb)
{
    return current_partner_evaluator ? current_partner_evaluator(c, h, p, cb) : fail_invalid_parameter;
}
result backend_release_primary(handle h) { return original_release(h); }
result backend_release_partner(handle h)
{
    release_fn function = raw_release(); return function ? function(h) : fail_invalid_parameter;
}
result get_resource(parameters p, const char *n, resource *r) { return static_cast<ngx_parameter *>(p)->Get(n, reinterpret_cast<ID3D11Resource **>(r)); }
bool set_resource(parameters p, const char *n, resource r)
{
    static_cast<ngx_parameter *>(p)->Set(n, static_cast<ID3D11Resource *>(r));
    resource observed = nullptr; return get_resource(p, n, &observed) == success && observed == r;
}

result __cdecl create_hook(context c, unsigned f, parameters p, handle *h)
{
    bridge *instance = active_bridge.load(std::memory_order_acquire);
    if (!instance) { log_line("create_hook: no bridge"); return fail_invalid_parameter; }
    if (f != 1) return original_create(c, f, p, h);
    log_line("create_hook: feature=1 ctx=%p params=%p", c, p);
    const result status = instance->create_feature(c, f, p, h);
    log_line("create_hook: feature=1 result=0x%08X handle=%p pairs=%zu", status,
        (h != nullptr) ? *h : nullptr, instance->feature_count());
    return status;
}
result evaluate_common(evaluate_fn fn, context c, handle h, parameters p, callback cb)
{
    bridge *instance = active_bridge.load(std::memory_order_acquire);
    if (!instance) { log_line("evaluate_hook: no bridge"); return fail_invalid_parameter; }
    if (!instance->contains(h)) return fn(c, h, p, cb);
    log_line("evaluate_hook: handle=%p ctx=%p", h, c);
    current_evaluator = fn;
    const auto mod = reinterpret_cast<std::uintptr_t>(mod_module);
    const std::uintptr_t raw_offset = fn == original_evaluate_cpp ? 0x22FCB0 : 0x22FCB8;
    current_partner_evaluator = *reinterpret_cast<evaluate_fn *>(mod + raw_offset);
    result status = instance->evaluate_feature(c, h, p, cb);
    current_partner_evaluator = nullptr; current_evaluator = nullptr;
    if (status == fail_capture) {
        log_line("evaluate_hook: capture failed; falling back to normal mod evaluate");
        return fn(c, h, p, cb);
    }
    log_line("evaluate_hook: handle=%p result=0x%08X", h, status);
    return status;
}
result __cdecl evaluate_cpp_hook(context c, handle h, parameters p, callback cb) { return evaluate_common(original_evaluate_cpp, c, h, p, cb); }
result __cdecl evaluate_c_hook(context c, handle h, parameters p, callback cb) { return evaluate_common(original_evaluate_c, c, h, p, cb); }
result __cdecl release_hook(handle h)
{
    bridge *instance = active_bridge.load(std::memory_order_acquire);
    if (!instance) { log_line("release_hook: no bridge handle=%p", h); return fail_invalid_parameter; }
    const bool tracked = instance->contains(h);
    log_line("release_hook: handle=%p tracked=%d", h, tracked ? 1 : 0);
    return tracked ? instance->release_feature(h) : original_release(h);
}

bool load_forwarders()
{
    mod_module = LoadLibraryW((directory + mod_name).c_str()); if (!mod_module) return false;
    struct item { const char *name; void **target; } items[] = {
        {"DllCanUnloadNow", &g_DllCanUnloadNow}, {"DllGetClassObject", &g_DllGetClassObject},
        {"DllRegisterServer", &g_DllRegisterServer}, {"DllUnregisterServer", &g_DllUnregisterServer},
        {"GetdfDIJoystick", &g_GetdfDIJoystick}, {"WH3DLSS_AcquirePostDepthV2", &g_WH3DLSS_AcquirePostDepthV2},
        {"WH3DLSS_EndPostDepthFrameV2", &g_WH3DLSS_EndPostDepthFrameV2}};
    direct_input_original = reinterpret_cast<direct_input_fn>(GetProcAddress(mod_module, "DirectInput8Create"));
    if (!direct_input_original) return false;
    for (auto &item : items) { *item.target = reinterpret_cast<void *>(GetProcAddress(mod_module, item.name)); if (!*item.target) return false; }
    return true;
}

bool install(void *target, void *replacement, void **original) { return MH_CreateHook(target, replacement, original) == MH_OK; }

bool prologue(std::uintptr_t address, const unsigned char *expected, std::size_t size)
{
    MEMORY_BASIC_INFORMATION memory{};
    const void *pointer = reinterpret_cast<const void *>(address);
    return VirtualQuery(pointer, &memory, sizeof(memory)) == sizeof(memory) &&
        memory.State == MEM_COMMIT && (memory.Protect & (PAGE_NOACCESS | PAGE_GUARD)) == 0 &&
        std::memcmp(pointer, expected, size) == 0;
}

DWORD WINAPI initialize(void *)
{
    if (hash_file(directory + mod_name) != mod_hash) fail_closed("mod hash", 0xE0111001);
    if (hash_file(directory + L"Warhammer3.exe") != game_hash) fail_closed("game hash", 0xE011100B);
    if (hash_file(directory + L"nvngx_dlss.dll") != dlss_hash) fail_closed("DLSS hash", 0xE011100C);
    if (!load_forwarders()) fail_closed("chained mod load", 0xE011100A);
    if (MH_Initialize() != MH_OK) fail_closed("MinHook", 0xE0111004);
    auto mod = reinterpret_cast<std::uintptr_t>(mod_module);
    const unsigned char create_bytes[] = {0x4C,0x8B,0xDC,0x53,0x48,0x81,0xEC,0xD0,0x00,0x00,0x00};
    const unsigned char evaluate_cpp_bytes[] = {0x48,0x83,0xEC,0x38,0x4C,0x89,0x4C,0x24,0x28};
    const unsigned char evaluate_c_bytes[] = {0x48,0x83,0xEC,0x38,0x4C,0x89,0x4C,0x24,0x28};
    const unsigned char release_bytes[] = {0x40,0x53,0x48,0x83,0xEC,0x20,0x48,0x8B,0xD9};
    if (!prologue(mod + 0xC120, create_bytes, sizeof(create_bytes)) ||
        !prologue(mod + 0xC380, evaluate_cpp_bytes, sizeof(evaluate_cpp_bytes)) ||
        !prologue(mod + 0xC3B0, evaluate_c_bytes, sizeof(evaluate_c_bytes)) ||
        !prologue(mod + 0xC620, release_bytes, sizeof(release_bytes)))
        fail_closed("mod prologue guard", 0xE0111005);
    if (!install(reinterpret_cast<void *>(mod + 0xC120), reinterpret_cast<void *>(&create_hook), reinterpret_cast<void **>(&original_create)) ||
        !install(reinterpret_cast<void *>(mod + 0xC380), reinterpret_cast<void *>(&evaluate_cpp_hook), reinterpret_cast<void **>(&original_evaluate_cpp)) ||
        !install(reinterpret_cast<void *>(mod + 0xC3B0), reinterpret_cast<void *>(&evaluate_c_hook), reinterpret_cast<void **>(&original_evaluate_c)) ||
        !install(reinterpret_cast<void *>(mod + 0xC620), reinterpret_cast<void *>(&release_hook), reinterpret_cast<void **>(&original_release))) fail_closed("create hooks", 0xE0111006);
    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) fail_closed("enable hooks", 0xE0111007);
    for (unsigned i = 0; i < 1500 && !geo_module; ++i) { geo_module = find_geo11_module(); if (!geo_module) Sleep(10); }
    if (!geo_module) fail_closed("geo hash", 0xE0111002);
    std::uintptr_t base = 0, end = 0; if (!image_range(geo_module, base, end)) fail_closed("geo image", 0xE0111003);
    geo11::configure({base, end});
    geo11::set_logger(backend_log);
    stereo_bridge = std::make_unique<bridge>(api{backend_create_primary, backend_create_partner,
        backend_evaluate_primary, backend_evaluate_partner,
        backend_release_primary, backend_release_partner, backend_release_partner,
        get_resource, set_resource, geo11::unwrap_context, geo11::unwrap_resource,
        backend_log});
    active_bridge.store(stereo_bridge.get(), std::memory_order_release);
    SetEvent(ready_event);
    log_line("ready"); return 0;
}
}

extern "C" HRESULT WINAPI DirectInput8Create_bridge(HINSTANCE i, DWORD v, REFIID id, LPVOID *o, LPUNKNOWN u)
{
    if (WaitForSingleObject(ready_event, 30000) != WAIT_OBJECT_0) fail_closed("readiness", 0xE0111008);
    return direct_input_original(i, v, id, o, u);
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason != DLL_PROCESS_ATTACH) return TRUE; DisableThreadLibraryCalls(module);
    auto path = module_path(module); auto separator = path.find_last_of(L"\\/"); if (separator == std::wstring::npos) return FALSE;
    directory = path.substr(0, separator + 1);
    ready_event = CreateEventW(nullptr, TRUE, FALSE, nullptr); if (!ready_event) return FALSE;
    HANDLE thread = CreateThread(nullptr, 0, initialize, nullptr, 0, nullptr); if (!thread) return FALSE; CloseHandle(thread); return TRUE;
}
