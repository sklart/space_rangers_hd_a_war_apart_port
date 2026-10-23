"""Original map-member camera lifetime subset, not the complete Camera TU."""
import hashlib
from matrixgame_vector_object_overrides import function


def prepare_camera(output):
    source = (output / 'MatrixGame/src/MatrixCamera.cpp').read_text(encoding='utf-8')
    names = ('void SetMaxCameraDistance(float perc)',
             'void SAutoFlyData::Release(void)',
             'void SAutoFlyData::KillTrajectory(void)',
             'CMatrixCamera::CMatrixCamera(void)',
             'CMatrixCamera::~CMatrixCamera(void)')
    # Keep the full prepared Camera.cpp intact for inventory; this explicit
    # selected-method TU is linked only by the integrated bootstrap.
    declaration = 'float MAX_VIEW_DISTANCE = 4000.0f;'
    if source.count(declaration) != 1:
        raise ValueError('Original camera distance definition changed')
    text = '''// Selected original MatrixCamera methods, GPL-2.0-or-later.
#include "MatrixCamera.hpp"
#include "MatrixMapStatic.hpp"
#include "MatrixGame.h"
#include "3g.hpp"
''' + declaration + '\n' + '\n'.join(function(source, name) for name in names) + '\n'
    name = 'MatrixGame/src/MatrixCameraLifecycle.cpp'
    data = text.encode()
    (output / name).write_bytes(data)
    return {name: hashlib.sha256(data).hexdigest()}
