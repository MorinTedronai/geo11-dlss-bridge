# Research starting points

Where to dig next if you want to extend the bridge to more games.

## Primary sources

- **geo-11** — drop-in DX11 stereo plugin, a 3DMigoto fork. Releases/binaries
  carry a PDB, which makes mapping `HackerTexture2D`, `HackerResource`, and the
  Direct Mode context straightforward. Direct Mode is the VR path.
- **3DMigoto** — the upstream project; its `HackerDevice`/`HackerTexture2D`
  resource-override and `StereoMode` code is the clearest description of the
  concepts (surface creation modes, fuzzy texture overrides, deferred
  stereoization).
- **NVIDIA NVNGX / DLSS** — the DLSS Programming Guide (multiple DLSS instances
  are supported for VR) and the NGX parameter ABI used by every DLSS caller.
- **OptiScaler / DLSS Enabler** — open implementations of an NGX provider proxy
  (the layer a game-agnostic bridge should occupy).

## Tooling

- **Disassembly / decompilation:** Ghidra, `objdump`, LLVM tools.
- **PE / RTTI:** RTTI recovery from MSVC vtables; `pefile`, `capstone` in Python.
- **Hooking:** MinHook (or Detours) for the proxy; keep hooks few and guarded.
- **Live inspection:** read-only process memory dumps are far safer than hooking
  constructors during startup.
- **Logs:** geo-11's own debug log (enable `calls`/`debug` in its ini) shows
  surface creation modes, texture overrides, and descriptor changes.

## Methods that worked

1. **Identify vtables by RTTI** rather than by pattern, then read fields
   read-only to learn the layout (`mode`, primary/partner, wrapped/partner
   texture, cached desc).
2. **Detect stereo generically** where possible: a resource whose `ArraySize`
   is 2 when you expected 1 is the eye pair; split with `CopySubresourceRegion`.
   This needs no engine-specific code.
3. **Prefer signature scanning** for offsets over hard-coded RVAs so a bridge can
   survive version changes.
4. **Mock before live:** unit-test the feature pairing and parameter save/restore
   with fakes, then run one controlled live test.
5. **Fail closed:** validate versions/hashes and refuse to run on a mismatch.

## Open questions

- Why is DLSS `Output` `mode = 1` while `Color`/`Depth`/`MotionVectors` are
  `mode = 0`? Which creation path chooses each?
- Does **SBS** mode (non-Direct) also produce arrays, or a single double-wide
  texture that must be cropped instead?
- How do other geo-11 versions lay out these fields, and can one signature cover
  several?
- Can NVAPI `SetSurfaceCreationMode` force stereo cleanly for these resources
  (it exists in 3DMigoto), or is it only cosmetic for this case?
- Can the output partner be found without private offsets (e.g. via a context
  field, or by observing the first stereoized target)?

## Safety notes

- Do not destructively patch geo-11 to force the stereo path; it is load-bearing
  and hangs at startup.
- Keep backups and a one-command rollback; never modify the game install without
  a restore path.
- Watch for GPU driver resets when experimenting with surface creation modes.
