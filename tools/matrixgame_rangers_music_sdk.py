"""Compile music owner in SDK archive; retain explicit existing-probe scope."""
import json
import matrixgame_rangers_sound_sdk as sdk
def main():
    sdk.BASE=sdk.ROOT/'build/matrixgame/e2e571-rangers-music/sdk-final'
    paths=[sdk.ROOT/'port/matrixgame/native'/name for name in ('rangers_music_session.cpp','rangers_music_session.hpp')]
    before={str(path.relative_to(sdk.ROOT)):sdk.sha(path) for path in paths}
    sdk.main()
    assert before=={str(path.relative_to(sdk.ROOT)):sdk.sha(path) for path in paths}
    assert 'RangersMusicSession::RangersMusicSession(' in (sdk.BASE/'archive-symbols.log').read_text()
    assert 'RangersMusicSession::RangersMusicSession(' not in (sdk.BASE/'symbols.log').read_text()
    (sdk.BASE/'music-source.json').write_text(json.dumps({'source_sha256':before,'status':'PASS; MUSIC OWNER ARCHIVE COMPILE ONLY'},indent=2)+'\n')
if __name__=='__main__':main()
