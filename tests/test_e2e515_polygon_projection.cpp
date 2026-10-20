#include "runtime_support.hpp"
#include "units/ab_Global.hpp"
#include "e2e515_polygon_projection.hpp"
#include <bit>
#include <cfenv>
#include <cstdio>
#include <stdexcept>
#include <limits>
#if defined(E2E515_PROJECTION_INLINE)
#include "inline-projector.hpp"
#endif
#if defined(E2E515_PROJECTION_PREPARED)
#include "prepared-projector.hpp"
#endif

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
    unsigned inline_checks{};
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
#if defined(E2E515_PROJECTION_INLINE) || defined(E2E515_PROJECTION_PREPARED)
            // Exercise perspective denominators and each original summation,
            // with varied finite signs/exponents instead of identity alone.
            for (unsigned row = 0; row < 4; ++row) for (unsigned col = 0; col < 4; ++col) {
                auto bits = (std::uint64_t(i + 1) * UINT64_C(0x9e3779b97f4a7c15)) ^
                            (std::uint64_t(row * 4 + col + 1) * UINT64_C(0xbf58476d1ce4e5b9));
                bits = (bits & UINT64_C(0x800fffffffffffff)) | ((UINT64_C(0x3e0) + (bits % 64)) << 52);
                matrix[row][col] = std::bit_cast<double>(bits);
            }
            if (i % 29 == 0) point.X = std::numeric_limits<double>::denorm_min();
            if (i % 31 == 0) point.Z = std::numeric_limits<double>::max();
            if (i % 37 == 0) point.Y = std::numeric_limits<double>::lowest();
            std::feclearexcept(FE_ALL_EXCEPT);
#endif
            const auto expected = original(matrix, point);
#if defined(E2E515_PROJECTION_INLINE) || defined(E2E515_PROJECTION_PREPARED)
            const auto expected_flags = std::fetestexcept(FE_ALL_EXCEPT);
            std::feclearexcept(FE_ALL_EXCEPT);
#if defined(E2E515_PROJECTION_PREPARED)
            const e2e515::PreparedPolygonMatrix prepared(matrix);
            if (std::fetestexcept(FE_ALL_EXCEPT)) return 9;
            const auto projected = e2e_project_prepared(prepared, pas::ConstRef<EC_Struct::TVector3D>(&point));
#else
            const auto projected = e2e_project_inline(matrix, pas::ConstRef<EC_Struct::TVector3D>(&point));
#endif
            if (!Same(projected, expected) ||
                std::fetestexcept(FE_ALL_EXCEPT) != expected_flags) {
                std::fprintf(stderr, "inline mismatch case=%u mode=%d expected_flags=%d actual_flags=%d\n",
                    i, mode, expected_flags, std::fetestexcept(FE_ALL_EXCEPT));
                return 8;
            }
            ++inline_checks;
#endif
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
#if defined(E2E515_PROJECTION_PREPARED)
    for (auto mode : {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO}) {
        std::fesetround(mode);
        for (auto bits : {UINT64_C(1), UINT64_C(0x7ff0000000000000),
                          UINT64_C(0x7ff8000000000123), UINT64_C(0x7ff0000000000001)}) {
            matrix[0][0] = std::bit_cast<double>(bits);
            std::feclearexcept(FE_ALL_EXCEPT);
            const auto expected = original(matrix, point);
            const auto flags = std::fetestexcept(FE_ALL_EXCEPT);
            std::feclearexcept(FE_ALL_EXCEPT);
            const e2e515::PreparedPolygonMatrix prepared(matrix);
            if (prepared.finite || std::fetestexcept(FE_ALL_EXCEPT)) return 10;
            if (!Same(e2e_project_prepared(prepared, pas::ConstRef<EC_Struct::TVector3D>(&point)), expected) ||
                flags != std::fetestexcept(FE_ALL_EXCEPT)) return 11;
            ++inline_checks;
        }
    }
    matrix[0][0] = 1.;
#endif
    try {
        memo.Project(matrix, point, 0xFFFFFFFF, [](const auto&, const auto&) -> EC_Struct::TVector3D {
            throw std::runtime_error("injected projector failure");
        }); return 6;
    } catch (const std::runtime_error&) {}
    if (!Same(memo.Project(matrix, point, 0xFFFFFFFF, original), original(matrix, point))) return 7;
    std::fesetround(initial);
    std::printf("PASS original projector memo; byte_checks=%u rounding_modes=4 hits=%llu misses=%llu extended_bytes=%zu\n",
        checks, static_cast<unsigned long long>(memo.Hits()), static_cast<unsigned long long>(memo.Misses()), sizeof(pas::Extended));
    std::printf("inline_projection_byte_checks=%u\n", inline_checks);
}
