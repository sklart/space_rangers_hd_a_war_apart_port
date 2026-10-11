"""Compile selected unmodified original UI methods in a controlled host fixture.

This is a method-level test, not a resource-backed main-menu launch.
"""

from __future__ import annotations

import os
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
GAME = ROOT / "upstream/cpp/src"
sys.path.insert(0, str(ROOT))
from tools.e2e_source_overrides import generated_source  # noqa: E402


def method(source: str, signature: str, following: str) -> str:
    start = source.index("    " + signature + " {")
    end = source.index("    " + following, start)
    return source[start:end]


def check_private_resource(game_root: Path) -> None:
    # Read only: the commercial data is never copied into the repository.
    sys.path.insert(0, str(ROOT / "tests"))
    import probe_m23_text_release as config
    from probe_ui_images_release import decode_dat

    nodes = list(config.nodes(config.parse_blocks(decode_dat(game_root / "CFG/Main.dat"))))
    styles = {"Style." + node.path.removeprefix("ML/Style/").replace("/", "."): node
              for node in nodes if node.path.startswith("ML/Style/")}
    button = next(node for node in nodes
                  if node.unique_path == "ML#0/MainForm#0/Panel#0/GraphButton#0")
    props = config.effective(config.style_chain(button, styles))
    assert props["Name"][-1] == "New"
    assert props["Kind"][-1] == "Normal"
    assert props["KindHit"][-1] == "Rect"
    assert "UpOnlyDown" not in props and "OnPressCode" not in props
    assert any(node.path == "ML/GameSettings" for node in nodes)
    assert any(node.unique_path.startswith("ML#0/GameSettings#0/") and
               node.last("Name") == "Ok" for node in nodes)
    screen_source = (GAME / "fGameSettings2.cpp").read_text(encoding="utf-8")
    opening = method(screen_source, "void TfGameSettings2::OnOpen()",
                     "void TfGameSettings2::OnClose()")
    static_names = {part.split('"', 1)[0]
                    for part in opening.split('GetByName(u"')[1:]}
    resource_names = {node.last("Name") for node in nodes
                      if node.unique_path.startswith("ML#0/GameSettings#0/")}
    assert static_names <= resource_names, sorted(static_names - resource_names)
    print(f"private Main.dat New/GameSettings metadata: PASS ({len(static_names)} OnOpen controls)")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--game-root", type=Path)
    args = parser.parse_args()
    if args.game_root:
        check_private_resource(args.game_root)
    original_button = (GAME / "GI_GraphButton.cpp").read_text(encoding="utf-8")
    with tempfile.TemporaryDirectory(prefix="e2e3-generated-button-") as generated_dir:
        generated = generated_source(GAME / "GI_GraphButton.cpp",
                                     Path(generated_dir) / "GI_GraphButton.cpp")
        button_source = generated.read_text(encoding="utf-8")
    assert "ConsumeTimerTickChange() || WasDown" not in original_button
    assert "ConsumeTimerTickChange() || WasDown" in button_source
    loop_source = (GAME / "GI_MessageLoop.cpp").read_text(encoding="utf-8")
    form_source = (GAME / "fMainForm.cpp").read_text(encoding="utf-8")
    body = r'''
#include <cassert>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
constexpr std::u16string_view operator""sv(const char16_t* text, std::size_t count) {
  return {text, count};
}
struct WideRef { std::u16string_view value; std::u16string_view get() const { return value; } };
constexpr WideRef operator""_wref(const char16_t* text, std::size_t count) {
  return {{text, count}};
}
namespace WindowsSdk {
struct TPoint { int X{}, Y{}; };
inline constexpr std::uint32_t VK_CONTROL=0x11, VK_SHIFT=0x10, VK_MENU=0x12,
                               VK_F3=0x72, VK_ESCAPE=0x1b, VK_RETURN=0x0d;
}
namespace GI_MessageLoop {
struct TObjectGI;
struct TMessageLoopGI {
  TObjectGI* HoveredControl{};
  std::uint32_t TimerTick{100}, LastObservedTimerTick{99};
  std::uint8_t ConsumeTimerTickChange();
};
struct TObjectGI {
  TMessageLoopGI* MessageLoop{};
  bool hit{true}, occluded{};
  void ProcessLeftButtonDown(std::uint32_t, WindowsSdk::TPoint) {}
  void ProcessLeftButtonUp(std::uint32_t, WindowsSdk::TPoint) {}
  bool IsOccludedAtPoint(WindowsSdk::TPoint) { return occluded; }
};
}
namespace pas { template<class T> bool assigned(const T& value) { return bool(value); } }
namespace GR_Main {
struct Sound { void PlaySound(const std::u16string&) {} };
inline Sound sound;
inline Sound* SoundManager = &sound;
inline bool control{}, shift{}, alt{};
inline bool IsVirtualKeyDown(std::uint32_t key) {
  return key == WindowsSdk::VK_CONTROL ? control :
         key == WindowsSdk::VK_SHIFT ? shift : key == WindowsSdk::VK_MENU && alt;
}
inline bool IsInstallFeatureEnabled(std::u16string_view) { return false; }
}
namespace GI_GraphButton {
enum Kind { gbkNormal, gbkFix, gbkDisable, gbkFixDisable };
struct TGraphButtonGI : GI_MessageLoop::TObjectGI {
  Kind Kind{gbkNormal};
  bool Disabled{}, Down{}, UpOnlyDown{};
  std::u16string ClickSound;
  std::function<void(TGraphButtonGI*)> DownCallback, UpCallback;
  int press_codes{}, visuals{};
  bool HitTest(WindowsSdk::TPoint) { return hit; }
  void ExecuteOnPressCode() { ++press_codes; }
  void UpdateStateVisuals() { ++visuals; }
  void ProcessLeftButtonDown(std::uint32_t, WindowsSdk::TPoint);
  void ProcessLeftButtonUp(std::uint32_t, WindowsSdk::TPoint);
};
}
namespace fShip2 { inline constexpr int phkEmpty = 0; }
namespace Robot {
struct Interface { int Support() { return 0; } };
inline Interface* RobotInterface{};
}
namespace fSaveManager { inline constexpr int smmLoad = 1; }
namespace fMainForm { struct TfMainForm; }
namespace Globals {
struct Ship { int SelectedHoldKind{}; void* SelectedHoldItem{}; };
inline Ship ship;
inline Ship* ShipScreen = &ship;
inline int SaveManagerMode{};
}
namespace GlobalsV {
inline int RequestedScreenId{}, SaveManagerReturnScreenId{};
inline constexpr int screenNewGame = 23, screenSaveManager = 24;
inline int FormToId(fMainForm::TfMainForm*) { return 1; }
}
namespace fMainForm {
struct TfMainForm {
  int close_count{}, exit_code{};
  GI_GraphButton::TGraphButtonGI* new_button{};
  void RequestClose(int code) { ++close_count; exit_code = code; }
  void NewGameClicked(GI_MessageLoop::TObjectGI*);
  void MainPanelKeyDown(GI_MessageLoop::TObjectGI*, std::uint32_t);
  GI_MessageLoop::TObjectGI* GetByName(std::u16string_view) { return new_button; }
  GI_MessageLoop::TObjectGI* FindControlByPath(std::u16string_view) { return nullptr; }
  void TextQuestClicked(GI_MessageLoop::TObjectGI*) {}
  void RobotBattleClicked(GI_MessageLoop::TObjectGI*) {}
  void ArcadeBattleClicked(GI_MessageLoop::TObjectGI*) {}
  void ModsClicked(GI_MessageLoop::TObjectGI*) {}
  void ClosePopup() {}
  void QuitClicked(GI_MessageLoop::TObjectGI*) {}
  void SettingsClicked(GI_MessageLoop::TObjectGI*) {}
};
}
'''
    body += "\nnamespace GI_MessageLoop {\n" + method(loop_source,
                   "std::uint8_t TMessageLoopGI::ConsumeTimerTickChange()",
                   "// Starts at RootUiObject") + "\n}\n"
    body += "\nnamespace GI_GraphButton {\n" + method(button_source,
                   "void TGraphButtonGI::ProcessLeftButtonDown(std::uint32_t KeyState, WindowsSdk::TPoint Point)",
                   "void TGraphButtonGI::ProcessLeftButtonUp")
    body += method(button_source,
                   "void TGraphButtonGI::ProcessLeftButtonUp(std::uint32_t KeyState, WindowsSdk::TPoint Point)",
                   "void TGraphButtonGI::ProcessLeftButtonDoubleClick") + "\n}\n"
    body += "\nnamespace fMainForm {\n" + method(form_source,
                   "void TfMainForm::NewGameClicked(GI_MessageLoop::TObjectGI* Sender)",
                   "void TfMainForm::AchievementsClicked")
    body += method(form_source,
                   "void TfMainForm::MainPanelKeyDown(GI_MessageLoop::TObjectGI* Sender, std::uint32_t Key)",
                   "void TfMainForm::MainPanelMouseMove") + "\n}\n"
    body += r'''
int main() {
  using namespace GI_GraphButton;
  GI_MessageLoop::TMessageLoopGI loop;
  TGraphButtonGI button;
  fMainForm::TfMainForm form;
  form.new_button = &button;
  button.MessageLoop = &loop;
  loop.HoveredControl = &button;
  button.UpCallback = [&](TGraphButtonGI*) { form.NewGameClicked(nullptr); };
  const WindowsSdk::TPoint point{900, 290};
  button.ProcessLeftButtonDown(1, point);
  assert(button.Down && form.close_count == 0);
  button.ProcessLeftButtonUp(0, point);
  assert(form.close_count == 1 && form.exit_code == 1);
  assert(GlobalsV::RequestedScreenId == GlobalsV::screenNewGame);
  assert(Globals::ShipScreen->SelectedHoldKind == fShip2::phkEmpty);
  // A fresh button action in the same TimerTick must not be lost.
  form.exit_code = 0;
  button.ProcessLeftButtonDown(1, point);
  button.ProcessLeftButtonUp(0, point);
  assert(form.close_count == 2);
  // Holding A alone is not an action; a delayed release is exactly one.
  form.exit_code = 0;
  button.ProcessLeftButtonDown(1, point);
  assert(form.close_count == 2);
  loop.TimerTick += 200;
  button.ProcessLeftButtonUp(0, point);
  assert(form.close_count == 3);
  // UpOnlyDown ignores an unmatched release and accepts a valid press.
  form.exit_code = 0;
  button.UpOnlyDown = true;
  loop.TimerTick += 1;
  button.ProcessLeftButtonUp(0, point);
  assert(form.close_count == 3);
  button.ProcessLeftButtonDown(1, point);
  button.ProcessLeftButtonUp(0, point);
  assert(form.close_count == 4);
  button.UpOnlyDown = false;
  // Disabling, occlusion and misses must not trigger a transition.
  form.exit_code = 0;
  button.Disabled = true; button.Kind = gbkDisable;
  button.ProcessLeftButtonDown(1, point); button.ProcessLeftButtonUp(0, point);
  assert(form.close_count == 4);
  button.Disabled = false; button.Kind = gbkNormal;
  button.occluded = true;
  button.ProcessLeftButtonDown(1, point); button.ProcessLeftButtonUp(0, point);
  assert(form.close_count == 4);
  button.occluded = false; button.hit = false;
  button.ProcessLeftButtonDown(1, point); button.ProcessLeftButtonUp(0, point);
  assert(form.close_count == 4);
  button.hit = true; loop.HoveredControl = nullptr;
  button.ProcessLeftButtonDown(1, point); button.ProcessLeftButtonUp(0, point);
  assert(form.close_count == 4);
  loop.HoveredControl = &button;
  button.ProcessLeftButtonDown(1, point);
  button.hit = false; loop.HoveredControl = nullptr;  // Pointer left while held.
  button.ProcessLeftButtonUp(0, point);
  assert(form.close_count == 4);
  button.Down = false;
  // The original main-panel keyboard handler uses the same NewGameClicked.
  GR_Main::control = true;
  form.MainPanelKeyDown(nullptr, WindowsSdk::VK_RETURN);
  assert(form.close_count == 4);
  GR_Main::control = false;
  form.MainPanelKeyDown(nullptr, WindowsSdk::VK_RETURN);
  assert(form.close_count == 5 && GlobalsV::RequestedScreenId == GlobalsV::screenNewGame);
}
'''
    with tempfile.TemporaryDirectory(prefix="e2e3-original-ui-") as temp:
        source = Path(temp) / "test.cpp"
        output = Path(temp) / ("test.exe" if os.name == "nt" else "test")
        source.write_text(body, encoding="utf-8")
        subprocess.run([os.environ.get("CXX", "g++"), "-std=c++20", "-O0", str(source),
                        "-o", str(output)], check=True)
        subprocess.run([str(output)], check=True)
    print("original New Game methods with disposable Switch button fix: PASS")


if __name__ == "__main__":
    main()
