// Decode an installed GI Format-0 image with the production CPU path and
// compare its pixels to an independent Python oracle fingerprint.
#include "gi_format0_cpu.hpp"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

int main(int argc, char **argv) {
    if (argc != 5) return 2;
    std::ifstream stream(argv[1], std::ios::binary);
    if (!stream) return 3;
    const std::vector<std::uint8_t> source(std::istreambuf_iterator<char>{stream}, {});
    srhd_awa::platform::gi_format0_cpu::Metadata meta;
    srhd_awa::platform::gi_format0_cpu::CpuImage image;
    std::string error;
    const auto status = srhd_awa::platform::gi_format0_cpu::Decode(
        source.data(), source.size(), &meta, &image, &error);
    if (status != srhd_awa::platform::gi_format0_cpu::Status::Ok) {
        std::cerr << "Format-0 decode failed: " << error << '\n';
        return 4;
    }
    std::uint64_t fnv = 0xcbf29ce484222325ull;
    for (const auto byte : image.pixels)
        fnv = (fnv ^ byte) * 0x100000001b3ull;
    std::uint64_t expected = 0;
    try { expected = std::stoull(argv[4], nullptr, 16); }
    catch (...) { return 5; }
    std::cout << "width=" << image.width << " height=" << image.height
              << " decoded_bytes=" << image.pixels.size()
              << " fnv64=" << std::hex << fnv << '\n';
    return image.width == std::stoi(argv[2]) && image.height == std::stoi(argv[3]) &&
        fnv == expected ? 0 : 6;
}
