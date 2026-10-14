# E2E-5 Options and RuinsTalk crash analysis (587f746)

Inputs: the earlier `1.zip` (Options) and `2.zip` (RuinsTalk) supplied on
14 October 2026. The files currently at `G:\1.zip` and `G:\2.zip` have since
been replaced with the `444a2c9` test; see `hardware-444a2c9-20261014.md`.
Matching ELF: `SpaceRangersHDAWarApartE2E4Combined-587f746.elf`, SHA-256
`CD82E52380C1CB1E616166CCDAAA5E942D032578D608AABE398CCD39C72F4AA3`.
Its GNU Build ID is `25cbb93e8291b67d4d5ac4a5cc50d18e74f6691f`,
matching both Atmosphère module IDs. The offsets below were resolved with
`aarch64-none-elf-addr2line -a -f -C -i`; the Options instruction was checked
with `aarch64-none-elf-objdump -d`.

## Options

| Offset | Function | Source |
| --- | --- | --- |
| PC `0x33F284` | `fCfgSettings::TfCfgSettings::FormatResolution` | generated `fCfgSettings.cpp:1512` |
| LR `0x34CB28` | `fCfgSettings::TfCfgSettings::OnOpen` | generated `fCfgSettings.cpp:413` |
| `0x562B84` | `GI_MessageLoop::TMessageLoopGI::Run` | generated `GI_MessageLoop.cpp:1328` |
| `0x5A67CC` | `Globals::RunMainScreenStateLoop` | generated `Globals.cpp:4054` |
| `0x5FF840` | `Rangers::ProgramMain` | generated `Rangers.cpp:436` |

At PC, `ldr w20, [x0, x19]` reads `GameDisplayModes[Index].Width`.
The report has `x0=0` and `x19=0xFFFFFFFFFFFFFFF0`, giving the reported
fault address `0xFFFFFFFFFFFFFFF0`: the mode array was empty and the index
was `-1`. The software renderer skipped the original Direct3D mode enumeration.
Native-width UI payload conversion already exists in the effective source
generator; pointer truncation is a separate hazard, but does not explain this
specific PC or fault address.

The current uncommitted platform change initializes one fixed 1280x720 game
mode and one robot mode before the Options UI builds. An optimized host binary
with SHA-256 `d592b49b493c2d4d129254ba6f1efb4cc0431c9eb27db92761254ab811da92b4`
opened and closed Options twice using original game resources. Both openings
logged `game=1 selected=0 length=1 robot=1 selected_robot=0 robot_length=1`,
`options groups constructed heights=1070,2575,444,0,738,196`, and
`screen Run OnOpen ready`. Isolated probe report:
`port/switch/build/e2e5-host-game/runs/c92155b8fc8c408f9b4b6147aed72603/report.json`.
The probe timed out after observing both cycles; the original process did not
crash. Changing a setting and checking all group interactions remain open.

## RuinsTalk

The second report's PC `0x00000023841BB710` is outside the game module.
LR `0x5A4B74` resolves to `Globals::FinalizeGlobalUiRuntime` through
`pas::destroy<PopUp::TfPopUpController>`; return address `0x600220` is
`Rangers::ProgramMain:527`. This is cleanup after the game log recorded
`Error Load Gai`, then `Rescaling Bm.FormRuins.2RCbg...`, then
`Exception AllocEC. size=-1889381056`. The subsequent Instruction Abort is
not evidence of the first error's cause.

The old game log lacks a GAI key and allocation call site. Its reported free
physical memory was about 2.78 GiB, so the negative allocation request must
not be classified as proven physical memory exhaustion. Current uncommitted
instrumentation logs a GAI key and validates bounds before allocation. In one
host run using original resources, it accepted `Bm.Gov.2FeiAnim0a`
(2,729,728 bytes) and `Bm.Gov.2FeiAnim1a` (11,296,250 bytes) while entering
RuinsTalk. It logged `Bm.FormRuins.2RCbg` bounds 1920x1536 and a checked RGBA
size of 11,796,480 bytes. The run reached four dialog callbacks and Hangar
screen 4 with 60 frames, without a crash. Isolated probe report:
`port/switch/build/e2e5-host-game/runs/d3824c87f76c4aec900084a126739e6c/report.json`.

This host pass does not identify which GAI failed on Switch, or prove that the
negative `AllocEC` request is fixed there. A new hardware log with the key and
pre-allocation size diagnostics is needed before assigning the root cause.
The focused malformed GAI bounds test passed locally under ASan and UBSan.
