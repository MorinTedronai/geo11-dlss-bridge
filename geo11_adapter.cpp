#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>

#include "geo11_adapter.h"
#include "abi/geo11_resource_abi.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <unordered_map>

namespace stereo_ngx::geo11
{
namespace
{
layout g_layout{};
void (*g_logger)(const char *) = nullptr;

struct array_pair
{
    ID3D11Texture2D *source = nullptr;
    ID3D11Texture2D *eyes[2]{};
    D3D11_TEXTURE2D_DESC source_desc{};
};

std::mutex g_array_mutex;
std::unordered_map<resource, array_pair> g_array_pairs;

void logf(const char *format, ...)
{
    if (g_logger == nullptr)
        return;
    char buffer[320]{};
    va_list arguments;
    va_start(arguments, format);
    std::vsnprintf(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);
    g_logger(buffer);
}

bool readable(const void *pointer, std::size_t size)
{
    if (pointer == nullptr)
        return false;
    MEMORY_BASIC_INFORMATION memory{};
    if (VirtualQuery(pointer, &memory, sizeof(memory)) != sizeof(memory) ||
        memory.State != MEM_COMMIT || (memory.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0)
        return false;
    const auto start = reinterpret_cast<std::uintptr_t>(pointer);
    const auto end = reinterpret_cast<std::uintptr_t>(memory.BaseAddress) + memory.RegionSize;
    return start <= end && size <= end - start;
}

bool array_eye_pair(context pass_through, resource wrapped, resource primary,
    resource_pair *pair)
{
    if (pass_through == nullptr || primary == nullptr || pair == nullptr)
        return false;

    auto *source_resource = static_cast<ID3D11Resource *>(primary);
    ID3D11Texture2D *source = nullptr;
    if (FAILED(source_resource->QueryInterface(
            __uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&source))))
        return false;

    D3D11_TEXTURE2D_DESC source_desc{};
    source->GetDesc(&source_desc);
    if (source_desc.ArraySize < 2 || source_desc.Usage != D3D11_USAGE_DEFAULT) {
        source->Release();
        logf("res: array source unusable wrapped=%p array=%u usage=%u",
            wrapped, source_desc.ArraySize, source_desc.Usage);
        return false;
    }

    auto *context = static_cast<ID3D11DeviceContext *>(pass_through);
    ID3D11Device *device = nullptr;
    context->GetDevice(&device);
    if (device == nullptr) {
        source->Release();
        return false;
    }

    std::lock_guard lock(g_array_mutex);
    auto found = g_array_pairs.find(primary);
    if (found == g_array_pairs.end()) {
        D3D11_TEXTURE2D_DESC eye_desc = source_desc;
        eye_desc.ArraySize = 1;
        array_pair created{};
        created.source = source;
        created.source_desc = source_desc;
        if (FAILED(device->CreateTexture2D(&eye_desc, nullptr, &created.eyes[0])) ||
            FAILED(device->CreateTexture2D(&eye_desc, nullptr, &created.eyes[1]))) {
            if (created.eyes[0]) created.eyes[0]->Release();
            if (created.eyes[1]) created.eyes[1]->Release();
            created.source->Release();
            device->Release();
            logf("res: eye texture creation failed wrapped=%p", wrapped);
            return false;
        }
        found = g_array_pairs.emplace(primary, created).first;
        logf("res: array eye pair created wrapped=%p source=%p left=%p right=%p array=%u",
            wrapped, primary, created.eyes[0], created.eyes[1], source_desc.ArraySize);
    } else {
        source->Release();
        if (std::memcmp(&found->second.source_desc, &source_desc,
                sizeof(source_desc)) != 0) {
            device->Release();
            logf("res: cached array descriptor changed wrapped=%p", wrapped);
            return false;
        }
    }

    array_pair &cached = found->second;
    for (UINT eye = 0; eye < 2; ++eye) {
        for (UINT mip = 0; mip < source_desc.MipLevels; ++mip) {
            const UINT source_subresource = D3D11CalcSubresource(
                mip, eye, source_desc.MipLevels);
            context->CopySubresourceRegion(cached.eyes[eye], mip, 0, 0, 0,
                cached.source, source_subresource, nullptr);
        }
    }
    device->Release();
    pair->primary = cached.eyes[0];
    pair->partner = cached.eyes[1];
    return true;
}
} // namespace

void configure(layout value)
{
    g_layout = value;
}

void set_logger(void (*logger)(const char *message))
{
    g_logger = logger;
}

bool unwrap_context(context wrapped, context *pass_through)
{
    if (pass_through == nullptr || !readable(wrapped, 0x28)) {
        logf("ctx: unreadable wrapped=%p", wrapped);
        return false;
    }
    const auto object = reinterpret_cast<std::uintptr_t>(wrapped);
    const auto vtable = *reinterpret_cast<const std::uintptr_t *>(object);
    const std::uintptr_t expected =
        g_layout.module_base + geo11_20220726_x64::direct_mode_context_vtable_rva;
    if (vtable != expected) {
        logf("ctx: vtable mismatch got=0x%llX expected=0x%llX",
            static_cast<unsigned long long>(vtable),
            static_cast<unsigned long long>(expected));
        return false;
    }
    auto value = *reinterpret_cast<context *>(
        object + geo11_20220726_x64::context_pass_through_offset);
    if (!readable(value, sizeof(void *))) {
        logf("ctx: pass-through unreadable %p", value);
        return false;
    }
    *pass_through = value;
    return true;
}

bool unwrap_resource(context pass_through, resource wrapped, resource_pair *pair)
{
    using namespace geo11_20220726_x64;
    if (pair == nullptr || !readable(wrapped, texture_desc_offset + texture_desc_size)) {
        logf("res: unreadable wrapped=%p", wrapped);
        return false;
    }
    const auto object = reinterpret_cast<std::uintptr_t>(wrapped);
    const auto texture_vtable = *reinterpret_cast<const std::uintptr_t *>(object);
    const auto resource_vtable = *reinterpret_cast<const std::uintptr_t *>(
        object + resource_subobject_offset);
    const std::uintptr_t expected_texture = g_layout.module_base + texture2d_vtable_rva;
    const std::uintptr_t expected_resource = g_layout.module_base + resource_vtable_rva;
    if (texture_vtable != expected_texture) {
        logf("res: texture vtable mismatch wrapped=%p got=0x%llX expected=0x%llX",
            wrapped, static_cast<unsigned long long>(texture_vtable),
            static_cast<unsigned long long>(expected_texture));
        return false;
    }
    if (resource_vtable != expected_resource) {
        logf("res: resource vtable mismatch wrapped=%p got=0x%llX expected=0x%llX",
            wrapped, static_cast<unsigned long long>(resource_vtable),
            static_cast<unsigned long long>(expected_resource));
        return false;
    }
    const auto stereoized = *reinterpret_cast<const std::uint8_t *>(object + stereoized_offset);
    if (stereoized == 0) {
        logf("res: not stereoized wrapped=%p", wrapped);
        return false;
    }

    D3D11_TEXTURE2D_DESC public_desc{};
    ID3D11Texture2D *texture = nullptr;
    if (SUCCEEDED(static_cast<ID3D11Resource *>(wrapped)->QueryInterface(
            __uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&texture)))) {
        texture->GetDesc(&public_desc);
        texture->Release();
    }
    logf("res: desc wrapped=%p %ux%u fmt=%u mips=%u array=%u samples=%u/%u usage=%u bind=0x%X misc=0x%X stereo=0x%02X mode=%u",
        wrapped, public_desc.Width, public_desc.Height,
        static_cast<unsigned>(public_desc.Format), public_desc.MipLevels,
        public_desc.ArraySize, public_desc.SampleDesc.Count,
        public_desc.SampleDesc.Quality, public_desc.Usage, public_desc.BindFlags,
        public_desc.MiscFlags, stereoized,
        *reinterpret_cast<const unsigned *>(object + 0x18));

