"""Package an isolated, hardware-pending integrated Switch test build."""
import hashlib
import argparse
import json
import re
import shlex
import subprocess
from pathlib import Path

from matrixgame_diagnostic_package import verify_nro

ROOT = Path(__file__).resolve().parents[1]
SDK = Path('C:/devkitPro')
ELF = ROOT / 'build/matrixgame/integrated-app-20261123/matrixgame-integrated-app.elf'
OUTPUT = ROOT / 'dist/integrated-test-20261123-1'
GIT = Path('C:/Program Files/Git/cmd/git.exe')

def sha(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()

def call(args):
    return subprocess.run(list(map(str, args)), cwd=ROOT, check=True, capture_output=True, text=True, encoding='utf-8', errors='replace').stdout

def linked_archives(elf):
    # Preserve the actual CMake/Ninja link inputs, including dependency hashes.
    ninja = (elf.parent / 'build.ninja').read_text(encoding='utf-8')
    match = re.search(r'^build ' + re.escape(elf.name) + r':[^\n]*\n(?:(?!build ).*\n)*?  LINK_LIBRARIES = (.*)$', ninja, re.M)
    if not match:
        raise RuntimeError('ELF link inputs absent from build.ninja')
    link_line = match.group(1)
    archive_tool = SDK / 'devkitA64/bin/aarch64-none-elf-ar.exe'
    compiler = SDK / 'devkitA64/bin/aarch64-none-elf-g++.exe'
    roots = [SDK / 'libnx/lib', SDK / 'portlibs/switch/lib']
    archives = {}
    unresolved = []
    for token in shlex.split(link_line):
        if token.startswith('-l') and len(token) > 2:
            name = 'lib' + token[2:] + '.a'
            path = next((root / name for root in roots if (root / name).is_file()), None)
            if path is None:
                candidate = Path(call([compiler, '-print-file-name=' + name]).strip())
                path = candidate if candidate.is_absolute() and candidate.is_file() else None
            if path is None:
                unresolved.append(token)
                continue
        elif token.endswith('.a'):
            path = Path(token)
            if not path.is_absolute():
                path = elf.parent / path
        else:
            continue
        path = path.resolve()
        assert path.is_file(), path
        key = str(path)
        if key not in archives:
            archives[key] = {'sha256': sha(path), 'members': call([archive_tool, 't', path]).splitlines()}
    return {'link_libraries': link_line, 'archives': archives, 'unresolved_library_flags': sorted(set(unresolved)),
            'scope': 'explicit LINK_LIBRARIES; compiler/specs implicit libraries not enumerated'}

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--elf',type=Path,default=ELF)
    parser.add_argument('--output',type=Path,default=OUTPUT)
    parser.add_argument('--e2e516',action='store_true',help='require original E2E516 ProgramMain and native Robot initialization hook')
    parser.add_argument('--user-profile',default='integrated-test')
    parser.add_argument('--frame-trace',action='store_true')
    args=parser.parse_args()
    if not re.fullmatch(r'[A-Za-z0-9_-]+',args.user_profile):
        raise ValueError('Invalid isolated profile name')
    elf=args.elf.resolve();output=args.output.resolve()
    if not elf.is_relative_to((ROOT/'build').resolve()) or not output.is_relative_to((ROOT/'dist').resolve()):
        raise ValueError('ELF and output must stay in the workspace build and dist directories')
    if output.exists():
        raise RuntimeError('Output already exists; preserve the previous package')
    assert elf.is_file()
    cache=(elf.parent/'CMakeCache.txt').read_text(encoding='utf-8')
    assert 'MATRIXGAME_INTEGRATED_ISOLATED_TEST:BOOL=ON' in cache
    assert f'MATRIXGAME_INTEGRATED_OUTPUT_NAME:STRING={elf.stem}' in cache
    archive_match=re.search(r'^BRIDGE_RANGERS_HOST_ARCHIVE:FILEPATH=(.+)$',cache,re.M)
    assert archive_match, 'Rangers host archive absent from CMake cache'
    rangers_archive=Path(archive_match.group(1).strip()).resolve()
    assert rangers_archive.is_file() and rangers_archive.is_relative_to((ROOT/'build').resolve())
    readelf=SDK/'devkitA64/bin/aarch64-none-elf-readelf.exe'
    nm=SDK/'devkitA64/bin/aarch64-none-elf-nm.exe'
    notes=call([readelf,'-n',elf]);match=re.search(r'Build ID:\s*([0-9a-f]+)',notes,re.I)
    assert match, 'ELF has no GNU Build ID'
    undefined=call([nm,'--undefined-only',elf]).strip();assert not undefined,undefined
    symbols=call([nm,'-C','--defined-only',elf])
    if args.frame_trace:
        strings=call([SDK/'devkitA64/bin/aarch64-none-elf-strings.exe',elf])
        assert 'NATIVE_FRAME_TRACE' in strings and '__wrap___cxa_throw' in symbols
    if args.user_profile != 'integrated-test':
        strings=call([SDK/'devkitA64/bin/aarch64-none-elf-strings.exe',elf])
        assert 'sdmc:/switch/space-rangers-hd-a-war-apart/'+args.user_profile in strings, 'profile not present in ELF'
    assert '__wrap_nwindowGetDefault' in symbols, 'integrated VI window wrapper absent'
    assert not re.search(r'\b__nx_win_init\b', symbols), 'libnx default-window startup conflicts with integrated VI owner'
    objdump=SDK/'devkitA64/bin/aarch64-none-elf-objdump.exe'
    presentation=call([objdump,'-d','--disassemble=_ZN7GR_Main19PresentScreenBufferEv',elf])
    assert 'PresentRgb565' in presentation, 'integrated frame presentation does not reach portable renderer'
    assert 'IDirect3D' not in presentation, 'integrated frame presentation unexpectedly calls Direct3D'
    required = ['Robot::FRun(', 'Globals::RunMainScreenStateLoop()', 'RunOriginalNativeLoop()',
                'NativeInitRuntime::SaveOriginalGameResult(']
    if args.e2e516:
        required += ['Rangers::ProgramMain()', '__wrap__ZN5Robot22InitializeRobotRuntimeEv',
                     'controller_ui::Process(']
        program=call([objdump,'-d','--disassemble=_ZN7Rangers11ProgramMainEv',elf])
        assert '__wrap__ZN5Robot22InitializeRobotRuntimeEv' in program, 'original ProgramMain bypasses native Robot hook'
    else:
        required += ['RangersIntegratedSession::RunInitialScreens()']
    for name in required:
        assert name in symbols,name
    head=call([GIT,'rev-parse','HEAD']).strip()
    dirty=call([GIT,'status','--short','--untracked-files=all','--ignore-submodules=all']).splitlines()
    submodules=call([GIT,'submodule','status','--recursive']).splitlines()
    paths=[
        'port/matrixgame/native-engine/rangers_switch_main.cpp',
        'port/matrixgame/native-engine/rangers_e2e516_main.cpp',
        'port/switch/platform/runtime_platform.cpp',
        'port/switch/platform/e2e5_cfg_bootstrap.hpp',
        'port/switch/platform/e2e515_perf.cpp',
        'port/switch/platform/e2e515_perf.hpp',
        'port/matrixgame/native-engine/native_frame_scope.hpp',
        'port/matrixgame/native-engine/native_frame_profiler.hpp',
        'port/matrixgame/native-engine/native_loop_switch.cpp',
        'tools/matrixgame_full_game_overrides.py',
        'tools/e2e516_cfg_copy_sdk.py',
        'tools/e2e516_native_window_overrides.py',
        'tools/e2e516_native_window_sdk.py',
        'port/matrixgame/native-engine/CMakeLists.txt',
        'port/matrixgame/native-engine/platform_switch.cpp',
        'port/matrixgame/native-engine/native_input_switch.cpp',
        'port/matrixgame/native-engine/native_stick_policy.hpp',
        'port/matrixgame/native-engine/native_rangers_canvas.cpp',
        'port/matrixgame/native-engine/rangers_input_queue.hpp',
        'port/matrixgame/native/rangers_ui_context.hpp',
        'port/matrixgame/native-engine/rangers_com_throw_trace.cpp',
        'port/matrixgame/native-engine/rangers_session_log.hpp',
        'port/matrixgame/native-engine/rangers_integrated_session.cpp',
        'port/matrixgame/native-engine/rangers_exception_boundary.hpp',
        'port/matrixgame/native-engine/native_rangers_interface.cpp',
        'port/matrixgame/native/rangers_ui_context.cpp',
        'port/matrixgame/native/rangers_screen_loop.cpp',
        'port/matrixgame/native/rangers_world_shutdown.cpp',
        str(rangers_archive.relative_to(ROOT)),
        'port/switch/platform/e2e5_save_inventory.hpp',
        'port/switch/platform/e2e515_item_references.hpp',
        'tools/matrixgame_save_buffer_ownership.py',
        'tools/matrixgame_snapshot_preparation.py',
        'tools/matrixgame_gov_autosave_guard.py',
        'tools/matrixgame_save_commit_wait.py',
        'port/switch/platform/e2e5_save_commit_wait.hpp',
        'port/switch/build/e2e-game-perf515/okgf-fast24-no-tls/libokgf.a',
        'port/switch/build/e2e-game-perf515/okgf-fast24-no-tls/vendor/softfloat/libokgf_softfloat.a',
    ]
    inputs={p:sha(ROOT/p) for p in paths}
    dependencies=linked_archives(elf)
    icon=SDK/'libnx/default_icon.jpg';nacptool=SDK/'tools/bin/nacptool.exe';elf2nro=SDK/'tools/bin/elf2nro.exe'
    output.mkdir(parents=True)
    nacp=output/'control.nacp';nro=output/'srhd-integrated-test.nro'
    title='Space Rangers HD Integrated TEST'
    call([nacptool,'--create',title,'SRHD AWA port','0.1.0-test',nacp])
    call([elf2nro,elf,nro,'--icon='+str(icon),'--nacp='+str(nacp)])
    validation=verify_nro(nro.read_bytes(),icon.read_bytes(),nacp.read_bytes(),title)
    readme=f'''# Space Rangers HD — интегрированный TEST\n\n**SDK-сборка; физический Switch не проверен. Это не релиз и не подтверждение играбельности.**\n\nBuild ID: `{match.group(1)}`\nSHA-256 NRO: `{sha(nro)}`\n\n1. Скопируйте `srhd-integrated-test.nro` в отдельную папку внутри `sdmc:/switch/`; основную версию не заменяйте.\n2. Используйте имеющиеся игровые ресурсы в `sdmc:/switch/space-rangers-hd-a-war-apart/game`. Коммерческие ресурсы в пакет не входят.\n3. Запустите через hbmenu в application mode. В главном меню начните новую тестовую кампанию; используются отдельные конфигурация и сохранения в `sdmc:/switch/space-rangers-hd-a-war-apart/integrated-test/`.\n4. Покиньте станцию, долетите до населённой планеты и выполните посадку. Откройте правительство и при наличии предложения выберите планетарную битву. При отсутствии предложения запишите достигнутую стадию.\n5. Доведите бой до штатного результата или сдачи и проверьте возврат к планете. При ошибке или зависании прекратите тест, выйдите штатными средствами консоли, если это возможно, и не продолжайте на важной кампании.\n6. Передайте каталог `sdmc:/switch/space-rangers-hd-a-war-apart/integrated-test/logs/`, если он создан, и сообщите Build ID, SHA-256, последний экран и действия, последнюю диагностическую фазу, результат и были ли ошибки. Если журналов нет, сообщите об этом.\n\nПерезагрузка runtime/mod пока не поддержана. Запись на SD, управление, работа GPU, возврат из боя и долгие сессии на физической консоли не проверены. Автоматическое восстановление отсоединённого снимка при аварийном выходе ещё не реализовано; используйте только тестовую кампанию.\n'''
    readme += '''\nПосле каждого запуска скопируйте журналы в отдельную папку на компьютере: файлы на SD дописываются между запусками. Рядом сохраните запись:\n\n```text\nДата и время:\nBuild ID:\nSHA-256 NRO:\nПоследний экран и достигнутая стадия:\nПоследняя диагностическая фаза из журнала:\nРезультат: PASS / FAIL / прервано / не определён\nДействия и описание ошибки:\n```\n\nЕсли приложение остановилось до создания журналов, укажите это и последний наблюдавшийся экран.\n'''
    readme=readme.replace('/integrated-test/',f'/{args.user_profile}/')
    (output/'README.ru.md').write_text(readme,encoding='utf-8')
    manifest={'status':'SDK PACKAGED; EXPERIMENTAL; HARDWARE_PENDING','head':head,'dirty':dirty,'submodules':submodules,
              'build_id':match.group(1),'elf_path':str(elf),'elf_sha256':sha(elf),'nro_sha256':sha(nro),
              'nacp_sha256':sha(nacp),'nro_validation':validation,'inputs_sha256':inputs,
              'link_dependencies':dependencies,
              'settings':{'isolated_test_root':True,'user_profile':args.user_profile,'game_resources_included':False,'runtime_reload':'NOT_IMPLEMENTED'},
              'evidence':{'undefined_symbols':0,'hardware':'NOT_RUN','full_windows_two_battles':'NOT_RUN'}}
    (output/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    (output/'SHA256SUMS.txt').write_text(''.join(f'{sha(p)}  {p.name}\n' for p in sorted(output.iterdir()) if p.is_file() and p.name!='SHA256SUMS.txt'),encoding='utf-8')
    print(json.dumps({'nro':str(nro),'sha256':sha(nro),'build_id':match.group(1),'validation':validation},indent=2))

if __name__=='__main__':
    main()
