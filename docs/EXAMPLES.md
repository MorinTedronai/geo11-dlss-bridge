# Examples

## Verified: Total War: WARHAMMER III

**Stack:** geo-11 1.3.16 (Direct Mode `katanga_vr`) + Warhammer3DLSS 0.3.1 +
`geo11-dlss-bridge` (a `dinput8.dll` proxy that chains the mod as
`wh3dlss393.dll`), on Direct3D 11.

**What happened before:** DLSS ran once, only the left eye was upscaled, the
right eye was black except for effects.

**What the bridge did:** split geo-11's two-eye resources per frame and ran DLSS
twice.

- `Color` / `Depth` / `MotionVectors`: `mode = 0` → array slices 0/1 copied into
  two one-slice textures.
- `Output`: `mode = 1` → geo-11's existing partner texture.

**Result:** both eyes render DLSS.

**Evidence (`geo11-dlss-bridge.log`):**

```
create: stereo pair ready primary=... partner=... pairs=1
res: array unwrap wrapped=... source=... result=1 left=... right=...
capture: key=Color wrapped=... unwrap=1 primary=... partner=...
evaluate_hook: handle=... result=0x00000001
```

Validation run: **2,405** bridged evaluations, zero capture/primary/partner/fatal
failures.

**Thanks:** the DLSS injector used for this example is *Total War: WARHAMMER III
DLSS* by **wackywooh00pizzaman** —
<https://www.nexusmods.com/totalwarwarhammer3/mods/393>.

## Candidate: a DX11 title with native DLSS + a geo-11 fix

Pattern: the game calls DLSS itself, and a geo-11 fix (Helix Mod) renders stereo.

1. Confirm the left eye shows DLSS under geo-11.
2. Build the **provider-proxy** variant (intercept NGX, run one instance per eye
   using the split described in `FINDINGS.md`).
3. Verify both eyes; the array path needs no geo-11 code knowledge, but a
   separate `Output` partner still needs the geo-11 backend.

Status: unverified. Start from `docs/RESEARCH.md`.

## Candidate: a DX11 DLSS title + OptiScaler under geo-11

OptiScaler already sits at the NGX layer. The bridge would have to sit **above**
it (game → bridge → OptiScaler → real `nvngx`) and let OptiScaler produce each
eye's upscale. Overlap and double-translation are the risks.

Status: unverified; see `docs/ECOSYSTEM.md`.

## What to record when testing a new title

- Which DLSS resources are `mode = 0` (array) vs `mode = 1` (partner).
- Whether both DLSS features are created and evaluated successfully.
- Whether the right eye renders (not just receives effects).
- Any geo-11 effect fixes that interact with DLSS (jitter, shadows, lighting).
