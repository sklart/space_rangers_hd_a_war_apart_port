"""Rejected polygon projection experiment, kept for diagnostic reproduction only."""
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