    logf("res: fields wrapped=%p stz=%02X u16=%02X mode=%u dev=%p cnt=%u a=%u b=%u c=%u f138=%02X f190=%02X f1D0=%p f1D8=%p",
        wrapped,
        *reinterpret_cast<const unsigned char *>(object + 0x14),
        *reinterpret_cast<const unsigned char *>(object + 0x16),
        *reinterpret_cast<const unsigned *>(object + 0x18),
        *reinterpret_cast<void *const *>(object + 0x20),
        *reinterpret_cast<const unsigned *>(object + 0x28),
        *reinterpret_cast<const unsigned *>(object + 0x2C),
        *reinterpret_cast<const unsigned *>(object + 0x30),
        *reinterpret_cast<const unsigned *>(object + 0x34),
        *reinterpret_cast<const unsigned char *>(object + 0x138),
        *reinterpret_cast<const unsigned char *>(object + 0x190),
        *reinterpret_cast<void *const *>(object + 0x1D0),
        *reinterpret_cast<void *const *>(object + 0x1D8));

    const auto primary = *reinterpret_cast<resource *>(object + primary_resource_offset);
    const auto partner = *reinterpret_cast<resource *>(object + partner_resource_offset);
    const auto texture_primary = *reinterpret_cast<resource *>(object + wrapped_texture_offset);
    const auto texture_partner = *reinterpret_cast<resource *>(object + partner_texture_offset);
    const auto mode = *reinterpret_cast<const unsigned *>(object + 0x18);
    if (mode == 0 && primary != nullptr && primary == texture_primary &&
        partner == nullptr && texture_partner == nullptr) {
        const bool extracted = array_eye_pair(
            pass_through, wrapped, primary, pair);
        logf("res: array unwrap wrapped=%p source=%p result=%d left=%p right=%p",
            wrapped, primary, extracted ? 1 : 0,
            extracted ? pair->primary : nullptr,
            extracted ? pair->partner : nullptr);
        return extracted;
    }
    if (primary == nullptr || partner == nullptr || primary == partner ||
        primary != texture_primary || partner != texture_partner) {
        logf("res: pair mismatch wrapped=%p primary=%p partner=%p texPrimary=%p texPartner=%p",
            wrapped, primary, partner, texture_primary, texture_partner);
        return false;
    }
    if (!readable(primary, sizeof(void *)) || !readable(partner, sizeof(void *))) {
        logf("res: pair unreadable wrapped=%p primary=%p partner=%p", wrapped, primary, partner);
        return false;
    }

    if (std::memcmp(&public_desc,
            reinterpret_cast<const void *>(object + texture_desc_offset),
            sizeof(public_desc)) != 0) {
        logf("res: descriptor mismatch wrapped=%p public=%ux%u fmt=%u cached=%ux%u fmt=%u",
            wrapped, public_desc.Width, public_desc.Height,
            static_cast<unsigned>(public_desc.Format),
            *reinterpret_cast<const unsigned *>(object + texture_desc_offset),
            *reinterpret_cast<const unsigned *>(object + texture_desc_offset + 4),
            *reinterpret_cast<const unsigned *>(object + texture_desc_offset + 16));
        return false;
    }

    pair->primary = primary;
    pair->partner = partner;
    return true;
}
} // namespace stereo_ngx::geo11
