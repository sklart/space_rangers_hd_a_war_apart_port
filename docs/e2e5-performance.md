# E2E-5 performance evidence

The isolated original-game Linux host probe now collects per-stage frame
timings. In its menu run, stage 15 sampled p95 at roughly 30 ms and stage 17
sampled p95 at roughly 6 ms. These host timings do not locate the Switch
bottleneck. Previous console logs showed 4.32–5.18 FPS in the menu and about
9.7–10.4 ms average SDL present time. That leaves a large pre-present interval
unmeasured on Switch. The stepped cursor remains unverified after the input
history change.

The combined experimental ARM64 build now records bounded count/average,
sampled p50/p95/p99, and maximum for intervals between the original
`GI_MessageLoop::Run` numeric stages every five seconds. It compiles and links;
no Switch stage data exists yet. These intervals do not separately identify
every nested resource decode or layout call.

The next build additionally records stage 30 (region preparation) and stage 31
(dirty rectangle traversal and drawing) inside `DrawQueuedUpdateRects`. The
existing stage 17 covers frame presentation. In one original-game host run to
Hangar, stage 30 was negligible and stage 31 dominated stage 15; that result
does not establish the Switch bottleneck or separate every nested framebuffer
operation. Further detail should follow the next console measurements.

The Switch input pump also records `input cadence` every five seconds: poll
count, pointer move count, window duration, and largest gap between polls.
It now replays recent HID button edges in order, coalesces intermediate pointer
motion, and reports missing HID samples. Host tests cover 30–300 ms A pulses
at 5–60 FPS. The counters and visible cursor behavior still need measurements
on the console.

Required measurements before a final build: input poll, Win32 dispatch,
original callback, game update, layout, resource lookup/decode, scaling,
draw/composite/copy, SDL presentation, sleep/waits; each with count, p50, p95,
p99 and maximum. Record the cursor update frequency separately from the
visible frame frequency. Change pacing only after a measured bottleneck is
identified.
