#pragma once
#include "runtime_support.hpp"
#include "types/ab_Global.hpp"
#include <cstdint>
#include <cstring>
#include <limits>

namespace e2e515 {
struct PreparedPolygonMatrix {
    ab_Global::TMatrix4D original{};
    pas::Array<pas::Array<pas::Extended, 0, 3>, 0, 3> coefficients{};
    bool finite{true};
    explicit PreparedPolygonMatrix(const ab_Global::TMatrix4D& matrix) {
        static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559);
        // pas::Extended is a ten-byte storage wrapper on x87 and long double
        // on ARM64; both perform arithmetic through this underlying type.
        static_assert(std::numeric_limits<long double>::digits >= 53);
        std::memcpy(&original, &matrix, sizeof(matrix));
        for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) {
            std::uint64_t bits{};
            std::memcpy(&bits, &matrix[r][c], sizeof(bits));
            const auto exponent = bits & UINT64_C(0x7ff0000000000000);
            if (exponent == UINT64_C(0x7ff0000000000000) ||
                (exponent == 0 && (bits & UINT64_C(0xfffffffffffff)))) {
                finite = false; return; // Keep exceptional/denormal loads in the original call order.
            }
        }
        for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c)
            coefficients[r][c] = static_cast<pas::Extended>(matrix[r][c]);
    }
    const auto& operator[](int row) const { return coefficients[row]; }
};
}
