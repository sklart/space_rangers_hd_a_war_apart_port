#pragma once
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>

namespace srhd_awa::platform {
// Value-only memo for the UI-thread polygon projector. No scene pointers or
// resource ownership survive a call. Matrix, point and FP control are exact keys.
template<class Matrix, class Vector, std::size_t Capacity = 4096> class PolygonProjectionMemo {
    static_assert(Capacity && !(Capacity & (Capacity - 1)));
    static_assert(sizeof(Matrix) == 16 * sizeof(double));
    struct Entry {
        std::array<std::uint64_t, 3> key{};
        Vector value{};
        std::uint64_t generation{};
    };
    std::array<Entry, Capacity> entries_{};
    std::array<std::uint64_t, 16> matrix_{};
    std::uint64_t control_{}, generation_{}, hits_{}, misses_{};
    bool matrix_finite_{};
    static bool Finite(std::uint64_t bits) {
        return (bits & UINT64_C(0x7ff0000000000000)) != UINT64_C(0x7ff0000000000000);
    }
public:
    std::uint64_t Hits() const { return hits_; }
    std::uint64_t Misses() const { return misses_; }
    template<class Projector> Vector Project(const Matrix& matrix, const Vector& point,
                                             std::uint64_t control, Projector&& projector) {
        std::array<std::uint64_t, 16> key_matrix;
        std::memcpy(key_matrix.data(), &matrix, sizeof(matrix));
        if (!generation_ || key_matrix != matrix_ || control != control_) {
            matrix_ = key_matrix; control_ = control;
            if (++generation_ == 0) { entries_ = {}; generation_ = 1; }
            matrix_finite_ = true;
            for (auto bits : matrix_) matrix_finite_ &= Finite(bits);
        }
        const std::array key{std::bit_cast<std::uint64_t>(point.X),
                             std::bit_cast<std::uint64_t>(point.Y),
                             std::bit_cast<std::uint64_t>(point.Z)};
        if (!matrix_finite_ || !Finite(key[0]) || !Finite(key[1]) || !Finite(key[2])) {
            ++misses_; return projector(matrix, point);
        }
        auto hash = key[0] ^ (key[1] * UINT64_C(0x9e3779b97f4a7c15)) ^ key[2];
        hash ^= hash >> 33; hash *= UINT64_C(0xff51afd7ed558ccd); hash ^= hash >> 33;
        auto& entry = entries_[hash & (Capacity - 1)];
        if (entry.generation == generation_ && entry.key == key) { ++hits_; return entry.value; }
        ++misses_;
        const Vector result = projector(matrix, point);
        if (Finite(std::bit_cast<std::uint64_t>(result.X)) && Finite(std::bit_cast<std::uint64_t>(result.Y)) &&
            Finite(std::bit_cast<std::uint64_t>(result.Z))) entry = {key, result, generation_};
        return result;
    }
};
}
