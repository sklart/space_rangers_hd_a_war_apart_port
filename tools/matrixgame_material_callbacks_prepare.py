"""Extract pinned original stage callbacks for differential tests, not the game.

Only API/type spelling is adapted. Callback branches, Prepare selection and
pass continuation remain original. Resource objects below are test inputs.
"""
from pathlib import Path
import hashlib
import re
from matrixgame_render_pipeline_overrides import adapt
from matrixgame_vector_object_overrides import function

ROOT=Path(__file__).resolve().parents[1]

def prepare(out):
    source=ROOT/'upstream/matrixgame/MatrixGame/src/MatrixSkinManager.cpp'
    data=source.read_bytes().replace(b'\r\n',b'\n')
    assert hashlib.sha256(data).hexdigest()=='d9d565af29522397011903bf6e3d730575092d0eb8ad540506740ff8f03f4778'
    text=data.decode()
    block=text[text.index('static void obj_shadow('):text.index('void CSkinManager::Clear(')]
    block=adapt(block).replace('D3DXMATRIX','Matrix').replace('LPDIRECT3DTEXTURE9','void*')
    block=block.replace('srhd_awa::matrixgame::render_pipeline::MaxSimultaneousTextures()','fixture_caps')
    block=block.replace('srhd_awa::matrixgame::render_pipeline::TextureTransform(','TextureTransform(')
    block=re.sub(r'\bg_D3DDCaps.MaxSimultaneousTextures\b','fixture_caps',block)
    for i in range(8):
        block=block.replace(f'g_D3DD->SetTransform(D3DTS_TEXTURE{i},',f'TextureTransform({i},')
    header=(ROOT/'upstream/matrixgame/MatrixGame/src/MatrixSkinManager.hpp').read_text()
    skin=header[header.index('struct SMatrixSkin :'):header.index('typedef SMatrixSkin')]
    skin=skin.replace('CTextureManaged','FixtureTexture')
    helper=(ROOT/'upstream/matrixgame/MatrixLib/3G/3g.hpp').read_text()
    start=helper.index('\n#else\ninline void SetColorOp(',helper.index('#ifdef DO_SMART_COLOROPS'))
    helper=adapt(helper[start+len('\n#else\n'):helper.index('\n#endif',start)]).replace('original_renderer_ops::','')
    # Original ordinary helpers refer to ReadyDevice; no native shader result
    # is returned by the callback fixture.
    out.mkdir(parents=True,exist_ok=True)
    target=out/'original_skin_materials.inc'
    aliases='namespace original_renderer_ops {\n'+''.join('using ::'+name+';\n' for name in (
        'SetColorOp','SetAlphaOp','SetColorOpAnyOrder','SetAlphaOpAnyOrder',
        'SetColorOpSelect','SetAlphaOpSelect','SetColorOpDisable','SetAlphaOpDisable'))+'}\n'
    target.write_text('// Extracted original GPL-2.0-or-later callbacks; test-only.\n'+skin+helper+aliases+block)
    render=(ROOT/'upstream/matrixgame/MatrixGame/src/MatrixRenderPipeline.cpp').read_text()
    names=('WaterSolid_t2','WaterClearSolid_t2','WaterAlpha_t3','WaterClearAlpha_t3',
        'TerBot','TerBotM','TerBotTex','TerBotTexM','TerBotClear',
        'TerSurf','TerSurfW','TerSurfM','TerSurfMW','TerSurfGloss','TerSurfGlossW','TerSurfGlossM','TerSurfGlossMW',
        'TerSurfTex','TerSurfTexM','TerSurfTexGloss','TerSurfTexGlossM','TerSurfClear','TerSurfClearGloss','TerSurfClearGlossM')
    callbacks=[]
    for name in names:
        signature=re.search(r'^(?:static )?void '+name+r'\(',render,re.M)
        assert signature,name
        callbacks.append(function(render,signature.group()))
    terrain=adapt('\n'.join(callbacks)).replace('srhd_awa::matrixgame::render_pipeline::TextureTransform(','TextureTransform(')
    (out/'original_terrain_materials.inc').write_text('// Extracted original GPL-2.0-or-later callbacks; test-only.\n'+terrain)
    return {'original_skin_sha256':hashlib.sha256(data).hexdigest(),
            'test_extraction_sha256':hashlib.sha256(target.read_bytes()).hexdigest(),
            'original_renderer_normalized_sha256':hashlib.sha256(render.encode()).hexdigest(),
            'terrain_test_extraction_sha256':hashlib.sha256((out/'original_terrain_materials.inc').read_bytes()).hexdigest(),
            'terrain_water_callbacks':names}

if __name__=='__main__':
    import sys,json
    print(json.dumps(prepare(Path(sys.argv[1])),indent=2))
