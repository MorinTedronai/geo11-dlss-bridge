#pragma once

#include <cstddef>
#include <cstdint>

// Private ABI for exactly this geo-11 x64 binary:
// SHA-256 17374B9D279BD1B1AD3B6BAB67A7690B6896160F46AAFEB811921A2D38535D94
namespace geo11_20220726_x64
{
constexpr char dll_sha256[] =
    "17374B9D279BD1B1AD3B6BAB67A7690B6896160F46AAFEB811921A2D38535D94";

constexpr std::uintptr_t texture2d_vtable_rva = 0x6A1A60;
constexpr std::uintptr_t resource_vtable_rva = 0x6A1AC0;
constexpr std::uintptr_t direct_mode_context_vtable_rva = 0x68D2A0;

constexpr std::size_t texture2d_object_size = 0x2E0;
constexpr std::size_t resource_subobject_offset = 0x08;
constexpr std::size_t stereoized_offset = 0x14;
constexpr std::size_t primary_resource_offset = 0x140;
constexpr std::size_t partner_resource_offset = 0x148;
constexpr std::size_t wrapped_texture_offset = 0x190;
constexpr std::size_t partner_texture_offset = 0x198;
constexpr std::size_t texture_desc_offset = 0x1A0;
constexpr std::size_t texture_desc_size = 44;

constexpr std::size_t context_device_offset = 0x08;
constexpr std::size_t context_pass_through_offset = 0x10;
constexpr std::size_t context_possibly_hooked_offset = 0x18;
constexpr std::size_t context_hacker_device_offset = 0x20;

// Anatomical eye assignment is deliberately not encoded. The binary evidence
// establishes a stable primary/original and stereo-partner pair only.
struct texture2d_prefix
{
    void **texture2d_vtable;                         // +0x000
    void **resource_vtable;                          // +0x008
    std::uint8_t unknown_010[stereoized_offset - 0x10];
    std::uint8_t stereoized;                         // +0x014
    std::uint8_t unknown_015[primary_resource_offset - 0x15];
    void *primary_resource;                          // +0x140
    void *partner_resource;                          // +0x148
    std::uint8_t unknown_150[wrapped_texture_offset - 0x150];
    void *wrapped_texture;                           // +0x190
    void *partner_texture;                           // +0x198
    std::uint8_t texture_desc[texture_desc_size];    // +0x1A0
};

static_assert(offsetof(texture2d_prefix, stereoized) == stereoized_offset);
static_assert(offsetof(texture2d_prefix, primary_resource) == primary_resource_offset);
static_assert(offsetof(texture2d_prefix, partner_resource) == partner_resource_offset);
static_assert(offsetof(texture2d_prefix, wrapped_texture) == wrapped_texture_offset);
static_assert(offsetof(texture2d_prefix, partner_texture) == partner_texture_offset);
static_assert(offsetof(texture2d_prefix, texture_desc) == texture_desc_offset);
} // namespace geo11_20220726_x64
