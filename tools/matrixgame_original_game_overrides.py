"""Preserve original Init/Deinit ownership; require the fresh full-game entry."""
import hashlib
from matrixgame_vector_object_overrides import function

def prepare_original_game(output):
    name='MatrixGame/src/MatrixGame.cpp';path=output/name;text=path.read_text(encoding='utf-8')
    original=function(text,'void CGame::Init(');updated=original
    brace=updated.index('{');updated=updated[:brace+1]+'\n    original_game_owner::ValidateEntry();'+updated[brace+1:]
    if updated.count('    static_init();')!=1:raise ValueError('Original CGame static init changed')
    updated=updated.replace('    static_init();','    static_init();\n    original_game_owner::ObserveStatics();',1)
    text=text.replace(original,updated,1)
    original=function(text,'void CGame::Deinit(void)');brace=original.index('{')
    updated=original[:brace+1]+'\n    original_game_owner::ValidateDeinit();'+original[brace+1:]
    text=text.replace(original,updated,1)
    prelude='#include "original_game.hpp"\nnamespace original_game_owner=srhd_awa::matrixgame::original_game;\n'
    path.write_bytes((prelude+text).encode());return {name:hashlib.sha256(path.read_bytes()).hexdigest()}
