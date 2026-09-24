# geo11-dlss-bridge

**DLSS in both eyes when a game is being rendered in stereo by
[geo-11](https://github.com/ThreeDeeJay/geo-11).**

geo-11 turns Direct3D 11 games into stereo (side-by-side, top-and-bottom, or
Direct Mode for VR). DLSS normally runs once, so only one eye gets an upscaled
image and the other stays black. This project runs DLSS **once per eye**, reusing
geo-11's own stereo resources, and documents how and why it works.

> **Status.** The stereo engine and geo-11 eye handling are **working and
> validated** in the bundled backend (geo-11 1.3.16 Direct Mode + a DLSS
> injector). The core (`bridge_core`, NGX parameter handling, the eye-splitting
> adapter) is written to be game-agnostic; the interception path currently ships
> as a reference backend for one title, with a generic NGX provider-proxy on the
> roadmap. See [`docs/ROADMAP.md`](docs/ROADMAP.md).

## The problem

With geo-11 and a DLSS injector both active, DLSS is evaluated once. The left eye
gets the upscaled image; the right eye shows only game effects. One DLSS
evaluation cannot fill two eyes — it is an integration gap, not a bug in either
tool.

NVIDIA's DLSS guide confirms multiple simultaneous DLSS instances (one per eye)
are supported for VR, so running DLSS twice is valid. The hard part is handing
DLSS the correct **per-eye inputs** and writing the **per-eye output**, which
means understanding how geo-11 stores the two eyes.

## How it works

geo-11 uses two stereo resource layouts. The bridge detects them read-only and
pairs them:

| geo-11 wrapper `mode` | layout | typical resource |
|---|---|---|
| `1` | separate primary + partner textures | DLSS `Output` |
| `0` | one raw `Texture2DArray`, eyes in slices 0 and 1 | `Color`, `Depth`, `MotionVectors` |

Then it:

1. Creates **two independent DLSS features** (game-facing primary + private
   partner) so each eye keeps its own history.
2. For `mode = 0` resources, copies array slice 0/1 into two one-slice textures
   and feeds them as the two eyes.
3. For `mode = 1` resources, uses geo-11's existing partner texture.
4. Evaluates both eyes and restores all NGX parameters afterwards.

Full background: [`docs/FINDINGS.md`](docs/FINDINGS.md).

## Use cases

- **VR via geo-11**: add DLSS to a geo-11 Direct Mode title instead of being told
  to disable it, so headsets get the frame-rate headroom.
- **Stereo 3D displays / projectors / AR glasses**: keep DLSS quality and
  performance while preserving real stereo in SBS/TAB output.
- **DLSS performance/quality research**: a harness for per-eye DLSS behavior,
  jitter, and resource layouts.
- **Tool authors**: the core is a reference for building game-agnostic DLSS
  stereo middleware.

## Limitations

- **D3D11 only.** geo-11 targets Direct3D 11; older APIs only via dgVoodoo2.
- **Direct Mode layout required.** The array/partner model is what geo-11 Direct
  Mode produces. Plain SBS packs the eyes differently (a crop, not a slice); an
  SBS backend is on the roadmap.
- **geo-11 version lock.** The bundled adapter reads exact geo-11 vtables and
  field offsets (1.3.16 x64). Other builds need re-derived offsets or signature
  scanning.
- **Needs an NGX path to intercept.** The game must actually run DLSS (native or
  a mod). There is nothing to bridge for titles with DLSS disabled or absent.
- **SuperSampling only.** Feature `1` (DLSS upscaling/DLAA). Frame Generation
  (`nvngx_dlssg`) is out of scope.
- **Roughly 2x DLSS cost** per frame (two evaluations).
- **Per-title effect fixes still matter.** DLSS + jitter interactions and geo-11
  shader fixes vary by game.
- **No ReShade-based DLSS under geo-11.** ReShade and geo-11 both want the D3D11
  device; that combination is out of scope.

## Examples

| Scenario | Status | Notes |
|---|---|---|
| Total War: WARHAMMER III + Warhammer3DLSS 0.3.1 + geo-11 1.3.16 Direct Mode | **Verified** | Both eyes render DLSS; 2,405 clean evaluations in the validation run. See [`docs/EXAMPLES.md`](docs/EXAMPLES.md). |
| DX11 title with **native DLSS** + a geo-11 fix | Candidate | Requires the provider-proxy build; inputs must be geo-11-stereoized. |
| DX11 DLSS title + **OptiScaler / DLSS Enabler** under geo-11 | Candidate | Same NGX layer as the proxy; chainable in principle, unproven. See [`docs/ECOSYSTEM.md`](docs/ECOSYSTEM.md). |

## Requirements

- Windows, a Direct3D 11 game, geo-11 in Direct Mode, and a working DLSS path.
- To build from source: Visual Studio 2022 (x64 C++ toolset) and MinHook
  (see [`third_party/README.md`](third_party/README.md)).

The bundled backend is version-locked to:

| File | SHA-256 |
|---|---|
| `Warhammer3.exe` (8.1.1.0) | `B7315FA718FD84E2E018E2C4DF06600E9DF0076156B474F148D9D558C939AA55` |
| geo-11 `d3d11.dll` (1.3.16) | `17374B9D279BD1B1AD3B6BAB67A7690B6896160F46AAFEB811921A2D38535D94` |
| `nvngx_dlss.dll` (310.9.1.0) | `3975567B8943C53ACCE397F2B72380092F84F162D00B0D2C7D08A1025C563983` |
| Warhammer3DLSS 0.3.1 `dinput8.dll` | `A12CA4AA0F3D510454B395A1EDC612A3CC1CF6C1BF46F01F74B29A0572252CA2` |

This repository does **not** distribute the DLSS mod or geo-11; supply your own
copies.

## Build

```powershell
git clone --depth 1 https://github.com/TsudaKageyu/minhook.git third_party/minhook
.\build.ps1            # -> build\dinput8.dll
```

Optional checks:

```powershell
.\build_tests.ps1      # CPU-only core unit tests
.\run_asan_tests.ps1   # same tests under AddressSanitizer
```

## Install

`deploy.ps1` verifies the game/geo-11/DLSS/mod hashes, then installs the bridge
next to them. Point `-ModDll` at your own copy of the DLSS injector's
`dinput8.dll`; it is installed unchanged as `wh3dlss393.dll` so the bridge can
chain it.

```powershell
.\deploy.ps1 `
  -GamePath "C:\Program Files (x86)\Steam\steamapps\common\Total War WARHAMMER III" `
  -ModDll   "C:\path\to\Warhammer3DLSS\dinput8.dll"
```

It copies `dinput8.dll` (the bridge, from `build\`), `wh3dlss393.dll` (your mod,
unchanged), and `wh3_dlss.ini`.

## Verify

1. Launch with geo-11 + the bridge and load a scene. **Both eyes show DLSS.**
2. Check `geo11-dlss-bridge.log` in the game folder:
   - `create: stereo pair ready ... pairs=1`
   - `res: array unwrap ... result=1` for Color/Depth/MotionVectors
   - `evaluate_hook: handle=... result=0x00000001` each frame
   - no `capture failed`, `primary failed`, `partner failed`, or `fatal:` lines

## Uninstall

```powershell
.\rollback.ps1 -GamePath "C:\Program Files (x86)\Steam\steamapps\common\Total War WARHAMMER III"
```

geo-11 is never modified, so DLSS simply returns to its previous single-eye
behavior.

## Repository layout

```
proxy.cpp             dinput8 proxy, hooks, startup guards (reference backend)
bridge_core.{h,cpp}   stereo feature pairing / parameter handling (unit-tested)
bridge_core_tests.cpp
geo11_adapter.{h,cpp} read-only geo-11 wrapper inspection + eye-slice extraction
ngx_parameter.h       NGX parameter interface used by the caller
abi/                  version-locked geo-11 private ABI header
forwarders.asm        export forwarders to wh3dlss393.dll
dinput8_proxy.def
wh3_dlss.ini          sample DLSS mod config
build*.ps1, run_asan_tests.ps1, deploy.ps1, rollback.ps1, find-vcvars.ps1
docs/                 FINDINGS, ECOSYSTEM, EXAMPLES, RESEARCH, ROADMAP
third_party/          MinHook setup + what is not distributed
```

## Research starting points

geo-11 is a 3DMigoto fork; the important state is its stereo resource
representation, and the clean long-term design is to proxy the NGX provider
(`nvngx_dlss.dll`) instead of chaining a specific mod. Sources, tooling, methods,
open questions, and safety notes are in
[`docs/RESEARCH.md`](docs/RESEARCH.md).

## Credits

geo-11 / 3DMigoto, the geo-11 fix community (Helix Mod), and the upscaler
translation projects [OptiScaler](https://github.com/OptiScaler/OptiScaler) and
[DLSS Enabler](https://github.com/artur-graniszewski/DLSS-Enabler).

Special thanks to **wackywooh00pizzaman**, author of *Total War: WARHAMMER III
DLSS* (<https://www.nexusmods.com/totalwarwarhammer3/mods/393>), the DLSS
injector used in the verified example.

## License

MIT — see [`LICENSE`](LICENSE).
