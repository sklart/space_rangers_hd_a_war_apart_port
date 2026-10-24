"""Keep opaque callback payloads pointer-sized through complete effect chains."""
import hashlib
import re


def prepare_callback_payloads(output):
    changed={}
    for stem in ['MovingObject','FirePlasma','Flame','BigBoom']:
        for extension,expected in [('hpp',2),('cpp',1)]:
            name=f'MatrixGame/src/Effects/MatrixEffect{stem}.{extension}'
            path=output/name;text=path.read_text(encoding='utf-8')
            # Only opaque user payloads, never color/light/hitmask/user_param.
            text,count=re.subn(r'\bstd::uint32_t (m_UserData|m_User|uservalue|user)\b',
                               r'uintptr_t \1',text)
            if count!=expected:
                raise ValueError(f'Original {stem} callback payload boundary changed: {count} in {name}')
            path.write_bytes(text.encode());changed[name]=hashlib.sha256(path.read_bytes()).hexdigest()
    return changed
