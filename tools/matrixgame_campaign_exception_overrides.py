"""Restore real caller/world state before propagating a C++ battle exception."""

def apply(name, text):
 if name not in ('Robot.cpp','fGov.cpp'):return text
 if name=='Robot.cpp':
  start=text.index('    std::int32_t FRun(');end=text.index('    void PAS_STDCALL RobotPlaySound(',start)
  edits=[
   ('        std::int32_t Result{};', '        std::int32_t Result{};\n        std::exception_ptr native_run_error{};'),
   ('                    } else {\n                        throw;\n                    }', '                    } else {\n                        native_run_error = std::current_exception();\n                    }'),
   ('                reinterpret_cast<GI_MessageLoop::TMessageLoopGI*>(GlobalsV::RegisteredScreens[GlobalsV::CurrentScreenId])->InvalidateViewport();', '                reinterpret_cast<GI_MessageLoop::TMessageLoopGI*>(GlobalsV::RegisteredScreens[GlobalsV::CurrentScreenId])->InvalidateViewport();\n                if (native_run_error) std::rethrow_exception(native_run_error);')]
 else:
  start=text.index('    void TfGov::OnOpen() {');end=text.index('    void TfGov::OnClose() {',start)
  edits=[
   ('                LoadPanel->OnOpen();\n                LoadPanel->SelectBackgroundStyle(3);', '                try {\n                LoadPanel->OnOpen();\n                LoadPanel->SelectBackgroundStyle(3);'),
   ('                Stage = 11;', '''                } catch (...) {
                    // The original Pascal failure path restores below. Native
                    // C++ exceptions must restore the same real snapshot before
                    // leaving OnOpen and destroying the campaign owner.
                    const auto native_battle_error = std::current_exception();
                    if (GlobalsV::MemorySnapshotActive) {
                        aSaveLoad::RestoreGameFromMemorySnapshot();
                    }
                    PendingTransition = 0;
                    std::rethrow_exception(native_battle_error);
                }
                Stage = 11;''')]
 body=text[start:end]
 for before,after in edits:
  assert body.count(before)==1,(name,before,body.count(before))
  body=body.replace(before,after)
 return '#include <exception>\n'+text[:start]+body+text[end:]
