"""Verify byte-identical source generation and stable timestamps on reconfigure."""
import hashlib,json,time
from pathlib import Path
from matrixgame_engine_prepare import prepare_engine
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/matrixgame/e2e522'
def main():
    output=BASE/'repro-source';before=json.loads((output/'native-engine-source.json').read_text());mtimes={n:(output/n).stat().st_mtime_ns for n in before['files_sha256']};start=time.monotonic()
    same=prepare_engine(output);assert same['files_sha256']==before['files_sha256'];assert all((output/n).stat().st_mtime_ns==mtime for n,mtime in mtimes.items())
    independent=prepare_engine(BASE/'repro-independent');assert independent['files_sha256']==same['files_sha256']
    for n,h in same['files_sha256'].items():assert hashlib.sha256((output/n).read_bytes()).hexdigest()==h
    result={'status':'PASS','files':len(mtimes),'same_source_mtimes_preserved':True,'independent_source_hashes_equal':True,'seconds':time.monotonic()-start,'files_sha256':same['files_sha256'],'source_sha256':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (ROOT/'tools/matrixgame_engine_prepare.py',ROOT/'tools/matrixgame_map_prepare_overrides.py',Path(__file__).resolve())}}
    (BASE/'repro-result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({k:v for k,v in result.items() if k not in ('files_sha256','source_sha256')}))
if __name__=='__main__':main()
