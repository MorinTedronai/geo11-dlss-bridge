#pragma once

#include <array>
#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace stereo_ngx
{
using result = std::uint32_t;
using handle = void *;
using context = void *;
using parameters = void *;
using resource = void *;
using callback = void *;

constexpr result success = 0x00000001;
constexpr result fail_invalid_parameter = 0xBAD00005;
constexpr result fail_feature_not_found = 0xBAD00004;
constexpr result fail_feature_already_exists = 0xBAD00003;
constexpr result fail_capture = 0xBAD0000F; // nothing evaluated; safe to fall back
inline constexpr std::array<const char *, 4> required_resources = {
    "Color", "Output", "Depth", "MotionVectors"
};

struct resource_pair
{
    resource primary = nullptr;
    resource partner = nullptr;
};

struct api
{
    result (*create_primary)(context, unsigned feature, parameters, handle *);
    result (*create_partner)(context, unsigned feature, parameters, handle *);
    result (*evaluate_primary)(context, handle, parameters, callback);
    result (*evaluate_partner)(context, handle, parameters, callback);
    result (*release_primary)(handle);
    result (*release_partner)(handle);
    result (*release_unregistered)(handle);
    result (*get_resource)(parameters, const char *, resource *);
    bool (*set_resource)(parameters, const char *, resource);
    bool (*unwrap_context)(context wrapped, context *pass_through);
    bool (*unwrap_resource)(context pass_through, resource wrapped, resource_pair *pair);
    void (*log)(const char *message);
};

class bridge
{
public:
    explicit bridge(api functions);

    result create_feature(context wrapped_context, unsigned feature,
        parameters params, handle *game_handle);
    result evaluate_feature(context wrapped_context, handle game_handle,
        parameters params, callback progress);
    result release_feature(handle game_handle);

    std::size_t feature_count() const;
    bool contains(handle game_handle) const;

private:
    struct feature_pair
    {
        handle primary = nullptr;
        handle partner = nullptr;
        context wrapped = nullptr;
        context pass_through = nullptr;
    };

    api functions_;
    mutable std::mutex mutex_;
    std::unordered_map<handle, feature_pair> features_;
};
} // namespace stereo_ngx
