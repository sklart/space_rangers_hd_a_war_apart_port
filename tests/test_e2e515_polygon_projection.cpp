#include "runtime_support.hpp"
#include "units/ab_Global.hpp"
#include "e2e515_polygon_projection.hpp"
#include <bit>
#include <cfenv>
#include <cstdio>
#include <stdexcept>

using Memo = srhd_awa::platform::PolygonProjectionMemo<ab_Global::TMatrix4D, EC_Struct::TVector3D, 16>;
bool Same(EC_Struct::TVector3D a, EC_Struct::TVector3D b) {
    return std::bit_cast<std::uint64_t>(a.X) == std::bit_cast<std::uint64_t>(b.X) &&
           std::bit_cast<std::uint64_t>(a.Y) == std::bit_cast<std::uint64_t>(b.Y) &&
           std::bit_cast<std::uint64_t>(a.Z) == std::bit_cast<std::uint64_t>(b.Z);
}
int main() {
    Memo memo;
    const auto initial = std::fegetround();
    unsigned checks{};
    auto original = [](const ab_Global::TMatrix4D& matrix, const EC_Struct::TVector3D& point) {
        return ab_Global::ProjectPointByMatrix(matrix, pas::ConstRef<EC_Struct::TVector3D>(&point));
    };
    for (auto mode : {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO}) {
        if (std::fesetround(mode)) return 2;
        ab_Global::TMatrix4D matrix{};
        for (int i = 0; i < 4; ++i) matrix[i][i] = 1.;
        for (unsigned i = 0; i < 8192; ++i) {
            EC_Struct::TVector3D point{double(i % 113) / 13., double(i % 71) / -9., double(i % 43) / 7.};
            if (i % 17 == 0) point.X = -0.;
            if (i % 32 == 0) matrix[3][0] = double(i % 83) / 17.;
            const auto expected = original(matrix, point);
            for (int repeat = 0; repeat < 3; ++repeat) {
                const auto actual = memo.Project(matrix, point, mode, original);
                if (!Same(actual, expected) || std::fegetround() != mode) return 3;
                ++checks;
            }
            // Reusing a point address with changed coordinates must never hit stale output.
            point.Y += .125;
            if (!Same(memo.Project(matrix, point, mode, original), original(matrix, point))) return 4;
            // FP-control changes are independent invalidation even with the same matrix.
            if (!Same(memo.Project(matrix, point, std::uint64_t(mode) + 0x10000, original), original(matrix, point))) return 5;
        }
    }
    ab_Global::TMatrix4D matrix{}; for (int i = 0; i < 4; ++i) matrix[i][i] = 1.;
    EC_Struct::TVector3D point{1.,2.,3.};
    try {
        memo.Project(matrix, point, 0xFFFFFFFF, [](const auto&, const auto&) -> EC_Struct::TVector3D {
            throw std::runtime_error("injected projector failure");
        }); return 6;
    } catch (const std::runtime_error&) {}
    if (!Same(memo.Project(matrix, point, 0xFFFFFFFF, original), original(matrix, point))) return 7;
    std::fesetround(initial);
    std::printf("PASS original projector memo; byte_checks=%u rounding_modes=4 hits=%llu misses=%llu extended_bytes=%zu\n",
        checks, static_cast<unsigned long long>(memo.Hits()), static_cast<unsigned long long>(memo.Misses()), sizeof(pas::Extended));
}
