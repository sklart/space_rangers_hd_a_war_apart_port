"""Diagnostic projection experiments; no shipping override is enabled here."""

def inline_projector(projector_text):
    signature = '    EC_Struct::TVector3D ProjectPointByMatrix('
    if projector_text.count(signature) != 1:
        raise RuntimeError('Original projector boundary changed')
    start = projector_text.index(signature)
    end = projector_text.index('\n    }', start) + len('\n    }')
    function = projector_text[start:end].replace('ProjectPointByMatrix(', 'e2e_project_inline(', 1)
    function = function.replace('const TMatrix4D&', 'const ab_Global::TMatrix4D&', 1)
    return 'namespace {\n    __attribute__((always_inline)) inline ' + function.strip() + '\n}\n'


def apply_inline(text, projector_text):
    prefix = 'ab_Global::ProjectPointByMatrix('
    if text.count(prefix) != 5:
        raise RuntimeError('Polygon projection callsites changed')
    anchor = 'namespace ab_Polygon {'
    if text.count(anchor) != 1:
        raise RuntimeError('Polygon namespace changed')
    return text.replace(prefix, 'e2e_project_inline(').replace(
        anchor, inline_projector(projector_text) + '\n' + anchor, 1)
def apply(text):
    prefix = 'ab_Global::ProjectPointByMatrix(ab_Global::SphereProjectionMatrix, '
    if text.count(prefix) != 5:
        raise RuntimeError('Polygon projection callsites changed')
    text = text.replace(prefix, 'e2e_polygon_project(')
    helper = '''
namespace {
EC_Struct::TVector3D e2e_polygon_project(pas::ConstRef<EC_Struct::TVector3D> input) {
    static srhd_awa::platform::PolygonProjectionMemo<ab_Global::TMatrix4D, EC_Struct::TVector3D> memo;
    const auto point = pas::load_unaligned<EC_Struct::TVector3D>(input.address);
    const auto control = (std::uint64_t(System::Get8087CW()) << 32) | unsigned(std::fegetround());
    return memo.Project(ab_Global::SphereProjectionMatrix, point, control,
        [](const ab_Global::TMatrix4D& matrix, const EC_Struct::TVector3D& value) {
            return ab_Global::ProjectPointByMatrix(matrix, pas::ConstRef<EC_Struct::TVector3D>(&value));
        });
}
}
'''
    anchor = 'namespace ab_Polygon {'
    if text.count(anchor) != 1: raise RuntimeError('Polygon namespace changed')
    return '#include "e2e515_polygon_projection.hpp"\n#include <cfenv>\n' + text.replace(anchor, helper + '\n' + anchor)
