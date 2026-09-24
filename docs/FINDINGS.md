# Findings

Why single-eye DLSS happens under geo-11, and what the bridge does about it.
Based on the reference implementation (geo-11 1.3.16 x64, Direct Mode).

## 1. One DLSS evaluation cannot fill two eyes

DLSS SuperSampling consumes a set of input textures (`Color`, `Depth`,
`MotionVectors`) and writes an `Output`. The DLSS guide confirms multiple
simultaneous instances are supported, so the working model is: **evaluate DLSS
once per eye with its own input/output and its own history**.

## 2. geo-11 has two stereo resource layouts

geo-11 wraps game textures in `HackerTexture2D`. A small `mode` field on the
wrapper says how the two eyes are stored:

| `mode` | layout | example |
|---|---|---|
| `1` | **separate textures**: primary at `+0x140`, partner at `+0x148` | DLSS `Output` |
| `0` | **one texture array**: raw `Texture2DArray`, eye 0 = slice 0, eye 1 = slice 1; wrapper reports `ArraySize = 1` and `partner = null` | `Color`, `Depth`, `MotionVectors` |

So for `mode = 0` there is **no second pointer to find**. The right eye is inside
the same resource. Observed live for a `mode = 0` resource:

```
res: desc ... fmt=10 ... array=1 ... mode=0
NvAPI surface create mode - 1
pNewDesc array_size = 2, stereo mode = 0, is_stereo = 1
```

`is_stereo = 1` and the internal new-desc has `array_size = 2`: geo-11 already
made it stereo, as an array.

## 3. What the bridge does

Read-only geometry inspection plus per-eye evaluation:

```
for each DLSS resource:
    if mode == 1:  eyes = (wrapper.primary, wrapper.partner)
    if mode == 0:  eyes = split raw array slices 0/1 into two one-slice textures

create feature A (game-facing) and feature B (private)
each frame:
    evaluate A with eye-0 resources
    evaluate B with eye-1 resources
    restore all NGX parameters
```

The split uses only public D3D11 (`GetDesc`, `CopySubresourceRegion`), so the
`mode = 0` path needs **no geo-11 code knowledge** — only the `mode = 1` output
partner comes from geo-11's private layout.

## 4. Things that do not work

- **Binary-patching geo-11** to force `mode = 1` on the inputs hangs at startup;
  the array path is load-bearing.
- **`[TextureOverride] StereoMode=1`** on the inputs does not help: they are
  already stereo (as an array), and forcing a creation mode is risky.
- **Hooking geo-11 geometry constructors** to observe creation is unstable at
  startup; read-only field inspection is enough.

## 5. Why this generalizes

- The DLSS feature/parameter ABI is NVIDIA's, not the game's.
- The array-slice split is generic D3D11.
- The only version-specific part is locating a **separate output partner** in
  geo-11, which can be isolated behind a per-version backend.
