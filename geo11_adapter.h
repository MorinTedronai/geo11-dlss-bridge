#pragma once

#include "bridge_core.h"

#include <cstdint>

namespace stereo_ngx::geo11
{
struct layout
{
    std::uintptr_t module_base = 0;
    std::uintptr_t module_end = 0;
};

void configure(layout value);
void set_logger(void (*logger)(const char *message));
bool unwrap_context(context wrapped, context *pass_through);
bool unwrap_resource(context pass_through, resource wrapped, resource_pair *pair);
} // namespace stereo_ngx::geo11
