"""Build the isolated Windows queue interleaving model and check the patch boundary."""
import json
import subprocess
from pathlib import Path

from matrixgame_save_commit_wait import apply
from matrixgame_windows_host_env import compiler, environment

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/matrixgame/save-queue-exclusion-test-20261123-3'
if OUT.exists():
    raise RuntimeError('output already exists')
OUT.mkdir(parents=True)
result = {'status': 'INCOMPLETE', 'scope': 'Windows model; real concurrent game calls and Switch NOT RUN'}
try:
    source = ROOT / 'build/matrixgame/windows-reference-rebuild-20261123/gov-frun-save-retry-link-1/aSaveLoad.cpp'
    transformed = apply(source.read_text(encoding='utf-8'))
    queue = transformed.index('SaveWriter->QueueSave(')
    lock = transformed.index('e2e5_save_commit_wait::SaveCall save_call;')
    wait = transformed.index('SaveWriter->WaitForIdle(WindowsSdk::INFINITE);', queue)
    check = transformed.index('GameSaveCommitted()', wait)
    end = transformed.index('if (aGalaxy::Galaxy->CampaignFlag183 != 0)', check)
    assert lock < transformed.index('GameState = pas::construct_call') < queue < wait < check < end
    (OUT / 'transformed-aSaveLoad.cpp').write_text(transformed, encoding='utf-8')
    env = environment(OUT)
    exe = OUT / 'save-queue-test.exe'
    command = [str(compiler(env)), '/nologo', '/std:c++17', '/EHsc', '/W4',
               '/I' + str(ROOT / 'port/switch/platform'),
               str(ROOT / 'tests/test_e2e5_save_queue_exclusion.cpp'), '/Fe' + str(exe),
               '/Fo' + str(OUT / 'save-queue-test.obj')]
    build = subprocess.run(command, cwd=OUT, env=env, capture_output=True, text=True, errors='replace')
    (OUT / 'build.log').write_text(build.stdout + build.stderr, encoding='utf-8')
    assert build.returncode == 0, 'compile failed; see build.log'
    run = subprocess.run([str(exe)], cwd=OUT, capture_output=True, text=True, errors='replace', timeout=30)
    (OUT / 'runtime.log').write_text(run.stdout + run.stderr, encoding='utf-8')
    assert run.returncode == 0 and 'PASS (model)' in run.stdout
    result.update(status='PASS; controlled interleaving model and source boundary', build_exit=build.returncode, runtime_exit=run.returncode)
finally:
    (OUT / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
