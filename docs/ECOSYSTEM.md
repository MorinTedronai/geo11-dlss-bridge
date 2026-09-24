# Ecosystem: how geo-11 VR works and how DLSS gets injected

Context for where a stereo DLSS bridge has to fit.

## geo-11 stereo delivery

geo-11 is a drop-in `d3d11.dll` that renders Direct3D 11 games in stereo
(SBS/TAB or Direct Mode). It is the DX11 successor to 3D Vision/3DMigoto, and
community fixes live on Helix Mod.

Typical VR delivery of geo-11 output:

| Tool | Role |
|---|---|
| **VRto3D** (OpenVR driver) | Presents SBS/TAB as an OpenVR stereo layer |
| **CaronteApp** | Universal SBS/TAB → OpenVR/OpenXR, with native geo-11 KatangaVR support |
| **VRScreenCap** | Captures the stereo window and feeds the headset |
| **KatangaVR** (geo-11 Direct Mode) | geo-11's own direct path; output consumed by the above |

These tools are **orthogonal** to DLSS: they consume geo-11's stereo output and
never touch the NGX layer. A stereo DLSS bridge fits underneath them.

## DLSS "injection" methods

| Method | How it works | Layer |
|---|---|---|
| DLL swap (DLSS Swapper) | Replace `nvngx_dlss.dll` version | NGX provider |
| NGX middleware (**OptiScaler**, **DLSS Enabler**) | Wrap `nvngx`/`winmm`/`dxgi`, intercept NGX calls, forward or translate to FSR/XeSS | NGX provider |
| ReShade addon (RenoDX, Depth3D) | Addon calls NGX from ReShade's device | ReShade's device |
| Engine/game mods | ASI/`winmm` proxy hooks engine render code, then calls NGX | Engine + NGX |
| Native | Game calls NGX itself | NGX provider |

## Compatibility with geo-11

Governed by one rule: **geo-11 owns the D3D11 device and swapchain.**

| Method | Compatible? | Why |
|---|---|---|
| DLL swap | Yes | No device ownership; only an NGX-version change |
| NGX middleware (OptiScaler / DLSS Enabler) | Same layer, conflicts in practice | Both claim the `nvngx` surface; spoofing/signature modes; may translate to FSR/XeSS; DX12-first |
| ReShade addon | No | Device ownership conflict with geo-11 |
| Engine/game mods | Conditionally | Works only if they call NGX and geo-11 has stereoized the inputs |
| Native DLSS | Yes (best case) | Proxy intercepts; per-eye split applies |

## Implications

- The clean long-term design is an **NGX provider proxy** (the same layer
  OptiScaler/DLSS Enabler occupy), which makes the bridge game- and mod-agnostic.
- The realistic compatible set is **DX11 + geo-11 + native or swapped-DLL DLSS**.
- Avoid ReShade-based DLSS paths under geo-11.

Sources: [OptiScaler](https://github.com/OptiScaler/OptiScaler) ·
[DLSS Enabler](https://github.com/artur-graniszewski/DLSS-Enabler) ·
[Helix Mod geo-11](https://helixmod.blogspot.com/search/label/geo-11) ·
[VRto3D](https://github.com/oneup03/VRto3D) ·
[CaronteApp](https://elliewasteland.github.io/CaronteLauncherVR/).
