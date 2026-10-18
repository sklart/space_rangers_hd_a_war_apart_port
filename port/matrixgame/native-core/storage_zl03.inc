// Bounded native implementation of the original ZL03 block framing.
static int ZL03_UnCompress(CBuf& out, byte* input, int input_size, size_t decoded_limit = matrixgame::native::StorageLimit) {
    using matrixgame::native::StorageLimit;
    out = CBuf{};
    if (decoded_limit > StorageLimit || input_size < 8 || !input || memcmp(input, "ZL03", 4) != 0) return 0;
    const auto read32 = [](const byte* p) {
        return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
    };
    const auto blocks = read32(input + 4);
    if (!blocks || blocks > (decoded_limit + 64999) / 65000) return 0;
    // Check the complete frame before reserving the advertised decoded capacity.
    // A truncated header with a plausible large block count must not trigger that allocation.
    size_t frame = 8;
    for (uint32_t block = 0; block < blocks; ++block) {
        if (frame > size_t(input_size) || size_t(input_size) - frame < 4) return 0;
        const auto packed = read32(input + frame); frame += 4;
        if (!packed || packed > compressBound(65000) || packed > size_t(input_size) - frame) return 0;
        frame += packed;
    }
    if (frame != size_t(input_size)) return 0;
    // Allocate once: repeated vector growth can retain nearly twice the decoded bytes.
    out.Len(std::min(size_t(blocks) * 65000, decoded_limit));
    if (out.Capacity() > decoded_limit) { out = CBuf{}; return 0; }
    size_t cursor = 8, written = 0;
    for (uint32_t block = 0; block < blocks; ++block) {
        if (cursor > size_t(input_size) || size_t(input_size) - cursor < 4) { out.Clear(); return 0; }
        const auto packed = read32(input + cursor); cursor += 4;
        if (!packed || packed > compressBound(65000) || packed > size_t(input_size) - cursor) { out.Clear(); return 0; }
        const auto before = written;
        if (before >= out.Len()) { out.Clear(); return 0; }
        const auto room = std::min(size_t(65000), out.Len() - before);
        uLongf decoded = room;
        uLong consumed = packed;
        const auto result = uncompress2(out.Buff<byte>() + before, &decoded, input + cursor, &consumed);
        if (result != Z_OK || consumed != packed || !decoded || (block + 1 < blocks && decoded != 65000)) {
            out.Clear(); return 0;
        }
        written += decoded; cursor += packed;
    }
    if (cursor != size_t(input_size)) { out.Clear(); return 0; }
    out.Len(written); out.Pointer(0);
    return static_cast<int>(out.Len());
}
static void ZL03_Compression(CBuf& out, byte* input, int input_size) {
    using matrixgame::native::StorageLimit;
    if (input_size < 0 || size_t(input_size) > StorageLimit || (!input && input_size)) ERROR_E;
    out.Clear();
    const uint32_t blocks = (uint32_t(input_size) + 64999) / 65000;
    out.Add("ZL03", 4); out.Add<uint32_t>(blocks);
    for (size_t cursor = 0; cursor < size_t(input_size); cursor += 65000) {
        const auto count = std::min(size_t(65000), size_t(input_size) - cursor);
        std::vector<byte> packed(compressBound(count));
        uLongf size = packed.size();
        if (compress2(packed.data(), &size, input + cursor, count, Z_BEST_COMPRESSION) != Z_OK) ERROR_E;
        if (size > StorageLimit - 4 || out.Len() > StorageLimit - size - 4) ERROR_E;
        out.Add<uint32_t>(static_cast<uint32_t>(size)); out.Add(packed.data(), size);
    }
}
