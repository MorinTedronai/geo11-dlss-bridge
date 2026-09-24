#include "bridge_core.h"

#include <array>
#include <cassert>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace stereo_ngx;

namespace
{
struct fake_parameters
{
    std::unordered_map<std::string, resource> values;
};

struct state
{
    int next_handle = 100;
    int create_calls = 0;
    int fail_create_call = 0;
    int evaluate_calls = 0;
    int fail_evaluate_call = 0;
    bool duplicate_second_handle = false;
    int fail_set_call = 0;
    int set_calls = 0;
    std::vector<handle> released;
    std::vector<handle> unregistered_released;
    std::vector<handle> evaluated_handles;
    std::vector<context> evaluated_contexts;
    std::vector<callback> evaluated_callbacks;
    std::vector<std::array<resource, 4>> evaluated_resources;
    std::vector<context> created_contexts;
    std::unordered_map<resource, resource_pair> pairs;
    context wrapped_context = reinterpret_cast<context>(0x1111);
    context pass_through = reinterpret_cast<context>(0x2222);
} g;

result create(context used_context, unsigned, parameters, handle *output)
{
    g.created_contexts.push_back(used_context);
    ++g.create_calls;
    if (g.fail_create_call == g.create_calls) {
        *output = nullptr;
        return 0xBAD0000B;
    }
    if (g.duplicate_second_handle && g.create_calls == 2) {
        *output = reinterpret_cast<handle>(100);
        return success;
    }
    *output = reinterpret_cast<handle>(static_cast<std::uintptr_t>(g.next_handle++));
    return success;
}

result evaluate(context used_context, handle used_handle, parameters params, callback progress)
{
    ++g.evaluate_calls;
    g.evaluated_handles.push_back(used_handle);
    g.evaluated_contexts.push_back(used_context);
    g.evaluated_callbacks.push_back(progress);
    auto *fake = static_cast<fake_parameters *>(params);
    std::array<resource, 4> snapshot{};
    for (std::size_t index = 0; index < required_resources.size(); ++index)
        snapshot[index] = fake->values[required_resources[index]];
    g.evaluated_resources.push_back(snapshot);
    return g.fail_evaluate_call == g.evaluate_calls ? 0xBAD00002 : success;
}

result release(handle value)
{
    g.released.push_back(value);
    return success;
}

result release_unregistered(handle value)
{
    g.unregistered_released.push_back(value);
    return success;
}

result get_resource(parameters params, const char *name, resource *output)
{
    auto *fake = static_cast<fake_parameters *>(params);
    const auto found = fake->values.find(name);
    if (found == fake->values.end())
        return fail_invalid_parameter;
    *output = found->second;
    return success;
}

bool set_resource(parameters params, const char *name, resource value)
{
    ++g.set_calls;
    if (g.fail_set_call == g.set_calls)
        return false;
    static_cast<fake_parameters *>(params)->values[name] = value;
    return true;
}

bool unwrap_context(context wrapped, context *output)
{
    if (wrapped != g.wrapped_context)
        return false;
    *output = g.pass_through;
    return true;
}

bool unwrap_resource(context, resource wrapped, resource_pair *output)
{
    const auto found = g.pairs.find(wrapped);
    if (found == g.pairs.end())
        return false;
    *output = found->second;
    return true;
}

void log_message(const char *)
{
}

api functions()
{
    return {create, create, evaluate, evaluate, release, release, release_unregistered,
        get_resource, set_resource, unwrap_context, unwrap_resource, log_message};
}

fake_parameters make_parameters(bool include_partner = true)
{
    fake_parameters params;
    for (std::size_t index = 0; index < required_resources.size(); ++index) {
        const auto wrapped = reinterpret_cast<resource>(0x1000 + index);
        const auto primary = reinterpret_cast<resource>(0x2000 + index);
        const auto partner = include_partner
            ? reinterpret_cast<resource>(0x3000 + index)
            : nullptr;
        params.values[required_resources[index]] = wrapped;
        g.pairs[wrapped] = {primary, partner};
    }
    return params;
}

void reset()
{
    g = {};
    g.next_handle = 100;
    g.wrapped_context = reinterpret_cast<context>(0x1111);
    g.pass_through = reinterpret_cast<context>(0x2222);
}

void assert_wrapped_restored(const fake_parameters &params)
{
    for (std::size_t index = 0; index < required_resources.size(); ++index)
        assert(params.values.at(required_resources[index]) ==
            reinterpret_cast<resource>(0x1000 + index));
}

void test_success()
{
    reset();
    bridge instance(functions());
    auto params = make_parameters();
    handle game = nullptr;
    assert(instance.create_feature(g.wrapped_context, 1, &params, &game) == success);
    assert(game == reinterpret_cast<handle>(100));
    assert(instance.feature_count() == 1);
    assert((g.created_contexts ==
        std::vector<context>{g.wrapped_context, g.wrapped_context}));

    callback progress = reinterpret_cast<callback>(0x4444);
    assert(instance.evaluate_feature(g.wrapped_context, game, &params, progress) == success);
    assert(g.evaluate_calls == 2);
    assert(g.evaluated_handles[0] == reinterpret_cast<handle>(100));
    assert(g.evaluated_handles[1] == reinterpret_cast<handle>(101));
    assert(g.evaluated_contexts[0] == g.wrapped_context);
    assert(g.evaluated_contexts[1] == g.wrapped_context);
    assert(g.evaluated_callbacks[0] == progress);
    assert(g.evaluated_callbacks[1] == nullptr);
    for (std::size_t index = 0; index < required_resources.size(); ++index) {
        assert(g.evaluated_resources[0][index] == reinterpret_cast<resource>(0x2000 + index));
        assert(g.evaluated_resources[1][index] == reinterpret_cast<resource>(0x3000 + index));
    }
    assert_wrapped_restored(params);
    assert(instance.release_feature(game) == success);
    assert((g.released == std::vector<handle>{
        reinterpret_cast<handle>(101), reinterpret_cast<handle>(100)}));
    assert(instance.feature_count() == 0);
}

void test_partner_failure_keeps_primary()
{
    reset();
    g.fail_create_call = 2;
    bridge instance(functions());
    auto params = make_parameters();
    handle game = nullptr;
    assert(instance.create_feature(g.wrapped_context, 1, &params, &game) == success);
    assert(game == reinterpret_cast<handle>(100));
    assert(instance.feature_count() == 0);
    assert(g.unregistered_released.empty());
    assert(g.released.empty());
}

void test_primary_failure()
{
    reset();
    g.fail_create_call = 1;
    bridge instance(functions());
    auto params = make_parameters();
    handle game = reinterpret_cast<handle>(7);
    assert(instance.create_feature(g.wrapped_context, 1, &params, &game) != success);
    assert(game == nullptr);
    assert(instance.feature_count() == 0);
}

void test_missing_partner()
{
    reset();
    bridge instance(functions());
    auto params = make_parameters(false);
    handle game = nullptr;
    assert(instance.create_feature(g.wrapped_context, 1, &params, &game) == success);
    assert(instance.evaluate_feature(g.wrapped_context, game, &params, nullptr) == fail_capture);
    assert(g.evaluate_calls == 0);
    assert_wrapped_restored(params);
}

void test_primary_evaluate_failure_restores()
{
    reset();
    g.fail_evaluate_call = 1;
    bridge instance(functions());
    auto params = make_parameters();
    handle game = nullptr;
    assert(instance.create_feature(g.wrapped_context, 1, &params, &game) == success);
    assert(instance.evaluate_feature(g.wrapped_context, game, &params, nullptr) != success);
    assert(g.evaluate_calls == 1);
    assert_wrapped_restored(params);
}

void test_partner_evaluate_failure_restores()
{
    reset();
    g.fail_evaluate_call = 2;
    bridge instance(functions());
    auto params = make_parameters();
    handle game = nullptr;
    assert(instance.create_feature(g.wrapped_context, 1, &params, &game) == success);
    assert(instance.evaluate_feature(g.wrapped_context, game, &params, nullptr) != success);
    assert(g.evaluate_calls == 2);
    assert_wrapped_restored(params);
}

void test_context_mismatch()
{
    reset();
    bridge instance(functions());
    auto params = make_parameters();
    handle game = nullptr;
    assert(instance.create_feature(g.wrapped_context, 1, &params, &game) == success);
    assert(instance.evaluate_feature(reinterpret_cast<context>(0x9999), game,
        &params, nullptr) == fail_invalid_parameter);
    assert(g.evaluate_calls == 0);
}

void test_parameter_write_failure()
{
    reset();
    bridge instance(functions());
    auto params = make_parameters();
    handle game = nullptr;
    assert(instance.create_feature(g.wrapped_context, 1, &params, &game) == success);
    g.fail_set_call = 2;
    assert(instance.evaluate_feature(g.wrapped_context, game, &params, nullptr) ==
        fail_capture);
    assert(g.evaluate_calls == 0);
    assert_wrapped_restored(params);
}

void test_stress()
{
    reset();
    bridge instance(functions());
    for (int iteration = 0; iteration < 1000; ++iteration) {
        auto params = make_parameters();
        handle game = nullptr;
        assert(instance.create_feature(g.wrapped_context, 1, &params, &game) == success);
        assert(instance.evaluate_feature(g.wrapped_context, game, &params, nullptr) == success);
        assert_wrapped_restored(params);
        assert(instance.release_feature(game) == success);
    }
    assert(instance.feature_count() == 0);
    assert(g.create_calls == 2000);
    assert(g.evaluate_calls == 2000);
    assert(g.released.size() == 2000);
}
} // namespace

int main()
{
    test_success();
    test_partner_failure_keeps_primary();
    test_primary_failure();
    test_missing_partner();
    test_primary_evaluate_failure_restores();
    test_partner_evaluate_failure_restores();
    test_context_mismatch();
    test_parameter_write_failure();
    test_stress();
    std::cout << "bridge_core_tests: PASS\n";
}
