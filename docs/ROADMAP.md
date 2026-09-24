# Roadmap to a fully game-agnostic bridge

The stereo engine is done; the missing piece is a generic interception layer.

## 1. NGX provider proxy (highest value)

Instead of chaining one game's DLSS mod, proxy the NGX provider
(`nvngx_dlss.dll` and friends):

- forward **all** exports unchanged,
- intercept only DLSS SuperSampling (`CreateFeature` / `EvaluateFeature` /
  `ReleaseFeature`),
- create a second feature per eye and split resources as in `docs/FINDINGS.md`.

This makes the bridge independent of any specific game or mod. It is the same
layer OptiScaler/DLSS Enabler use, so ordering/chaining must be designed.

## 2. Version-tolerant geo-11 backend

- Replace fixed RVAs with **signature scanning** for the wrapper vtables and the
  `mode`/partner fields.
- Support multiple geo-11 versions behind one interface, selected at runtime;
  fail closed when unknown.

## 3. Additional eye backends

- **Generic array backend** (no engine knowledge): detect `ArraySize == 2`.
- **geo-11 backend**: array + separate output partner.
- **SBS/TAB backend**: crop a double-wide/tall texture into two eyes.

## 4. Configuration

- Injection DLL name (per game).
- Optional/relaxed hashes; strict by default.
- Feature id (SuperSampling only) and resource-name overrides for non-canonical
  callers.

## 5. Test matrix

- Extend `src/bridge_core_tests.cpp` with array/partner/SBS capture cases.
- Add a fake-NGX integration harness (two features, per-eye histories).
- Track a small compatibility table of titles (verified vs candidate) in
  `docs/EXAMPLES.md`.

## 6. Packaging

- Provider proxy build (drop-in `nvngx_dlss.dll`) plus a thin loader if needed.
- Document supported combinations and the "no ReShade under geo-11" rule.
