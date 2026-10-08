# M19 — portable scene compositor

M19 adds a deliberately small CPU-only scene layer. `Scene` owns only the decoded images for its current sprites; it does not introduce a resource cache, loader, worker, UI object, game object, audio path or Direct3D state.

`SceneSprite` has an id, one BGRA image, top-left `x/y`, global alpha, layer and visibility flag. Rendering uses stable ascending layer order, so sprites with the same layer retain insertion order. An opaque sprite uses the M18 opaque path; a translucent sprite first combines its per-pixel alpha with its global alpha and then uses the unchanged M18 RGB565 alpha formula. Out-of-frame coordinates are intentionally clipped by the compositor.

## Verification

`make -C port/switch host-scene-compositor-test` covers layer precedence, exact RGB565 alpha stacking, CRC32/FNV64, top-left positioning, negative/outside clipping, hidden sprites and repeat-render determinism. `tests/probe_scene_compositor.py <game-root>` is an independent Python oracle: it decodes the real package, selects `DATA/Asteroid/00.gai` plus the first other decodable `DATA/*.gai` in lexical order, constructs the same three-sprite scene, and prints the scene and framebuffer fingerprints.

At runtime M19 uses Asteroid frame 0, one lexically selected second real GAI frame 0, and a 50%-alpha Asteroid overlay. The log records `[M19] scene compositor BEGIN`, every resource/position/layer, the canonical scene CRC32/FNV64 and `[M19] scene PASS`. A Switch result is valid only after at least 10 seconds of presentation, `PLUS`, nonzero frames/presents and `[BOOT] COMPLETE`.

M19 does not implement UI framework, Forms, game objects, input routing, EC_Cache, audio or gameplay. M20 is not started by this milestone.
