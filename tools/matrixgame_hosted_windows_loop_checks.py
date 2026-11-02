"""Current production MatrixGame + real Rangers host + original interactive loop."""
import matrixgame_hosted_windows_map_checks as check
from matrixgame_standard_nro_audit import ROOT,read
if __name__=='__main__':
 check.BASE=ROOT/'build/matrixgame/e2e581-hosted-windows-loop'
 check.NATIVE=ROOT/'build/matrixgame/e2e580-loading-capture/native-build'
 check.PROBE=ROOT/'tests/probe_matrixgame_hosted_windows_loop.cpp'
 check.EXTRA_SOURCES=[ROOT/'port/matrixgame/native-engine'/name for name in ('native_form_session.cpp','native_form_backend.cpp','native_loop.cpp','native_input_engine.cpp','applet_engine_events.cpp')]
 check.EXTRA_SOURCES+=[check.NATIVE/'native/source/MatrixLib/3G/OriginalNativeLoop.cpp',check.NATIVE/'native/source/MatrixGame/src/OriginalNativeForm.cpp']
 check.main()
 records=sorted(check.BASE.glob('result*.json'),key=lambda p:p.stat().st_mtime_ns,reverse=True)
 result=read(records[0]);assert result['status'].startswith('PASS')
 assert result['stdout'].count('HOSTED_ORIGINAL_LOOP draws=208 elapsed=5125 input_pause_menu_camera=PASS focus_release_recreate=PASS host_callbacks_live=PASS')==2
