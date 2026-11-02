"""Audit real PNG/private output, completed-slot policy and SDK integration."""
import argparse
import json
import subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_diagnostic_package import verify_nro
from matrixgame_screenshot_png_verify import verify_fixture
BASE=ROOT/'build/matrixgame/e2e567-screenshot'
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);output=parser.parse_args().output
    names=('host','arm64','windows','readback-regression/host','readback-regression/arm64','sdk')
    profiles={name:read(BASE/name/'result.json') for name in names};independent={}
    for name,result in profiles.items():
        assert result['status'].startswith('PASS'),name
        assert all(c['exit']==0 for c in result['commands']),name
        check_hashes(result.get('source_sha256',{}))
        if name in ('host','arm64','windows'):
            assert 'PNG_roundtrip=36 RGB_channels=3024 rejected=8 slot_sequences=4' in result['stdout']
            check_hashes(result['output_sha256']);fixture=ROOT/result['fixture']
            assert sha(fixture/'pixels.bin')==result['observations_sha256']
            independent[name]=verify_fixture(fixture)
            binary=BASE/name/('screenshot.exe' if name=='windows' else 'screenshot');assert sha(binary)==result['binary_sha256']
            for path,digest in result['archive_sha256'].items():assert sha(Path(path.replace('/mnt/d/','D:/')))==digest
        elif name.startswith('readback-'):
            assert 'retained_primary=6 late_deliveries=6' in result['baseline_stdout']
            assert 'retained_primary=0 late_deliveries=0' in result['candidate_stdout']
    assert len({profiles[name]['observations_sha256'] for name in ('host','arm64','windows')})==1
    sdk=BASE/'sdk';metadata=read(sdk/'native-diagnostic-build.json');previous=read(ROOT/'build/matrixgame/e2e566-readback-abort/sdk/native-diagnostic-build.json')
    check_hashes(metadata['own_source_sha256']);check_hashes(metadata['generated_original_sha256'],sdk/'source')
    old=previous['generated_original_sha256'];new=metadata['generated_original_sha256'];assert old.keys()==new.keys()
    changes=[name for name in old if old[name]!=new[name]];assert changes==['MatrixGame/src/OriginalNativeForm.cpp'],changes
    wrapper=(sdk/'source/MatrixGame/src/OriginalNativeForm.cpp').read_text()
    assert 'const auto filename=native_form::Screenshot();g_MatrixMap->m_DI.T(' in wrapper
    source=(ROOT/'port/matrixgame/native-engine/platform_switch.cpp').read_text()
    start=source[source.index('    void StartRecording(){'):source.index('    void AcquirePrimary(){')]
    assert start.index('screenshot_frame.Invalidate()')<start.index('frame.Acquire()')
    detach=source[source.index('    void Detach('):source.index('    void Commit(')]
    assert detach.index('screenshot_frame.Invalidate()')<detach.index('old.swapchain=nullptr')
    present_start=source.index('void DekoDevice::PresentNativeFrame()')
    present=source[present_start:source.index('DekoDevice::DiagnosticCounters',present_start)]
    assert present.index('v.queue.presentImage')<present.index('native presentation queue error')<present.index('v.screenshot_frame.Presented')
    abort=source[source.index('DeviceResult DekoDevice::AbortNativeFrame()'):source.index('DeviceResult DekoDevice::NativeClear(')]
    assert abort.index('v.screenshot_frame.Invalidate()')<abort.index('if(!v.queue')
    capture=source[source.index('native_form::ScreenshotPixels DekoDevice::CaptureScreenshot()'):source.index('DeviceResult DekoDevice::AbortNativeFrame()')]
    assert capture.index('screenshot_frame.Require(v.Busy() || v.recording,v.Healthy())')<capture.index('ReadImage(')
    assert 'v.Targets().images->scene[slot]' in capture
    symbols=(sdk/'symbols.log').read_text()
    for symbol in ('DekoDevice::CaptureScreenshot(', 'native_form::Screenshot[abi:cxx11](', 'native_form::SaveScreenshot[abi:cxx11](', 'CBitmap::SaveInPNG('):assert symbol in symbols,symbol
    check_hashes(profiles['sdk']['link_inputs_sha256']);assert not profiles['sdk']['strong_undefined']
    for name,value in profiles['sdk']['artifacts'].items():assert sha(ROOT/name)==value['sha256']
    for name,digest in profiles['sdk']['protected_after'].items():assert sha(Path(name))==digest
    previous_nro=ROOT/'build/matrixgame/e2e566-readback-abort/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro'
    assert sha(previous_nro)=='9e25cb5673e106e4d7fd78849d77d7cb11f1027348f9ffbf9541d46a262d1f0b'
    package=sdk/'diagnostic-nro'
    validation=verify_nro((package/'matrixgame-native-diagnostic.nro').read_bytes(),Path('C:/devkitPro/libnx/default_icon.jpg').read_bytes(),(package/'control.nacp').read_bytes())
    protected_report=read(ROOT/'reports/matrixgame-e2e521-font-evidence.json')
    protected={name:value['sha256'] for name,value in protected_report['protected'].items() if value.get('status')!='UNAVAILABLE'}
    protected.update({name:value['sha256'] for name,value in protected_report['extra_frozen_artifacts_verified'].items()});check_hashes(protected)
    primary=protected_report['primary_NRO_verified_against_previous_fingerprint'];assert sha(Path(primary['path']))==primary['sha256']
    assert sha(ROOT/'Switch-E2E5-TEST.nro')==protected_report['root_cumulative_NRO_verified_against_documented_hash']
    git='C:/Program Files/Git/cmd/git.exe';upstream=ROOT/'upstream/matrixgame'
    pin=subprocess.check_output([git,'-C',upstream,'rev-parse','HEAD'],text=True).strip();assert pin=='218115a72fa9c5d43269f4ca84a8596a0bcb82f1'
    assert not subprocess.check_output([git,'-C',upstream,'status','--porcelain'],text=True)
    result={'status':'PASS; NATIVE F9 SOFTWARE INTEGRATION AND PNG; GPU CAPTURE HARDWARE PENDING','profiles':profiles,'independent_PNG_oracle':independent,
        'generated_changes_since_E566':changes,'NRO_validation':validation,'previous_E566_NRO_preserved':sha(previous_nro),'protected_verified':len(protected)+2,
        'upstream_pin':pin,'upstream_clean':True,'scope':'Synthetic capture provider; actual CBitmap PNG/CFile and private output; SDK-linked Deko scene readback; slot hooks source-audited, not executed on GPU.',
        'remaining':['Switch F9 capture and visual verification','full native Init/map after ReadParams/ApplyGammaRamp','real Rangers resource/audio services','hardware VRAM/HID/focus/presentation/recovery'],
        'audit_source_sha256':{str(Path(__file__).resolve().relative_to(ROOT)):sha(Path(__file__).resolve()),'tools/matrixgame_screenshot_png_verify.py':sha(ROOT/'tools/matrixgame_screenshot_png_verify.py')},
        'Switch':'NOT RUN','commit':False,'push':False}
    output.parent.mkdir(parents=True,exist_ok=True)
    with output.open('x',encoding='utf-8') as stream:json.dump(result,stream,indent=2);stream.write('\n')
    print(result['status']);print(json.dumps(profiles['sdk']['artifacts'],indent=2))
if __name__=='__main__':main()
