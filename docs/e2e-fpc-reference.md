# E2E-1: FPC platform reference

Reference: [`pakompom/SpaceRangersHD_FPC`](https://github.com/pakompom/SpaceRangersHD_FPC), inspected at `5d13e0694377e0186501e07f46930e2e1c546450`. This is a read-only comparison; the C++ upstream remains pinned at `57fa689c630193a66fdea6ca4c79a188814991cd`.

| C++ game API/path | FPC implementation | Switch C++ decision |
| --- | --- | --- |
| `Forms::Application`, `GR_Main::InitializePlatformRuntimeAndMainWindow` | `platform/GameWindow.pas` creates an SDL window and renderer (`SDL_CreateWindow`, `SDL_CreateRenderer`) | Keep original startup and bind its required window semantics to the existing SDL2 Switch platform. |
| `GI_MessageLoop` and `WM_*` | `platform/GameWindow.pas` converts `SDL_PollEvent` input into game messages; `platform/GameInput.pas` retains `WM_MOUSEMOVE`, button and key constants | Feed normalized Switch controller events into the original message loop using its internal message protocol. |
| Event waits/threads | `platform/GameEvents.pas` uses SDL mutexes and condition variables | Implement only the synchronization primitives reached by the original runtime; preserve waits. |
| Software framebuffer and presentation | `platform/GameGraphics.pas` uses SDL textures; `GameGraphicsBase.pas` implements D3D-shaped interfaces over SDL | Prefer original CPU `TGraphBufGR` traversal, then present RGB565 through the existing Switch SDL backend. |
| Audio startup | `platform/GameAudio.pas` uses `SDL_InitSubSystem(SDL_INIT_AUDIO)` and `SDL_OpenAudioDevice` | First use the original sound-disabled path for menu startup; if required, map the actual calls to SDL audio. |
| File paths and clock | `platform/GameSystem.pas` resolves path case and implements `GameTickCount` from `GetTickCount64` with 32-bit wrap | Keep install and user paths separate; retain millisecond and wrap semantics. QPC uses a coherent counter/frequency pair. |
| Script DLL imports | `platform/GameScriptLibrary.pas` rejects Win32 DLL imports outside 32-bit Windows and returns no procedure pointer | FPC offers no ARM64 implementation of this ABI. The C++ E2E path must inspect actual script-host calls and port required behavior; a script-host no-op cannot complete E2E-1. |
| ARM64 numeric handling | FPC source contains explicit wide arithmetic in `source/core/aBezierPC24.pas`; no general binary compatibility for C++ `long double` follows from that | Measure devkitA64 `long double` and audit serialized `Extended` fields independently. |

The FPC repository currently documents Linux x86_64 and macOS ARM64 support. Its platform code is a semantic reference, not evidence that the Switch C++ executable already runs.
