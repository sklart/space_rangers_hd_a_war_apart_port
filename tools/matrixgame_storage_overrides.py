"""Pinned original CStorage portability edits; no game logic or fake backend."""
import re

INPUTS = {
    'utils.hpp': 'fc7432bf0828efcb3e11bfd930e5107058a4571672abab3929af7b5f4c556f63',
    'CStorage.hpp': '6aafd8b3858ac39e2654cc709666c8c59bc40876dcab139d9a0aea0019c6aa95',
    'CStorage.cpp': '80309d945c6d1922aacd579a8d1af3aaa78f1c3a22e377fee9ec2b448a3940fb',
    'CHeap.hpp': 'fb6fcb64d4bd8254689fe46bb9d64a1407050a92c6a7ec2820493b34692e6ad8',
    'CBlockPar.hpp': 'c746f291ba45996c4150b0543b81f2cd7226cff5b618c7af48394947e4371cde',
}

def patch_storage(files, root, body):
    heap = files['CHeap.hpp'].decode()
    heap = heap.replace('#include "Tracer.hpp"', '#include <cstdlib>\n#include <cstddef>')
    heap = heap.replace('''#if (defined _DEBUG) || (defined FORCE_ENABLE_MEM_SPY)
#define MEM_SPY_ENABLE
//#define DEAD_PTR_SPY_ENABLE
#endif''', '''#if defined(FORCE_ENABLE_MEM_SPY) || defined(MEM_SPY_ENABLE)
#error Native CHeap uses sanitizers; Win32 memory spy is not ported
#endif''')
    heap = heap.replace('static void AllocationError(int zn);', 'static void AllocationError(size_t zn);')
    files['CHeap.hpp'] = heap.encode()
    files['CBlockPar.hpp'] = files['CBlockPar.hpp'].replace(b'#include "Tracer.hpp"', b'#include "utils.hpp"')
    files['utils.hpp'] = files['utils.hpp'].replace(b'std::swprintf(buf, sizeof(buf),', b'std::swprintf(buf, sizeof(buf) / sizeof(buf[0]),')
    h = files['CStorage.hpp'].decode()
    h = re.sub(r'\bDWORD\b', 'dword', h)
    h = re.sub(r'\bBYTE\b', 'byte', h)
    h = h.replace('#include <windows.h>', '''#include <algorithm>
#include <bit>
#include <climits>
#include <span>
#include <utility>
#include "utf16.hpp"
#include "storage_limits.hpp"''')
    h = h.replace('namespace Base {', '''namespace Base {
static_assert(std::endian::native == std::endian::little);
static_assert(sizeof(dword) == 4);
''', 1)
    h = h.replace('    struct SDataBufHeader {', '    #pragma pack(push, 1)\n    struct SDataBufHeader {')
    marker='''        dword allocated_count;
    };'''
    h = h.replace(marker, marker + '\n    #pragma pack(pop)\n    static_assert(sizeof(SDataBufHeader) == 12 && sizeof(SDataBufAllocTableEntry) == 12);')
    start=h.index('    SDataBufAllocTableEntry *TableEntry(int i)')
    h=h[:start]+'''    SDataBufHeader* CheckedHeader() {
        if (Len() < sizeof(SDataBufHeader) || Len() > matrixgame::native::StorageLimit) ERROR_E;
        auto* header = Buff<SDataBufHeader>();
        if (header->element_type_size != 1 && header->element_type_size != 2 &&
            header->element_type_size != 4 && header->element_type_size != 8) ERROR_E;
        if (header->arrays_count > INT_MAX || header->alloc_table_disp < sizeof(SDataBufHeader) ||
            header->alloc_table_disp > Len() || header->arrays_count >
            (Len() - header->alloc_table_disp) / sizeof(SDataBufAllocTableEntry)) ERROR_E;
        return header;
    }
    void ValidateEntry(const SDataBufAllocTableEntry& entry) {
        const auto* header = CheckedHeader();
        if (entry.count > entry.allocated_count || entry.disp < sizeof(SDataBufHeader) ||
            entry.disp > header->alloc_table_disp || entry.allocated_count >
            (header->alloc_table_disp - entry.disp) / size_t(header->element_type_size)) ERROR_E;
    }
''' +h[start:]
    h=body(h, 'SDataBufAllocTableEntry *TableEntry(int i)', '''SDataBufAllocTableEntry *TableEntry(int i) {
        const auto* header = CheckedHeader();
        if (i < 0 || dword(i) >= header->arrays_count) ERROR_E;
        auto* entry = reinterpret_cast<SDataBufAllocTableEntry*>(Buff<byte>() + header->alloc_table_disp +
            size_t(i) * sizeof(SDataBufAllocTableEntry));
        ValidateEntry(*entry); return entry;
    }''')
    h=body(h, 'D *GetFirstElement(SDataBufAllocTableEntry *te)', '''D *GetFirstElement(SDataBufAllocTableEntry *te) {
        ValidateEntry(*te);
        auto* address = Buff<byte>() + te->disp;
        if (reinterpret_cast<uintptr_t>(address) % alignof(D)) ERROR_E;
        return reinterpret_cast<D*>(address);
    }''')
    h=body(h, 'D *GetEndElement(SDataBufAllocTableEntry *te)', '''D *GetEndElement(SDataBufAllocTableEntry *te) {
        auto* begin = GetFirstElement<D>(te);
        if (te->count > (Len() - te->disp) / sizeof(D)) ERROR_E;
        return begin + te->count;
    }''')
    h=h.replace('''    void ExpandArray(dword i, dword sz) {
        Expand(sz);''', '''    void ExpandArray(dword i, dword sz) {
        CheckedHeader();
        if (sz > matrixgame::native::StorageLimit - Len()) ERROR_E;
        Expand(sz);''')
    h=h.replace('''        int ets = StorageTypeSize(st);''', '''        if (uint32_t(st) > uint32_t(ST_WCHAR)) ERROR_E;
        int ets = StorageTypeSize(st);''', 1)
    h=h.replace('ASSERT(sizeof(D) == header->element_type_size);', 'if (sizeof(D) != size_t(header->element_type_size)) ERROR_E;')
    h=h.replace('ASSERT(i < header->arrays_count);', 'if (i >= header->arrays_count) ERROR_E;')
    h=h.replace('''        if (te->count >= te->allocated_count) {''', '''        if (te->count >= matrixgame::native::StorageLimit / sizeof(D) - 16) ERROR_E;
        if (te->count >= te->allocated_count) {''', 1)
    h=h.replace('*GetEndElement<D>(te) = d;', 'memcpy(GetFirstElement<byte>(te) + size_t(te->count) * sizeof(D), &d, sizeof(D));')
    h=h.replace('''        if ((te->count + count) > te->allocated_count) {''', '''        if (count < 0 || (count && !d) || te->count > matrixgame::native::StorageLimit / sizeof(D) - 16 || size_t(count) > matrixgame::native::StorageLimit / sizeof(D) - 16 - te->count) ERROR_E;
        if ((te->count + count) > te->allocated_count) {''')
    h=h.replace('memcpy(GetEndElement<D>(te), d, sizeof(D) * count);', 'if (count) memcpy(GetFirstElement<byte>(te) + size_t(te->count) * sizeof(D), d, sizeof(D) * count);')
    h=h.replace('''        if (newlen > (int)te->allocated_count) {''', '''        if (newlen < 0 || size_t(newlen) > matrixgame::native::StorageLimit / sizeof(D) - 16) ERROR_E;
        if (newlen > (int)te->allocated_count) {''')
    h=h.replace('''        SetArrayLength<D>(i, count);
        memcpy(GetFirst<D>(i), d, count * sizeof(D));''', '''        if (count < 0 || (count && !d)) ERROR_E;
        SetArrayLength<D>(i, count);
        if (count) memcpy(GetFirst<byte>(i), d, size_t(count) * sizeof(D));''')
    h=body(h, 'int AddArray(void)', '''int AddArray(void) {
        auto* header = CheckedHeader();
        const size_t data_size = size_t(header->element_type_size) * 16;
        const size_t extra = data_size + sizeof(SDataBufAllocTableEntry);
        if (header->arrays_count == INT_MAX || extra > matrixgame::native::StorageLimit - Len()) ERROR_E;
        const auto count = header->arrays_count;
        const auto old_table = header->alloc_table_disp;
        Expand(extra); header = Buff<SDataBufHeader>();
        memmove(Buff<byte>() + old_table + data_size, Buff<byte>() + old_table, Len() - extra - old_table);
        header->alloc_table_disp += dword(data_size);
        auto* entry = reinterpret_cast<SDataBufAllocTableEntry*>(Buff<byte>() + header->alloc_table_disp +
            size_t(count) * sizeof(SDataBufAllocTableEntry));
        entry->allocated_count = 16; entry->count = 0;
        entry->disp = old_table; ++header->arrays_count;
        return int(count);
    }''')
    h=h.replace('dword GetArraysCount(void) { return Buff<SDataBufHeader>()->arrays_count; }', 'dword GetArraysCount(void) { return CheckedHeader()->arrays_count; }')
    h=body(h, 'void SetWStr(dword idx, const std::wstring &str)', '''void SetWStr(dword idx, const std::wstring &str) {
        const auto bytes = matrixgame::native::EncodeUtf16LE(str);
        if (bytes.size() / 2 > INT_MAX) ERROR_E;
        SetArrayLength<uint16_t>(idx, int(bytes.size() / 2));
        if (!bytes.empty()) memcpy(GetFirst<byte>(idx), bytes.data(), bytes.size());
    }''')
    h=body(h, 'int AddWStr(const std::wstring &str)', '''int AddWStr(const std::wstring &str) {
        const auto bytes = matrixgame::native::EncodeUtf16LE(str);
        if (bytes.size() / 2 > INT_MAX) ERROR_E;
        std::vector<uint16_t> units(bytes.size() / 2);
        if (!bytes.empty()) memcpy(units.data(), bytes.data(), bytes.size());
        const int index = AddArray();
        AddToArray<uint16_t>(index, units.data(), int(units.size()));
        return index;
    }''')
    h=body(h, 'std::wstring GetAsWStr(int i)', '''std::wstring GetAsWStr(int i) {
        const auto* header = CheckedHeader();
        if (header->element_type_size != 2) ERROR_E;
        const auto* entry = TableEntry(i);
        return matrixgame::native::DecodeUtf16LE(std::span<const byte>(GetFirst<byte>(i), size_t(entry->count) * 2));
    }''')
    h=body(h, 'ParamParser GetAsParamParser(int i)', 'ParamParser GetAsParamParser(int i) { return GetAsWStr(i); }')
    h=body(h, 'int FindAsWStr(const wchar *val, int len)', '''int FindAsWStr(const wchar *val, int len) {
        if (len < 0 || (!val && len)) ERROR_E;
        const auto bytes = matrixgame::native::EncodeUtf16LE(len ? std::wstring_view(val, size_t(len)) : std::wstring_view{});
        const auto* header = CheckedHeader();
        if (header->element_type_size != 2) ERROR_E;
        for (dword i = 0; i < header->arrays_count; ++i) {
            if (GetArrayLength(i) == bytes.size() / 2 &&
                (bytes.empty() || memcmp(bytes.data(), GetFirst<byte>(i), bytes.size()) == 0)) return int(i);
        }
        return -1;
    }''')
    h=h.replace('memcpy(data0 - saved, data0, data1 - data0);', 'memmove(data0 - saved, data0, data1 - data0);')
    h=h.replace('memcpy(Buff<byte>() + (header->alloc_table_disp - saved),', 'memmove(Buff<byte>() + (header->alloc_table_disp - saved),')
    h=h.replace('''    void Compact(void) {
        SDataBufHeader *header = Buff<SDataBufHeader>();''', '''    void Compact(void) {
        ValidateSerialized();
        SDataBufHeader *header = Buff<SDataBufHeader>();''')
    insert='''    void ValidateSerialized() {
        const auto* header = CheckedHeader();
        size_t previous = sizeof(SDataBufHeader);
        for (dword i = 0; i < header->arrays_count; ++i) {
            const auto* entry = TableEntry(int(i));
            if (entry->disp < previous) ERROR_E;
            previous = entry->disp + size_t(entry->allocated_count) * header->element_type_size;
        }
    }
    void ValidateSerialized(EStorageType type) {
        ValidateSerialized();
        if (CheckedHeader()->element_type_size != StorageTypeSize(type)) ERROR_E;
    }
    template<class D> D GetValue(int array, dword index) {
        static_assert(std::is_trivially_copyable_v<D>);
        const auto* entry = TableEntry(array);
        if (sizeof(D) != size_t(CheckedHeader()->element_type_size) || index >= entry->count) ERROR_E;
        D result; memcpy(&result, Buff<byte>() + entry->disp + size_t(index) * sizeof(D), sizeof(D));
        return result;
    }
    template<class D> void SetValue(int array, dword index, D value) {
        static_assert(std::is_trivially_copyable_v<D>);
        const auto* entry = TableEntry(array);
        if (sizeof(D) != size_t(CheckedHeader()->element_type_size) || index >= entry->count) ERROR_E;
        memcpy(Buff<byte>() + entry->disp + size_t(index) * sizeof(D), &value, sizeof(D));
    }
'''
    h=h.replace('    void Compact(void) {', insert + '\n    void Compact(void) {', 1)
    h=h.replace('CStorageRecordItem(CHeap *heap) : m_Name{}, m_Buf(NULL)', 'CStorageRecordItem(CHeap *heap) : m_Name{}, m_Type(ST_INT32), m_Buf(NULL)')
    h=h.replace('    ~CStorageRecordItem();', '''    CStorageRecordItem(CStorageRecordItem&& item) noexcept
      : m_Name(std::move(item.m_Name)), m_Type(item.m_Type), m_Buf(std::exchange(item.m_Buf, nullptr)) {}
    ~CStorageRecordItem();''')
    h=h.replace('    ~CStorageRecord();', '''    CStorageRecord(CStorageRecord&& record) noexcept
      : m_Heap(record.m_Heap), m_Name(std::move(record.m_Name)),
        m_Items(std::exchange(record.m_Items, nullptr)), m_ItemsCount(std::exchange(record.m_ItemsCount, 0)) {}
    ~CStorageRecord();''')
    h=h.replace('    ~CStorage();', '''    CStorage(const CStorage&) = delete;
    CStorage& operator=(const CStorage&) = delete;
    ~CStorage();''')
    array_api = h.index('    template <class D>\n    void AddToArray')
    h = h[:array_api] + h[array_api:].replace('SDataBufHeader *header = Buff<SDataBufHeader>();', 'SDataBufHeader *header = CheckedHeader();')
    files['CStorage.hpp']=h.encode()
    c=files['CStorage.cpp'].decode()
    c=re.sub(r'\bDWORD\b','dword',c); c=re.sub(r'\bBYTE\b','byte',c)
    c=c.replace('#undef ZEXPORT\n#define ZEXPORT __cdecl\n', '')
    c=c.replace('#include "CRC32.hpp"', '''#include "CRC32.hpp"
#include "utf16.hpp"
#include "CFile.hpp"
#include "storage_limits.hpp"
#include <algorithm>
#include <climits>
#include <span>
#include <utility>
#include <vector>''')
    block_start=c.index('void CStorage::StoreBlockPar(')
    block_end=c.index('}  // namespace Base',block_start)
    prefix=c[:c.index('#include <new>')]
    files['CStorage_blockpar.cpp']=(prefix+'#include "CStorage.hpp"\n#include "utils.hpp"\nnamespace Base {\n'+c[block_start:block_end]+'}\n').encode()
    c=c[:block_start]+c[block_end:]
    start=c.index('static int ZL03_UnCompress(');end=c.index('CStorageRecordItem::~CStorageRecordItem()',start)
    core=root/'port/matrixgame/native-core'
    c=c[:start]+(core/'storage_zl03.inc').read_text()+ '\n'+(core/'storage_preflight.inc').read_text()+'\n'+c[end:]
    c=c.replace('dword x = CalcCRC32_Buf(xi, m_Name.c_str(), m_Name.length() * sizeof(wchar));', '''const auto name = matrixgame::native::EncodeUtf16LE(m_Name);
    dword x = CalcCRC32_Buf(xi, name.data(), int(name.size()));''')
    c=c.replace('''    m_Buf->Compact();''', '''    m_Buf->ValidateSerialized(m_Type);
    m_Buf->Compact();''')
    c=body(c,'bool CStorageRecordItem::Load(CBuf &buf)', '''bool CStorageRecordItem::Load(CBuf &buf) {
    auto name = buf.WStr();
    const auto raw_type = buf.Get<dword>();
    const auto type = EStorageType(raw_type & ~uint32_t(ST_COMPRESSED));
    if (uint32_t(type) > uint32_t(ST_WCHAR)) return false;
    const auto size = buf.Get<dword>();
    if (buf.Pointer() > buf.Len() || size > buf.Len() - buf.Pointer() ||
        size > matrixgame::native::StorageLimit) return false;
    CDataBuf candidate(nullptr, type);
    if (raw_type & uint32_t(ST_COMPRESSED)) {
        if (!ZL03_UnCompress(candidate, buf.Buff<byte>() + buf.Pointer(), int(size))) return false;
        buf.Pointer(buf.Pointer() + size);
    } else {
        candidate.Len(size); buf.Get(candidate.Get(), size);
    }
    candidate.ValidateSerialized(type);
    if (!m_Buf) ERROR_E;
    *m_Buf = std::move(candidate);
    m_Name = std::move(name); m_Type = type;
    return true;
}''')
    c=body(c,'void CStorageRecord::AddItem(const CStorageRecordItem &item)', '''void CStorageRecord::AddItem(const CStorageRecordItem &item) {
    if (m_ItemsCount < 0 || size_t(m_ItemsCount) >= matrixgame::native::StorageLimit / sizeof(CStorageRecordItem)) ERROR_E;
    auto* items = static_cast<CStorageRecordItem*>(HAlloc(sizeof(CStorageRecordItem) * size_t(m_ItemsCount + 1), m_Heap));
    try { new(items + m_ItemsCount) CStorageRecordItem(item); } catch (...) { HFree(items, m_Heap); throw; }
    for (int i = 0; i < m_ItemsCount; ++i) {
        new(items + i) CStorageRecordItem(std::move(m_Items[i])); m_Items[i].~CStorageRecordItem();
    }
    HFree(m_Items, m_Heap); m_Items = items; ++m_ItemsCount;
}''')
    c=c.replace('m_Name{rec.m_Name}', 'm_Name(rec.m_Name)')
    c=body(c,'CStorageRecord::CStorageRecord(const CStorageRecord &rec)', '''CStorageRecord::CStorageRecord(const CStorageRecord &rec)
    : m_Heap(rec.m_Heap), m_Name(rec.m_Name), m_Items(nullptr), m_ItemsCount(0) {
    if (rec.m_ItemsCount < 0 || size_t(rec.m_ItemsCount) > matrixgame::native::StorageLimit / sizeof(CStorageRecordItem)) ERROR_E;
    if (!rec.m_ItemsCount) return;
    m_Items = static_cast<CStorageRecordItem*>(HAlloc(sizeof(CStorageRecordItem) * size_t(rec.m_ItemsCount), m_Heap));
    try {
        for (int i = 0; i < rec.m_ItemsCount; ++i) {
            new(m_Items + i) CStorageRecordItem(rec.m_Items[i]); ++m_ItemsCount;
            m_Items[i].InitBuf(m_Heap);
        }
    } catch (...) {
        for (int i = 0; i < m_ItemsCount; ++i) { m_Items[i].ReleaseBuf(m_Heap); m_Items[i].~CStorageRecordItem(); }
        HFree(m_Items, m_Heap); throw;
    }
}''')
    c=body(c,'bool CStorageRecord::Load(CBuf &buf)', '''bool CStorageRecord::Load(CBuf &buf) {
    CStorageRecord loaded(m_Heap);
    loaded.m_Name = buf.WStr();
    const auto count = buf.Get<dword>();
    if (count > INT_MAX || count > (buf.Len() - buf.Pointer()) / 10 ||
        count > matrixgame::native::StorageLimit / sizeof(CStorageRecordItem)) return false;
    if (count) loaded.m_Items = static_cast<CStorageRecordItem*>(HAlloc(sizeof(CStorageRecordItem) * size_t(count), m_Heap));
    for (dword i = 0; i < count; ++i) {
        new(loaded.m_Items + i) CStorageRecordItem(m_Heap); ++loaded.m_ItemsCount;
        if (!loaded.m_Items[i].Load(buf)) return false;
    }
    std::swap(m_Items, loaded.m_Items); std::swap(m_ItemsCount, loaded.m_ItemsCount); m_Name.swap(loaded.m_Name);
    return true;
}''')
    c=body(c,'void CStorage::AddRecord(const CStorageRecord &sr)', '''void CStorage::AddRecord(const CStorageRecord &sr) {
    if (m_RecordsCnt < 0 || size_t(m_RecordsCnt) >= matrixgame::native::StorageLimit / sizeof(CStorageRecord)) ERROR_E;
    auto* records = static_cast<CStorageRecord*>(HAlloc(sizeof(CStorageRecord) * size_t(m_RecordsCnt + 1), m_Heap));
    try { new(records + m_RecordsCnt) CStorageRecord(sr); } catch (...) { HFree(records, m_Heap); throw; }
    for (int i = 0; i < m_RecordsCnt; ++i) {
        new(records + i) CStorageRecord(std::move(m_Records[i])); m_Records[i].~CStorageRecord();
    }
    HFree(m_Records, m_Heap); m_Records = records; ++m_RecordsCnt;
}''')
    c=body(c,'void CStorage::DelRecord(const wchar *table)', '''void CStorage::DelRecord(const wchar *table) {
    for (int i = 0; i < m_RecordsCnt; ++i) if (m_Records[i].GetName() == table) {
        m_Records[i].~CStorageRecord(); --m_RecordsCnt;
        if (i != m_RecordsCnt) {
            new(m_Records + i) CStorageRecord(std::move(m_Records[m_RecordsCnt]));
            m_Records[m_RecordsCnt].~CStorageRecord();
        }
        return;
    }
}''')
    c=body(c,'bool CStorage::Load(const wchar *fname)', """bool CStorage::Load(const wchar *fname) {
    if (!fname) return false;
    CFile file(fname, std::wstring(fname).size());
    file.OpenRead();
    const auto size = file.SizeFull();
    if (size < 12 || uint64_t(size) > matrixgame::native::StorageLimit) return false;
    CBuf buf; buf.Len(size_t(size));
    file.Read(buf.Get(), size_t(size));
    return Load(buf);
}""")
    c=body(c,'bool CStorage::Load(CBuf &buf_in)', '''bool CStorage::Load(CBuf &buf_in) {
    if (buf_in.Len() < 12 || buf_in.Capacity() > matrixgame::native::StorageLimit) return false;
    buf_in.Pointer(0);
    if (buf_in.Get<dword>() != 0x47525453) return false;
    const auto version = buf_in.Get<dword>();
    if (version > 1) return false;
    CBuf decoded; CBuf* body = &buf_in;
    size_t retained = buf_in.Capacity();
    if (version == 1) {
        if (!ZL03_UnCompress(decoded, buf_in.Buff<byte>() + 8, int(buf_in.Len() - 8), matrixgame::native::StorageLimit - retained)) return false;
        body = &decoded; retained += decoded.Capacity();
    }
    if (!StoragePreflight(std::span<const byte>(body->Buff<byte>() + body->Pointer(), body->Len() - body->Pointer()), retained)) return false;
    CStorage loaded(m_Heap);
    const auto count = body->Get<dword>();
    if (count) loaded.m_Records = static_cast<CStorageRecord*>(HAlloc(sizeof(CStorageRecord) * size_t(count), m_Heap));
    for (dword i = 0; i < count; ++i) {
        new(loaded.m_Records + i) CStorageRecord(m_Heap); ++loaded.m_RecordsCnt;
        if (!loaded.m_Records[i].Load(*body)) return false;
    }
    if (body->Pointer() != body->Len()) return false;
    std::swap(m_Records, loaded.m_Records); std::swap(m_RecordsCnt, loaded.m_RecordsCnt);
    return true;
}''')
    files['CStorage.cpp']=c.encode()
