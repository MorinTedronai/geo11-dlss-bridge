#include "bridge_core.h"

#include <cstdarg>
#include <cstdio>
#include <utility>

namespace stereo_ngx
{
namespace
{
void logf(const api &functions, const char *format, ...)
{
    if (functions.log == nullptr)
        return;
    char buffer[320]{};
    va_list arguments;
    va_start(arguments, format);
    std::vsnprintf(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);
    functions.log(buffer);
}

struct parameter_resources
{
    parameters params = nullptr;
    const api *functions = nullptr;
    context pass_through = nullptr;
    std::array<resource, required_resources.size()> wrapped{};
    std::array<resource_pair, required_resources.size()> pairs{};
    bool captured = false;

    bool capture()
    {
        bool ok = true;
        for (std::size_t index = 0; index < wrapped.size(); ++index) {
            wrapped[index] = nullptr;
            const result got = functions->get_resource(
                params, required_resources[index], &wrapped[index]);
            if (got != success || wrapped[index] == nullptr) {
                logf(*functions, "capture: key=%s get=0x%08X wrapped=%p",
                    required_resources[index], got, wrapped[index]);
                ok = false;
                continue;
            }
            resource_pair pair{};
            const bool unwrapped = functions->unwrap_resource(
                pass_through, wrapped[index], &pair);
            logf(*functions, "capture: key=%s wrapped=%p unwrap=%d primary=%p partner=%p",
                required_resources[index], wrapped[index], unwrapped ? 1 : 0,
                pair.primary, pair.partner);
            if (!unwrapped || pair.primary == nullptr || pair.partner == nullptr ||
                pair.primary == pair.partner) {
                ok = false;
                continue;
            }
            pairs[index] = pair;
        }
        captured = ok;
        return ok;
    }

    bool select(bool partner)
    {
        for (std::size_t index = 0; index < wrapped.size(); ++index)
            if (!functions->set_resource(params, required_resources[index],
                    partner ? pairs[index].partner : pairs[index].primary))
                return false;
        return true;
    }

    bool restore()
    {
        if (!captured)
            return true;
        bool restored = true;
        for (std::size_t index = 0; index < wrapped.size(); ++index)
            restored = functions->set_resource(
                params, required_resources[index], wrapped[index]) && restored;
        captured = false;
        return restored;
    }

    ~parameter_resources()
    {
        restore();
    }
};
} // namespace

bridge::bridge(api functions) : functions_(functions)
{
}

result bridge::create_feature(context wrapped_context, unsigned feature,
    parameters params, handle *game_handle)
{
    if (wrapped_context == nullptr || params == nullptr || game_handle == nullptr ||
        functions_.create_primary == nullptr || functions_.create_partner == nullptr ||
        functions_.release_primary == nullptr || functions_.release_partner == nullptr ||
        functions_.unwrap_context == nullptr)
        return fail_invalid_parameter;

    context pass_through = nullptr;
    if (!functions_.unwrap_context(wrapped_context, &pass_through) || pass_through == nullptr) {
        logf(functions_, "create: unwrap_context failed wrapped=%p", wrapped_context);
        return fail_invalid_parameter;
    }

    handle primary = nullptr;
    result status = functions_.create_primary(wrapped_context, feature, params, &primary);
    logf(functions_, "create: primary feature=%u ctx=%p status=0x%08X handle=%p",
        feature, wrapped_context, status, primary);
    if (status != success || primary == nullptr) {
        *game_handle = nullptr;
        return status != success ? status : fail_invalid_parameter;
    }

    handle partner = nullptr;
    status = functions_.create_partner(wrapped_context, feature, params, &partner);
    logf(functions_, "create: partner feature=%u ctx=%p status=0x%08X handle=%p",
        feature, wrapped_context, status, partner);
    if (status != success || partner == nullptr || partner == primary) {
        // Keep the game-facing primary feature alive. It is owned by mod 393 and
        // must be released through the normal game release path. Do not track it
        // here, so evaluate/release pass through unchanged.
        logf(functions_, "create: partner unusable (0x%08X); keeping primary only",
            status);
        *game_handle = primary;
        return success;
    }

    {
        std::lock_guard lock(mutex_);
        if (features_.find(primary) != features_.end()) {
            functions_.release_unregistered(partner);
            functions_.release_unregistered(primary);
            *game_handle = nullptr;
            return fail_invalid_parameter;
        }
        features_.emplace(primary, feature_pair{
            primary, partner, wrapped_context, pass_through});
    }

    logf(functions_, "create: stereo pair ready primary=%p partner=%p pairs=%zu",
        primary, partner, feature_count());
    *game_handle = primary;
    return success;
}

result bridge::evaluate_feature(context wrapped_context, handle game_handle,
    parameters params, callback progress)
{
    if (wrapped_context == nullptr || game_handle == nullptr || params == nullptr ||
        functions_.evaluate_primary == nullptr || functions_.evaluate_partner == nullptr ||
        functions_.get_resource == nullptr ||
        functions_.set_resource == nullptr || functions_.unwrap_resource == nullptr)
        return fail_invalid_parameter;

    std::lock_guard lock(mutex_);
    const auto found = features_.find(game_handle);
    if (found == features_.end())
        return fail_feature_not_found;
    const feature_pair &feature = found->second;

    if (wrapped_context != feature.wrapped) {
        logf(functions_, "evaluate: context mismatch handle=%p got=%p expected=%p",
            game_handle, wrapped_context, feature.wrapped);
        return fail_invalid_parameter;
    }

    parameter_resources resources{params, &functions_, feature.pass_through};
    if (!resources.capture()) {
        logf(functions_, "evaluate: resource capture failed handle=%p", game_handle);
        return fail_capture;
    }

    if (!resources.select(false)) {
        resources.restore();
        return fail_capture;
    }
    result status = functions_.evaluate_primary(
        feature.wrapped, feature.primary, params, progress);
    if (status != success) {
        logf(functions_, "evaluate: primary failed handle=%p status=0x%08X",
            feature.primary, status);
        if (!resources.restore())
            return fail_invalid_parameter;
        return status;
    }

    if (!resources.select(true)) {
        resources.restore();
        return fail_invalid_parameter;
    }
    status = functions_.evaluate_partner(
        feature.wrapped, feature.partner, params, nullptr);
    if (status != success)
        logf(functions_, "evaluate: partner failed handle=%p status=0x%08X",
            feature.partner, status);
    if (!resources.restore())
        return fail_invalid_parameter;
    return status;
}

result bridge::release_feature(handle game_handle)
{
    if (game_handle == nullptr || functions_.release_primary == nullptr ||
        functions_.release_partner == nullptr)
        return fail_invalid_parameter;

    feature_pair feature{};
    {
        std::lock_guard lock(mutex_);
        const auto found = features_.find(game_handle);
        if (found == features_.end())
            return fail_feature_not_found;
        feature = found->second;
        features_.erase(found);
    }

    const result partner_status = functions_.release_partner(feature.partner);
    const result primary_status = functions_.release_primary(feature.primary);
    logf(functions_, "release: primary=%p/0x%08X partner=%p/0x%08X pairs=%zu",
        feature.primary, primary_status, feature.partner, partner_status,
        feature_count());
    return primary_status != success ? primary_status : partner_status;
}

std::size_t bridge::feature_count() const
{
    std::lock_guard lock(mutex_);
    return features_.size();
}

bool bridge::contains(handle game_handle) const
{
    std::lock_guard lock(mutex_);
    return features_.find(game_handle) != features_.end();
}
} // namespace stereo_ngx
