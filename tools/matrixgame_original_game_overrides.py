"""Preserve original Init/Deinit ownership; require the fresh full-game entry."""
import hashlib
from matrixgame_vector_object_overrides import function

def prepare_original_game(output):
    name='MatrixGame/src/MatrixGame.cpp';path=output/name;text=path.read_text(encoding='utf-8')
    original=function(text,'void CGame::Init(');updated=original
    if updated.count('    L3GInitAsDLL(')!=1:raise ValueError('Original platform entry changed')
    updated=updated.replace('    L3GInitAsDLL(','    original_game_owner::EnterPlatform(',1)
    brace=updated.index('{');updated=updated[:brace+1]+'\n    original_game_owner::ValidateEntry();'+updated[brace+1:]
    if updated.count('    static_init();')!=1:raise ValueError('Original CGame static init changed')
    updated=updated.replace('    static_init();','    static_init();\n    original_game_owner::ObserveStatics();',1)
    for statement,stage in [('g_MatrixMap->RobotPreload();','RobotPreload complete')]:
        if updated.count(statement)!=1:raise ValueError('Original Init stage changed: '+statement)
        updated=updated.replace(statement,statement+'\n    original_game_owner::ObserveStage("'+stage+'");',1)
    statement='stor.Load(mapname.c_str());'
    if updated.count(statement)!=1:raise ValueError('Original Init storage boundary changed')
    updated=updated.replace(statement,'original_game_owner::ObserveStage(stor.Load(mapname.c_str()) ? "map storage loaded" : "map storage Load FAILED");',1)
    marker='    std::wstring mapname_lowercase(g_MatrixMap->MapName());'
    if updated.count(marker)!=1:raise ValueError('Original Init PrepareMap boundary changed')
    updated=updated.replace(marker,'    original_game_owner::ObserveStage("PrepareMap complete");\n'+marker,1)
    text=text.replace(original,updated,1)
    original=function(text,'void CGame::Deinit(void)');brace=original.index('{')
    updated=original[:brace+1]+'\n    original_game_owner::ValidateDeinit();'+original[brace+1:]
    text=text.replace(original,updated,1)
    prelude='#include "original_game.hpp"\nnamespace original_game_owner=srhd_awa::matrixgame::original_game;\n'
    path.write_bytes((prelude+text).encode());return {name:hashlib.sha256(path.read_bytes()).hexdigest()}
