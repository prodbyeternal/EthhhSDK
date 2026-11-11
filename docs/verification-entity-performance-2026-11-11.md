# Verification — entity performance, camera/FOV and mouse attacks (2026-11-11)

## Outcome and limits

The correction is built in **Release / Win32** under `build/dist`. This resumed
work performed **no installation, game launch, termination, restart or process
inspection**. Fresh manual game-closure confirmation is required before deploying
the matching core, managed host and HalfLifeMovement assembly.

The reported **20 FPS / 3–11 active simulation Hz** entity regression and camera
flicker remain **live-unaccepted**. Offline performance measurements, camera-policy
tests and mouse-binding evidence do not establish recovered engine FPS, actual
cutscene compatibility, attack naming or sword-hover behavior.

## Implemented correction

- Entity sampling uses a complete lightweight bounded owner list rather than
  allocating inspector snapshots. Protected readable-region metadata is scoped
  to sampling/render phases; every copy remains SEH-protected.
- Identity, allocation, count, model/controller, double-buffer-pair, submitted
  root/bone and mesh-index checks remain intact. Reusable buffers and guarded
  identical-alpha output reuse remove warmed-up heap churn.
- Sampling and aggregate SDK render validation/blending each have a **2 ms soft
  work budget**. Started bounded work can finish beyond the limit; remaining
  owners render natively. Original rendering and Present are not charged.
- Orbit installation is limited to the primary View call (return `0x43E230`).
  Secondary effect passes reuse installed endpoints without recapturing baseline.
  Native pose/derived matrices are restored after the complete render scope.
- Optional **30–120° vertical normal-mode FOV** replaces only render-time View
  lens/projection data, never native `Camera+0x108`. It uses the existing
  exact-player, focus, movie, HL and scripted-shot guards, even with orbit off.
- **M1 slash / held M2 spin** follows live native action bindings, preserving
  native edges, held pressure, keyboard/gamepad attacks and release-to-rearm.
  Cross-action/gate conflicts and unsupported custom bindings are refused.
  Exact-player validation adds no allocating inspector snapshot to input polling.
- Shared manager pages include FOV/input controls and entity timing/query/budget
  diagnostics. Camera controls adapt to narrow widths; tall pages scroll.

Details and offsets: [ethhhpatch.md](ethhhpatch.md).

## Offline verification

The complete runner has produced a genuine **exit 0**, not just printed PASS
lines. Python unittest progress now uses stdout, avoiding Windows PowerShell's
successful native-stderr `ErrorRecord` conversion; failed tests still exit nonzero.

| Regression | Result |
| --- | --- |
| Entity interpolation, lifetime, budgets, buffers | 130,991 assertions |
| Camera orbit/restoration | 14,208 assertions |
| Primary View gate and coupled scoped pose/lens cleanup | 1,418 assertions |
| Native mouse holds/edges/rearming/custom bindings | 1,004 assertions |
| Native movement-key lease/ranges | 41,876 assertions |
| Static evidence/refusal checks | 30 tests |
| Hot-path source guards | 20 tests |
| x86 ABI | 219 exports / 217 P/Invoke bindings resolved |

HL math, collision, wall-state, managed interop/lifecycle, Ninja identity/facing,
render scheduling/projection and BCrypt hashing regressions also passed. These
are isolated tests, not live game acceptance.

The fingerprinted static inspector checks **199 instruction anchors, 59 bounded
slices and 51 explicit edges**. It also reads embedded KEYBOARD/PS2 maps from
`Data/general.vol`, with no process access or archive writes. Native held-pressure
semantics are verified; slash/spin naming remains square/circle layout inference.

### Synthetic reader benchmark

`EthhhPatchEntityPerformanceTests.cpp` exercises the actual production reader with
128 entities × 32 nodes, 200 samples/frames, plus real page revocation and guard
pages. The recorded final pre-layout suite reported:

| Measurement | Conservative reader | Scoped protected reader |
| --- | ---: | ---: |
| Sample time | 20.9060 ms | 1.6971 ms |
| SDK render validation time | 24.2052 ms | 0.1899 ms |
| Page queries/sample | 2,178 | 2 |
| Page queries/frame | 2,560 | 3 |

Warmed-up scoped sampling/rendering performed **zero heap allocations**.
Timing varies with load and is not a pass threshold. The stable synthetic poses
and repeated identical alpha isolate reader/guard/allocation overhead; they do
not benchmark the engine, changing animation, rendering, collision or actual FPS.

## Preservation

Read-only comparison against `ethhh-entity-performance-baseline-20261111.json`
confirmed the original executable, Data archives, `ininja-offsets.h`, saves,
original settings and manifests remain intact. A changed SDK tuning file was
retained rather than reset.

The old baseline's installed SDK DLLs were already replaced before this resumed
work: installed core is `2807547c34628323e083f1f1e29f39548f08c880c0a1496cfa330b9977b62165`
(file time 16:51:24), with an older 16:50 backup present. Installed host/mod changes
were also retained. Build scripts were checked: native, managed and mod outputs
write staging only, not live DLLs. These historical differences must not be
misreported as unchanged or reverted.

No commits, resets, cleanup of user files or manifest/settings replacement were
performed here. Final Valve agreement/attribution packaging is still pending.

## Required manual acceptance after authorized deployment

1. Compare Animation **off/on** in the same scene, F9 closed. Record game FPS,
   active simulation Hz, simulation seconds/wall second, and last SDK sample/render
   milliseconds/query counts. At 100-Hz rendering simulation must remain 60 Hz.
2. Check mouse orbit while stationary/moving, near walls, through nearby/distant
   scripted shots, movies, F9/alt-tab, respawn, resume and HL handoff.
3. Test FOV at 30/60/90/120° with orbit off/on, screen-edge visibility, native
   framing during exclusions and restoration after disabling the override.
4. Test M1 slash and held M2 spin/hover airborne; keyboard/gamepad coexistence,
   simultaneous holds, custom keys, refused conflicts and release-to-rearm.
5. Check native death, chains, rails, Ctrl handoff and independent wall Space;
   retain HL defaults of **69 eye-height / 24 forward-offset HL units**.
6. Verify exact 1080p/4K Reset, HUD/movie fit, high-refresh cadence and manual
   display restore. No offline assertion substitutes for these checks.
