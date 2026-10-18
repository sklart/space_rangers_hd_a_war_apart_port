// Validate and budget the whole decoded STRG body before allocating records.
static bool StoragePreflight(std::span<const byte> data, size_t retained_input) {
    constexpr auto limit = matrixgame::native::StorageLimit;
    size_t at = 0, budget = retained_input;
    const auto add = [&](size_t count, size_t size) {
        if (size && count > (limit - std::min(limit, budget)) / size) return false;
        budget += count * size; return budget <= limit;
    };
    const auto u32 = [&](uint32_t& value) {
        if (at > data.size() || data.size() - at < 4) return false;
        value = uint32_t(data[at]) | uint32_t(data[at + 1]) << 8 |
            uint32_t(data[at + 2]) << 16 | uint32_t(data[at + 3]) << 24;
        at += 4; return true;
    };
    const auto name = [&]() {
        size_t units = 0;
        while (at <= data.size() && data.size() - at >= 2) {
            const auto ch = matrixgame::native::Utf16Unit(data.data() + at); at += 2;
            if (!ch) return add(units * 2 + 1, sizeof(wchar_t)); // Allow geometric wstring growth.
            ++units;
        }
        return false;
    };
    uint32_t records;
    if (!u32(records) || records > INT_MAX || !add(records, sizeof(CStorageRecord))) return false;
    for (uint32_t record = 0; record < records; ++record) {
        uint32_t items;
        if (!name() || !u32(items) || items > INT_MAX || !add(items, sizeof(CStorageRecordItem))) return false;
        for (uint32_t item = 0; item < items; ++item) {
            uint32_t type, bytes;
            if (!name() || !u32(type) || (type & ~uint32_t(ST_COMPRESSED)) > uint32_t(ST_WCHAR) ||
                !u32(bytes) || bytes > data.size() - at || !add(1, sizeof(CDataBuf))) return false;
            size_t decoded = bytes;
            if (type & uint32_t(ST_COMPRESSED)) {
                if (bytes < 8 || memcmp(data.data() + at, "ZL03", 4)) return false;
                const auto p = data.data() + at + 4;
                const auto count = uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
                if (!count || count > (limit + 64999) / 65000) return false;
                decoded = size_t(count) * 65000; // Conservative final-block budget.
            }
            if (!add(std::max(decoded, size_t(24)), 1)) return false;
            at += bytes;
        }
    }
    return at == data.size();
}
