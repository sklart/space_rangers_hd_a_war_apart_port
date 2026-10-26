"""Source/official SDK inventory, NOT runtime stored-state support."""
import json,re,hashlib,argparse
from pathlib import Path
from matrixgame_vector_object_overrides import function
ROOT=Path(__file__).resolve().parents[1]
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--sdk-types',type=Path,default=Path('C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/shared/d3d9types.h'));args=ap.parse_args()
 paths=['upstream/matrixgame/MatrixLib/3G/DeviceState.cpp','port/matrixgame/native-engine/native_device_state.cpp','port/matrixgame/native-engine/platform_switch.cpp','port/matrixgame/native-engine/platform_switch.hpp','port/matrixgame/deko/fixed_vertex.cpp','port/matrixgame/deko/raster_state.hpp','port/matrixgame/deko/sampler_state.hpp']
 src={n:(ROOT/n).read_text() for n in paths};original=src[paths[0]];sdk=args.sdk_types.read_text()
 render=re.findall(r'\bD3DRS_\w+',original);sampler=re.findall(r'\bD3DSAMP_\w+',original);assert len(render)==103 and len(set(render))==103 and len(sampler)==13
 ids={key:int(value) for key,value in re.findall(r'\b(D3DRS_\w+|D3DSAMP_\w+)\s*=\s*(\d+)\s*,',sdk)};assert all(k in ids for k in render+sampler)
 raster=src[paths[5]];enum=raster[raster.index('enum class RasterRegister'):raster.index('};')];raster_ids={name:int(value) for name,value in re.findall(r'(\w+)\s*=\s*(\d+)',enum)}
 regs=raster[raster.index('inline constexpr std::array<RasterRegister,11>'):];initial={raster_ids[n] for n in re.findall(r'RasterRegister::(\w+)',regs)};assert len(initial)==11
 vertex=function(src[paths[4]],'bool FixedVertexState::Render(');vertex_ids={int(n) for n in re.findall(r'r==(\d+)',vertex)}
 setter=function(src[paths[2]],'DeviceResult DekoDevice::SetRenderState(');special={int(n) for n in re.findall(r'unsigned\(r\)==(\d+)',setter)};supported=initial|vertex_ids|special
 enum_sampler=src[paths[6]].split('enum class SamplerRegister',1)[1].split('};',1)[0];sampler_ids={int(n) for n in re.findall(r'=\s*(\d+)',enum_sampler)};assert len(sampler_ids)==8
 r=[{'original_ordinal':i,'sdk_register':key,'numeric_id':ids[key],'native_setter_route':ids[key] in supported,'native_initial_raw_register':ids[key] in initial} for i,key in enumerate(render)]
 s=[{'original_ordinal':i,'sdk_register':key,'numeric_id':ids[key],'native_setter_getter_route':ids[key] in sampler_ids} for i,key in enumerate(sampler)]
 declaration=next(l for l in src[paths[3]].splitlines() if l.startswith('class DekoDevice final:'));assert 'StoredStateBackend' not in declaration
 result={'scope':'SOURCE ANALYSIS ONLY; no StoredStateBackend added, no GPU/runtime PASS','render_count':103,'sampler_count_per_slot':13,'sampler_slots':8,'light_slots':8,'material':True,'render':r,'sampler':s,
 'native_render_setter_routes':sum(x['native_setter_route'] for x in r),'missing_native_render_setter_routes':[x['sdk_register'] for x in r if not x['native_setter_route']],
 'missing_native_sampler_routes':[x['sdk_register'] for x in s if not x['native_setter_getter_route']],
 'native_light_material_getter_adapter':'NOT IMPLEMENTED; preserve original raw values, enable state and absent Type=0 semantics',
 'guard':'CMatrixMap::ReleasePoolDefaultResources -> map_device::StateBackend -> map stored device state backend unavailable',
 'ownership':'CMatrixMap owns original CDeviceState allocated by HNew(g_MatrixHeap); restore/delete in CreatePoolDefaultResources or delete on map destruction',
 'native_capture_lease_equivalent_to_original_CDeviceState':False,'ordinal_numeric_id_cast_valid':False,
 'initial_fillmode_gap':r[1], 'source_sha256':{n:hashlib.sha256((ROOT/n).read_bytes()).hexdigest() for n in paths},'sdk_types_path':str(args.sdk_types),'sdk_types_sha256':hashlib.sha256(args.sdk_types.read_bytes()).hexdigest()}
 out=ROOT/'reports/matrixgame-e2e528-stored-state-inventory.json';out.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({'render':103,'native_setter_routes':result['native_render_setter_routes'],'missing_sampler':result['missing_native_sampler_routes'],'scope':result['scope']}))
if __name__=='__main__':main()
