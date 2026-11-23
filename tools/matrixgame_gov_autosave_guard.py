"""Abort planet-battle entry when its original autosave is rejected."""

def apply(name, text):
    if name != 'fGov.cpp':
        return text
    functions = ('StartPlanetBattleWithoutSupport', 'StartPlanetBattleWithReinforcements',
                 'StartPlanetBattleWithBombardment')
    old = '        aSaveLoad::SaveGameToFile(fSaveManager::TfSaveManager::GetAutoSavePath(), u"as"_w);'
    new = '''        const bool autosave_ok = [&] {
            srhd_awa::platform::e2e5_save_commit_wait::Scope wait_for_autosave;
            return aSaveLoad::SaveGameToFile(fSaveManager::TfSaveManager::GetAutoSavePath(), u"as"_w);
        }();
        if (!autosave_ok) {
            // The saver has already emitted the original localized warning.
            // Keep the caller screen alive and make the failure visible now.
            const auto& message = GR_Main::LanguageDataConfig->GetParamByPathOrMarker(u"FormSaveManager.SaveError"_wref.get());
            GI_MessageBox::ShowMessageBoxGI(this, message, GI_MessageBox::mbgOK | GI_MessageBox::mbgError, 0, 0, 0);
            return;
        }'''
    for function in functions:
        start = text.index('    void TfGov::' + function + '(')
        end = text.index('\n    }', start)
        body = text[start:end]
        if body.count(old) != 1:
            raise RuntimeError('unexpected autosave in ' + function)
        text = text[:start] + body.replace(old, new) + text[end:]
    return '#include "e2e5_save_commit_wait.hpp"\n' + text
