"""Link an isolated reader for saves created by the dispatcher reference."""
import hashlib
import json
import subprocess
import sys
from pathlib import Path
from matrixgame_windows_host_env import environment

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'build/matrixgame/windows-reference-rebuild-20261123'

def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def main():
    unknown_snapshot = sys.argv[1:] == ['--snapshot-test-unknown']
    snapshot_test = sys.argv[1:] in (['--snapshot-test'], ['--snapshot-test-fixed']) or unknown_snapshot
    fixed_buffer = sys.argv[1:] in (['--buffer-test-fixed'], ['--snapshot-test-fixed']) or unknown_snapshot
    buffer_test = sys.argv[1:] == ['--buffer-test'] or fixed_buffer or snapshot_test
    preflight = sys.argv[1:] == ['--preflight'] or buffer_test
    item_calibrate_v4 = sys.argv[1:] == ['--gov-item-calibrate-v4']
    item_trace_v4 = sys.argv[1:] == ['--gov-item-trace-v4'] or item_calibrate_v4
    item_dangling = sys.argv[1:] == ['--gov-item-dangling']
    item_calibrate = sys.argv[1:] == ['--gov-item-calibrate'] or item_dangling or item_calibrate_v4
    item_trace_v3 = sys.argv[1:] == ['--gov-item-trace-v3'] or item_calibrate or item_trace_v4
    item_trace_v2 = sys.argv[1:] == ['--gov-item-trace-v2'] or item_trace_v3
    item_trace = sys.argv[1:] == ['--gov-item-trace'] or item_trace_v2
    fixed_gov = sys.argv[1:] == ['--gov-fixed'] or item_trace
    gov_preflight = sys.argv[1:] == ['--gov-preflight'] or fixed_gov
    preflight = preflight or gov_preflight
    if sys.argv[1:] and not preflight:
        raise RuntimeError('unknown arguments')
    out = BASE / ('gov-frun-item-calibrate-link-3' if item_calibrate_v4 else 'gov-frun-gov-item-trace-link-4' if item_trace_v4 else 'gov-frun-item-dangling-link-1' if item_dangling else 'gov-frun-item-calibrate-link-2' if item_calibrate else 'gov-frun-gov-item-trace-link-3' if item_trace_v3 else 'snapshot-prep-unknown-link-1' if unknown_snapshot else 'snapshot-prep-fixed-link-1' if snapshot_test and fixed_buffer else 'snapshot-prep-baseline-link-2' if snapshot_test else 'save-buffer-fixed-link-2' if fixed_buffer else 'save-buffer-baseline-link-1' if buffer_test else 'gov-frun-gov-item-trace-link-2' if item_trace_v2 else 'gov-frun-gov-item-trace-link-1' if item_trace else 'gov-frun-gov-preflight-link-4' if fixed_gov else 'gov-frun-gov-preflight-link-2' if gov_preflight else 'gov-frun-save-preflight-link-1' if preflight else 'gov-frun-reload-link-2')
    if out.exists():
        raise RuntimeError('output already exists')
    out.mkdir()
    env = environment(out)
    prior = BASE / 'gov-frun-link-3'
    old = json.loads((prior / 'result.json').read_text(encoding='utf8'))
    result = {'status': 'INCOMPLETE', 'commands': []}

    def run(label, command):
        command = list(map(str, command))
        process = subprocess.run(command, cwd=ROOT, env=env, capture_output=True, text=True, encoding='utf8', errors='replace')
        (out / (label + '.log')).write_text(process.stdout + process.stderr, encoding='utf8')
        result['commands'].append({'label': label, 'exit': process.returncode, 'command': command})
        print(label, process.returncode, flush=True)
        if process.returncode:
            raise RuntimeError('see ' + label + '.log')

    try:
        original = BASE / 'gov-frun-two-link/campaign_frun_two.cpp'
        source_text = original.read_text(encoding='utf8')
        anchor = '#include "units/fGov.hpp"'
        assert source_text.count(anchor) >= 1
        source_text = source_text.replace(anchor, anchor + '\n#include "types/fSaveManager.hpp"\n#include "units/aSaveLoad.hpp"\n#include <cstdlib>', 1)
        if gov_preflight:
            source_text = source_text.replace(anchor, anchor + '\n#include "types/fGalaxy2.hpp"\n#include "units/fGalaxy2.hpp"\n#include "types/aScript.hpp"\n#include "types/fEquipmentShop.hpp"\n#include "units/fEquipmentShop.hpp"\n#include "types/aItem.hpp"', 1)
        if item_trace:
            source_text = source_text.replace(anchor, anchor + '\n#include "matrixgame_item_lifetime_probe.hpp"', 1)
        if item_calibrate:
            source_text = source_text.replace(anchor, anchor + '\n#include "units/aItem.hpp"', 1)
        if buffer_test:
            source_text = source_text.replace(anchor, anchor + '\n#include "matrixgame_save_buffer_probe.hpp"\n#include <fstream>\n#include <iterator>', 1)
        if snapshot_test:
            source_text = source_text.replace(anchor, anchor + '\n#include "units/EC_Buf.hpp"', 1)
        if unknown_snapshot:
            source_text = source_text.replace(anchor, anchor + '\n#include "types/aScript.hpp"', 1)
        if item_trace_v2:
            source_text = source_text.replace(anchor, anchor + '\n#include "units/EC_CacheBuf.hpp"\n#include "units/EC_Cache.hpp"\n#include "types/EC_Buf.hpp"\n#include <fstream>', 1)
        if preflight:
            source_text = source_text.replace(anchor, anchor + '\n#include "rangers_session_log.hpp"', 1)
            log_anchor = ' HostClean();MatrixClean();Empty();const auto cwd=std::filesystem::current_path();'
            assert source_text.count(log_anchor) == 1
            log_setup = (' auto logdir=std::filesystem::path(output_root)/"rangers-user"/"logs";'
                         'std::filesystem::create_directories(logdir);'
                         'm::InitializeRangersLog((logdir/"game.log").string().c_str());')
            source_text = source_text.replace(log_anchor, log_anchor + '\n' + log_setup, 1)
        begin = '   auto* gov=Globals::GovernmentScreen;gov->InitializeLayout();'
        end = '  GlobalsV::CurrentScreenId=previous_screen;'
        assert source_text.count(begin) == source_text.count(end) == 1
        before, rest = source_text.split(begin, 1)
        _, after = rest.split(end, 1)
        scenario = (ROOT / ('tests/matrixgame_windows_snapshot_unknown.inc' if unknown_snapshot else 'tests/matrixgame_windows_snapshot_failure.inc' if snapshot_test else 'tests/matrixgame_windows_save_buffer_failure.inc' if buffer_test else 'tests/matrixgame_windows_gov_save_preflight.inc' if gov_preflight else 'tests/matrixgame_windows_save_preflight.inc' if preflight else 'tests/matrixgame_windows_autosave_reload.inc')).read_text(encoding='utf8')
        if item_trace:
            marker = '    std::printf("SAVE_PREFLIGHT quest_item_opaque=%llx\\n", static_cast<unsigned long long>(value));'
            assert scenario.count(marker) == 1
            scenario = scenario.replace(marker, marker + '\n    matrixgame_item_lifetime_probe::Report(value);', 1)
            if item_trace_v3:
                scenario = scenario.replace(marker, marker + '\n    matrixgame_item_lifetime_probe::ReportAssignment(cell, value);', 1)
        if item_trace_v2:
            marker = 'std::puts("SAVE_PREFLIGHT original_Gov_and_previews=PASS");'
            assert scenario.count(marker) == 1
            scenario = scenario.replace(marker, marker + '\n' + (ROOT / 'tests/matrixgame_dump_script.inc').read_text(encoding='utf8'), 1)
        if item_calibrate:
            marker = 'const auto path = pas::concat_wide({GR_Main::GetGameUserDirectory(), u"Save\\\\InventoryProbe.sav"});'
            assert scenario.count(marker) == 1
            calibration = '''EC_Expression::TVarEC* tracked = nullptr;
for (int si=0; si<pas::list_count(aGalaxy::Galaxy->Scripts); ++si) {
 auto* script=pas::list_at<aScript::TScript>(aGalaxy::Galaxy->Scripts,si);
 if(script->ScriptFileName != u"Script.PC_fem_rangers"_w) continue;
 auto* vars=script->InitCode->LocalVar;
 for(int vi=0;vi<vars->Count;++vi){auto* cell=EC_Expression::TVarArrayEC_GetItem(vars,vi);
  if(cell->Name==u"quest_item"_w && cell->Kind==EC_Expression::vkDword) tracked=cell;}
}
assert(tracked); const auto original=tracked->GetDword();
auto* item=pas::construct_call<aItem::TUselessItem>(aItem::TUselessItem_Create);
const auto address=reinterpret_cast<std::uintptr_t>(item); const auto id=item->Id;
tracked->SetDword(address); matrixgame_item_lifetime_probe::ReportAssignment(tracked,address);
pas::free(item); matrixgame_item_lifetime_probe::Report(address); tracked->SetDword(original);
std::printf("ITEM_CALIBRATION restored=PASS id=%u\\n",unsigned(id));
'''
            if item_dangling:
                calibration = calibration.replace('pas::free(item); matrixgame_item_lifetime_probe::Report(address); tracked->SetDword(original);', '''pas::free(item); matrixgame_item_lifetime_probe::Report(address);
const auto dangling_path=pas::concat_wide({GR_Main::GetGameUserDirectory(),u"Save\\\\DanglingProbe.sav"});
assert(!aSaveLoad::SaveGameToFile(dangling_path,u"dangling test"_w));
assert(tracked->GetDword()==address); tracked->SetDword(original);
std::puts("ITEM_DANGLING real_preflight_rejected=PASS");''', 1)
            if item_calibrate_v4:
                calibration = calibration.replace('pas::free(item); matrixgame_item_lifetime_probe::Report(address); tracked->SetDword(original);', 'pas::free(item); matrixgame_item_lifetime_probe::Report(address); matrixgame_item_lifetime_probe::ReportAssignment(tracked,address); tracked->SetDword(original);', 1)
            scenario = scenario.replace(marker, calibration + '\n' + marker, 1)
        source_text = before + scenario + '\n  }\n' + end + after
        source = out / 'campaign_frun_reload.cpp'
        source.write_text(source_text, encoding='utf8')
        (out / 'hosted_map_build.hpp').write_text('inline constexpr char hosted_map_source_sha[]="' + sha(source) + '";\n', encoding='utf8')
        command = next(x for x in old['commands'] if x['label'] == 'compile-campaign_frun')['command'][:]
        command.insert(1, '/I' + str(out))
        if item_trace or buffer_test:
            command.insert(1, '/I' + str(ROOT / 'tests'))
        command[command.index('/c') + 1] = str(source)
        obj = out / 'campaign_frun_reload.obj'
        command = [('/Fo' + str(obj)) if x.startswith('/Fo') else x for x in command]
        run('compile', command)
        sdk_entry = next(x for x in old['commands'] if x['label'] == 'compile-campaign_sdk_main')['command'][:]
        sdk_original = Path(sdk_entry[sdk_entry.index('/c') + 1])
        sdk_text = sdk_original.read_text(encoding='utf8')
        original_check = 'if(!CallerGammaVerified() || device.OutstandingStateLeases() || window.OutstandingTimerLeases())return 12;'
        assert sdk_text.count(original_check) == 1
        sdk_text = sdk_text.replace(original_check, 'if(device.OutstandingStateLeases() || window.OutstandingTimerLeases())return 12;')
        sdk_source = out / 'campaign_sdk_reload.cpp'
        sdk_source.write_text(sdk_text, encoding='utf8')
        sdk_obj = out / 'campaign_sdk_reload.obj'
        sdk_entry[sdk_entry.index('/c') + 1] = str(sdk_source)
        sdk_entry = [('/Fo' + str(sdk_obj)) if x.startswith('/Fo') else x for x in sdk_entry]
        run('compile-sdk-reload', sdk_entry)
        old_obj = prior / 'campaign_frun.obj'
        old_sdk_obj = prior / 'campaign_sdk_main.obj'
        items = (prior / 'link.rsp').read_text(encoding='utf8').splitlines()
        assert sum(line.strip('"') == str(old_obj) for line in items) == 1
        assert sum(line.strip('"') == str(old_sdk_obj) for line in items) == 1
        rsp = out / 'link.rsp'
        replacements = {str(old_obj): obj, str(old_sdk_obj): sdk_obj}
        if fixed_gov:
            old_galaxy = str(BASE / 'rangers-windows-2/objects/original/aGalaxy.obj')
            assert sum(line.strip('"') == old_galaxy for line in items) == 1
            replacements[old_galaxy] = BASE / ('item-roots-fixed-1/aGalaxy-fixed.obj' if item_trace else 'ship-reference-fixed-2/aGalaxy-fixed.obj')
            assert replacements[old_galaxy].is_file()
        if item_trace:
            item_original = BASE / 'rangers-windows-2/source/aItem.cpp'
            item_text = item_original.read_text(encoding='utf8')
            item_text = '#include "matrixgame_item_lifetime_probe.hpp"\n' + item_text
            create_marker = '        Self->NameOverride = pas::WideString();\n'
            destroy_marker = '    void TItem_Destroy(TItem* Self) {\n'
            assert item_text.count(create_marker) == item_text.count(destroy_marker) == 1
            item_text = item_text.replace(create_marker, create_marker + '        matrixgame_item_lifetime_probe::Created(Self, static_cast<std::uint32_t>(Self->Id));\n', 1)
            item_text = item_text.replace(destroy_marker, destroy_marker + '        matrixgame_item_lifetime_probe::Destroyed(Self, static_cast<std::uint32_t>(Self->Id));\n', 1)
            item_source = out / 'aItem-traced.cpp'; item_source.write_text(item_text, encoding='utf8')
            item_obj = out / 'aItem-traced.obj'
            object_flags = json.loads((BASE / 'rangers-windows-2/result.json').read_text(encoding='utf8'))['flags']
            run('compile-item-trace', [sdk_entry[0], '/I' + str(ROOT / 'tests'), *object_flags, '/c', item_source, '/Fo' + str(item_obj)])
            old_item = str(BASE / 'rangers-windows-2/objects/original/aItem.obj')
            assert sum(line.strip('"') == old_item for line in items) == 1
            replacements[old_item] = item_obj
        if item_trace_v3:
            expression = BASE / 'rangers-windows-2/source/EC_Expression.cpp'
            expression_text = expression.read_text(encoding='utf8')
            marker = '    void TVarEC::SetDword(std::uintptr_t Value) {'
            assert expression_text.count(marker) == 1
            expression_text = '#include "matrixgame_item_lifetime_probe.hpp"\n' + expression_text.replace(marker, marker + '\n        if (Name == u"quest_item"_w) matrixgame_item_lifetime_probe::Assigned(this, Value);', 1)
            expression_source = out / 'EC_Expression-traced.cpp'; expression_source.write_text(expression_text, encoding='utf8')
            expression_obj = out / 'EC_Expression-traced.obj'
            run('compile-expression-trace', [sdk_entry[0], '/I' + str(ROOT / 'tests'), *object_flags, '/c', expression_source, '/Fo' + str(expression_obj)])
            old_expression = str(BASE / 'rangers-windows-2/objects/original/EC_Expression.obj')
            assert sum(line.strip('"') == old_expression for line in items) == 1
            replacements[old_expression] = expression_obj
        if buffer_test:
            object_flags = json.loads((BASE / 'rangers-windows-2/result.json').read_text(encoding='utf8'))['flags']
            for unit in ('EC_Buf', 'aSaveLoad'):
                unit_text = (BASE / ('rangers-windows-2/source/' + unit + '.cpp')).read_text(encoding='utf8')
                if fixed_buffer and unit == 'aSaveLoad':
                    from matrixgame_save_buffer_ownership import apply
                    unit_text = apply(unit_text)
                    if snapshot_test:
                        from matrixgame_snapshot_preparation import apply as apply_snapshot
                        unit_text = apply_snapshot(unit_text)
                    marker = '                save_buffers.queued = true;'
                    assert unit_text.count(marker) == 1
                    unit_text = unit_text.replace(marker, marker + '\n                if (matrixgame_save_buffer_probe::fail_before == 3) pas::raise(pas::make_exception<pas::Exception>("injected post-queue failure"_a));', 1)
                unit_text = '#include "matrixgame_save_buffer_probe.hpp"\n' + unit_text
                if unit == 'EC_Buf':
                    for marker, hook in (('    void TBufEC_Create(TBufEC* Self) {\n', 'matrixgame_save_buffer_probe::Created();'),
                                         ('    void TBufEC_Destroy(TBufEC* Self) {\n', 'matrixgame_save_buffer_probe::Destroyed();')):
                        assert unit_text.count(marker) == 1
                        unit_text = unit_text.replace(marker, marker + '        ' + hook + '\n', 1)
                else:
                    for marker, point in (('                aGalaxy::Galaxy->SaveToBuffer(GameState);', 1),
                                          ('                SaveWriter->QueueSave(FileName, Header, Preview, SecondaryPreview, GameState, Films);', 2)):
                        assert unit_text.count(marker) == 1
                        injection = ('                if (matrixgame_save_buffer_probe::fail_before == ' + str(point) + ') '
                                     'pas::raise(pas::make_exception<pas::Exception>("injected pre-queue failure"_a));\n')
                        unit_text = unit_text.replace(marker, injection + marker, 1)
                    if snapshot_test:
                        first = ('        } snapshot_preparation{aGalaxy::Galaxy, aGalaxy::Galaxy->SaveCount};' if fixed_buffer else
                                 '        MemorySnapshotBuffer = pas::construct_call<EC_Buf::TBufEC>(EC_Buf::TBufEC_Create);')
                        second = '        --aGalaxy::Galaxy->SaveCount;'
                        for marker, point in ((first, 4), (second, 5)):
                            assert unit_text.count(marker) == 1
                            injection = ('\n        if (matrixgame_save_buffer_probe::fail_before == ' + str(point) + ') '
                                         'pas::raise(pas::make_exception<pas::Exception>("injected snapshot failure"_a));')
                            unit_text = unit_text.replace(marker, marker + injection, 1)
                unit_source = out / (unit + '-probe.cpp'); unit_source.write_text(unit_text, encoding='utf8')
                unit_obj = out / (unit + '-probe.obj')
                run('compile-' + unit, [sdk_entry[0], '/I' + str(ROOT / 'tests'), *object_flags, '/c', unit_source, '/Fo' + str(unit_obj)])
                old_unit = str(BASE / ('rangers-windows-2/objects/original/' + unit + '.obj'))
                assert sum(line.strip('"') == old_unit for line in items) == 1
                replacements[old_unit] = unit_obj
        rsp.write_text('\n'.join('"' + str(replacements[x.strip('"')]) + '"' if x.strip('"') in replacements else x for x in items), encoding='utf8')
        link = next(x for x in old['commands'] if x['label'] == 'link')['command'][:]
        binary = out / 'gov-frun-reload.exe'
        link = [('@' + str(rsp)) if x.startswith('@') else ('/Fe' + str(binary)) if x.startswith('/Fe') else x for x in link]
        run('link', link)
        result.update(status='PASS; reload reference linked; runtime NOT RUN', binary_sha256=sha(binary), source_sha256=sha(source))
    finally:
        (out / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf8')

if __name__ == '__main__':
    main()
