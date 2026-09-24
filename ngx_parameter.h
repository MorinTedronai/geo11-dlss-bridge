#pragma once

#include "bridge_core.h"

struct ID3D11Resource;
struct ID3D12Resource;

struct ngx_parameter
{
    virtual void Set(const char *, unsigned long long) = 0;
    virtual void Set(const char *, float) = 0;
    virtual void Set(const char *, double) = 0;
    virtual void Set(const char *, unsigned int) = 0;
    virtual void Set(const char *, int) = 0;
    virtual void Set(const char *, ID3D11Resource *) = 0;
    virtual void Set(const char *, ID3D12Resource *) = 0;
    virtual void Set(const char *, void *) = 0;
    virtual stereo_ngx::result Get(const char *, unsigned long long *) const = 0;
    virtual stereo_ngx::result Get(const char *, float *) const = 0;
    virtual stereo_ngx::result Get(const char *, double *) const = 0;
    virtual stereo_ngx::result Get(const char *, unsigned int *) const = 0;
    virtual stereo_ngx::result Get(const char *, int *) const = 0;
    virtual stereo_ngx::result Get(const char *, ID3D11Resource **) const = 0;
    virtual stereo_ngx::result Get(const char *, ID3D12Resource **) const = 0;
    virtual stereo_ngx::result Get(const char *, void **) const = 0;
    virtual void Reset() = 0;
};
