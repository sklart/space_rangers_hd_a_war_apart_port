"""Strict HAL/CPU/SDK fog proof, baseline diagnostics and protected-input audit."""
import json,re,hashlib,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/matrixgame/e2e530-fog'
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 results={}
 for kind in ('windows','host','arm64','sdk'):
  j=read(BASE/kind/'result.json');assert j['status']=='PASS' and all(c['exit']==0 for c in j['commands']),kind
  for n,h in j['source_sha256'].items():assert sha(ROOT/n)==h,(kind,n)
  results[kind]=j
 windows=results['windows'];assert 'cases=1344 gradients=168 sampled_pixels=193536 mismatches=0 max_delta=0' in windows['stdout']
 assert (BASE/'windows/fog.golden').stat().st_size==840*40
 golden=sha(BASE/'windows/fog.golden');assert windows['golden_sha256']==golden
 for kind in ('host','arm64'):
  j=results[kind];assert 'HAL_goldens=840 max_delta=1 original_scalar_restore=exact lease_restore=exact' in j['stdout']
  assert 'CPU_ONLY NO_NATIVE_INIT NO_GPU SWITCH_HARDWARE_NOT_RUN' in j['stdout']
  assert j['golden_sha256']==golden and sha(BASE/kind/'pixel-fog')==j['binary_sha256']
  assert '-UNDEBUG' in j['commands'][0]['command']
 assert '-fsanitize=address,undefined' in results['host']['commands'][0]['command']
 for label,path,key in (('material',BASE/'materials/windows/result.json','input_sha256'),('alpha',BASE/'windows-alpha/result.json','source_sha256')):
  j=read(path);assert j['status']=='PASS' and all(c['exit']==0 for c in j['commands']),label
  for n,h in j[key].items():assert sha(Path(n) if Path(n).is_absolute() else ROOT/n)==h,n
  results[label]=j
 assert 'D3D9_differential=1476 mismatches=0' in results['material']['stdout'] and 'GLSL_differential=1476 GLSL_mismatches=0' in results['material']['stdout']
 assert 'fixed vertex cases=84 GLSL_feedback=84 D3D9_ProcessVertices=80' in results['material']['stdout']
 assert '9184 cases PASS maximum delta=0' in results['alpha']['stdout']
 baseline=read(BASE/'baseline-windows/result.json');assert baseline['status']=='INCOMPLETE' and baseline['commands'][-1]['exit']==1
 baseline_log=(BASE/'baseline-windows/runtime.log').read_text();assert 'cases=384 gradients=48 sampled_pixels=55296 mismatches=22 max_delta=60' in baseline_log
 assert 'LegacyRasterViewport' not in (BASE/'baseline-windows/source/tests/matrixgame_material_gl_reference.hpp').read_text()
 sdk=results['sdk'];assert not sdk['strong_undefined'];assert sha(BASE/'sdk/matrixgame-native-entry.elf')==sdk['elf_sha256']
 for n,h in sdk['link_inputs_sha256'].items():assert sha(ROOT/n)==h,n
 caps=Path('C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/shared/d3d9caps.h');text=caps.read_text();bits={k:int(v,16) for k,v in re.findall(r'#define D3DPRASTERCAPS_(FOGTABLE|WFOG|ZFOG)\s+0x([0-9A-Fa-f]+)',text)}
 assert bits=={'FOGTABLE':0x100,'WFOG':0x100000,'ZFOG':0x200000}
 generated=read(BASE/'sdk/source/native-engine-source.json')['files_sha256'];assert len(generated)==252
 assert generated==read(ROOT/'build/matrixgame/e2e529-stored-state/sdk/source/native-engine-source.json')['files_sha256']
 for n,h in generated.items():assert sha(BASE/'sdk/source'/n)==h,n
 previous=read(ROOT/'reports/matrixgame-e2e521-font-evidence.json');checked=0
 for n,v in previous['protected'].items():
  if v.get('status')!='UNAVAILABLE':assert sha(Path(n))==v['sha256'],n;checked+=1
 for n,v in previous['extra_frozen_artifacts_verified'].items():assert sha(Path(n))==v['sha256'],n
 primary=previous['primary_NRO_verified_against_previous_fingerprint'];assert sha(Path(primary['path']))==primary['sha256']
 assert sha(ROOT/'Switch-E2E5-TEST.nro')==previous['root_cumulative_NRO_verified_against_documented_hash']
 hardware=read(ROOT/'build/hardware-20261023/input-manifest.json')
 for n,v in hardware.items():assert sha(ROOT/'logs/2026.10.23'/n)==v['sha256'],n
 assert sha(ROOT/'windows/Space Rangers HD A War Apart/DATA/robots.pkg')=='8a22f856a27a8937ef28ac61274ffdb85cc2ff865492bd4d3ddeb2a6054011f0'
 for n,h in read(ROOT/'build/matrixgame/e2e519/original-init-host/result.json')['source_cfg_sha256'].items():assert sha(ROOT/'upstream/matrixgame/MatrixGame/CFG'/n)==h,n
 frozen=read(ROOT/'reports/e2e5/hardware-20261023-cache-frozen-audit.json');assert sha(ROOT/frozen['cache_source'])==frozen['expected_sha256']
 git=r'C:\Program Files\Git\cmd\git.exe'
 def output(*args,cwd=ROOT):return subprocess.check_output([git,*args],cwd=cwd,text=True).strip()
 parent=output('rev-parse','HEAD');assert parent==previous['parent'];up=ROOT/'upstream/matrixgame'
 assert output('rev-parse','HEAD',cwd=up)=='218115a72fa9c5d43269f4ca84a8596a0bcb82f1' and not output('status','--porcelain','--untracked-files=all',cwd=up)
 names=sorted(set(n for j in results.values() for n in j.get('source_sha256',{})))+['tools/matrixgame_fog_audit.py','reports/matrixgame-e2e530-fog.md','docs/matrixgame-readiness-plan.md']
 subprocess.run([git,'diff','--check','--',*names],cwd=ROOT,check=True)
 for n in names:
  if not n.startswith('build'):
   assert not any(l.rstrip()!=l or l.startswith(('<<<<<<<','=======','>>>>>>>')) for l in (ROOT/n).read_text(encoding='utf-8').splitlines()),n
 assert not list(BASE.rglob('*.nro'))
 result={'status':'E2E-5.30 PIXEL FOG/RASTER SOFTWARE PASS / FULL NATIVE INIT PARTIAL','results':results,'parent':parent,
  'source_sha256':{n:sha(ROOT/n) for n in names},'official_caps_sha256':sha(caps),'original_generated_files_unchanged':252,
  'protected_verified':checked,'hardware_inputs_verified':len(hardware),'HAL_golden_sha256':golden,
  'baseline_gradient_mismatches':22,'baseline_max_RGB_delta':60,'final_HAL_GLSL_max_delta':0,'CPU_goldens_max_delta':1,
  'baseline_artifacts_sha256':{str(p.relative_to(BASE)):sha(p) for p in (BASE/'baseline-windows').rglob('*') if p.is_file()},
  'full_native_Init_executed':False,'native_map_loaded':False,'native_frame_presented':False,'Switch_hardware':'NOT RUN / HARDWARE_PENDING',
  'next_conditional_guard':'CMatrixMap::DrawShadows [stencil shadows] -> SHADEMODE=FLAT -> native flat shading not implemented; then StencilCaps/pipeline',
  'first_native_Init_runtime_guard':'NOT OBSERVED','NRO_created':False,'commit':False,'push':False,
  'P0':'OPEN','GI_repeating_text_causality':'NOT PROVEN','manual_publish_ENOENT':'OPEN','Switch_hangs':'OPEN'}
 (ROOT/'reports/matrixgame-e2e530-fog-evidence.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 print(json.dumps({k:result[k] for k in ('status','baseline_gradient_mismatches','final_HAL_GLSL_max_delta','protected_verified','hardware_inputs_verified','next_conditional_guard')}))
if __name__=='__main__':main()
