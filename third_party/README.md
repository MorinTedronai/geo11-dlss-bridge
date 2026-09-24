# Third-party dependencies

## MinHook

The bridge uses [MinHook](https://github.com/TsudaKageyu/minhook) for the mod
393 hook trampolines. It is not vendored here; clone it into `third_party/minhook`
(or set `MINHOOK_DIR`):

```powershell
git clone --depth 1 https://github.com/TsudaKageyu/minhook.git third_party/minhook
```

`build.ps1` expects `third_party/minhook/include/MinHook.h` and the four source
files under `third_party/minhook/src`.

## Not distributed

This repository intentionally does **not** include:

- **Warhammer3DLSS 0.3.1** (`dinput8.dll`) — the DLSS/NGX injecting mod.
- **geo-11** (`d3d11.dll`) — the stereo renderer.

Obtain both from their respective authors. `deploy.ps1` verifies their exact
hashes before installing the bridge alongside them.
