# E2E-5 performance evidence

No E2E-5 per-stage frame timings have been collected. Previous console logs
showed 4.32–5.18 FPS in the menu and about 9.7–10.4 ms average SDL present
time. This leaves a large unmeasured pre-present interval; it does not identify
the responsible subsystem. The stepped cursor remains uncorrected.

The combined experimental ARM64 build now records bounded count/average,
sampled p50/p95/p99, and maximum for intervals between the original
`GI_MessageLoop::Run` numeric stages every five seconds. It compiles and links;
no Switch stage data exists yet. These intervals do not separately identify
every nested resource decode or layout call.

The Switch input pump also records `input cadence` every five seconds: poll
count, pointer move count, window duration, and largest gap between polls.
This separates slow input sampling from visible frame pacing without changing
cursor behavior. The counters still need measurements on the console.

Required measurements before a final build: input poll, Win32 dispatch,
original callback, game update, layout, resource lookup/decode, scaling,
draw/composite/copy, SDL presentation, sleep/waits; each with count, p50, p95,
p99 and maximum. Record the cursor update frequency separately from the
visible frame frequency. Change pacing only after a measured bottleneck is
identified.
