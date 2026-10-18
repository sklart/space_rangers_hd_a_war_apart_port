"""Prepare pinned original MatrixLib CPU sources; never edit the submodule."""
import argparse
import hashlib
import json
from pathlib import Path
from matrixgame_storage_overrides import INPUTS as STORAGE_INPUTS, patch_storage
from matrixgame_blockpar_overrides import INPUTS as BLOCKPAR_INPUTS, patch_blockpar

INPUTS = {
    'CFile.cpp': '09987398ba4a3805bee5a7a8595ec19a674f3a09d98a4fd318b4711ea9348a49',
    'CFile.hpp': '20ff28cc056a6754ea8866a250f0dd866b4d823e90af18938699a736147d0abc',
    'CMain.hpp': 'fa22e13fe14e0484598320601203ed5289176d8a4926a2bb3fbbd51f86805d1b',
    'CException.hpp': 'b07668359bd5c9e383d5dbd652a727389faecfddc500d3d92584c32052d08303',
    'CBuf.cpp': '7a404ddcd08c96f985c07e67ed44915dae83c768e9b39617361df24891670eac',
    'CBuf.hpp': '37beddebc23e07e50a35171782a581c5901a8b27a292c5bc864f293466d9b1a0',
    "BaseDef.hpp": "087fec96990f3e14a9988e8f1dec9c9a22ab14f2e1cc38fa68f5b3970d00f1e6",
    "CRC32.hpp": "287fb8f9b67853c487beb3b6ce96c45aa0bcdf617b348407c8bc0caed4ce69fe",
    "CRC32.cpp": "f6bf4e86be9ae18dc836e0e5dc87536e36af09c7bf33705064e4a493fafe3c76",
}

INPUTS.update(STORAGE_INPUTS)
INPUTS.update(BLOCKPAR_INPUTS)

def replace_body(text, signature, replacement):
    if text.count(signature) != 1:
        raise ValueError("Unexpected MatrixLib function: " + signature)
    start = text.index(signature)
    opening = text.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        if text[end] == "{": depth += 1
        if text[end] == "}": depth -= 1
        end += 1
    return text[:start] + replacement + text[end:]

def prepare(output: Path):
    root = Path(__file__).resolve().parents[1]
    if output.resolve().is_relative_to((root / "upstream").resolve()):
        raise ValueError("Refusing to write into pinned upstream sources")
    source = root / "upstream/matrixgame/MatrixLib/Base"
    files = {name: (source / name).read_bytes().replace(b"\r\n", b"\n") for name in INPUTS}
    # Validate every input before producing any source from a different revision.
    for name, data in files.items():
        if hashlib.sha256(data).hexdigest() != INPUTS[name]:
            raise ValueError("Pinned MatrixLib input changed: " + name)
    text = files["BaseDef.hpp"].decode("utf-8")
    for old, new in [
        ("using dword = unsigned long;", "using dword = std::uint32_t;"),
        ("typedef unsigned long dword;", "typedef std::uint32_t dword;"),
    ]:
        if text.count(old) != 1:
            raise ValueError("Unexpected dword declaration")
        text = text.replace(old, new)
    text = text.replace("#pragma once", "#pragma once\n#include <cstdint>", 1)
    # Keep standard wchar_t ABI; CBuf uses an explicit UTF-16LE boundary.
    text = text.replace('asm volatile("int $0x03");', "__builtin_trap();")
    files["BaseDef.hpp"] = text.encode("utf-8")
    header = files["CBuf.hpp"].decode("utf-8")
    for signature in [
        "std::wstring WStr(void)", "void WStr(const std::wstring &str)",
        "void WStrNZ(const std::wstring &str)", "std::wstring WStrText(void)",
        "void WStrText(const std::wstring& str)",
    ]:
        header = replace_body(header, signature, signature + ";")
    header = header.replace("    size_t Len(void) const { return m_Buf.size(); }",
        "    size_t Len(void) const { return m_Buf.size(); }\n    size_t Capacity(void) const { return m_Buf.capacity(); }")
    header = header.replace("#include <vector>", "#include <vector>\n#include <type_traits>\n#include <utility>")
    header = header.replace("    ~CBuf() = default;", """    ~CBuf() = default;
    CBuf(const CBuf&) = default;
    CBuf& operator=(const CBuf&) = default;
    CBuf(CBuf&& other) noexcept
      : m_Pointer(std::exchange(other.m_Pointer, 0)), m_Buf(std::move(other.m_Buf)) {}
    CBuf& operator=(CBuf&& other) noexcept {
        if (this != &other) {
            m_Pointer = std::exchange(other.m_Pointer, 0); m_Buf = std::move(other.m_Buf);
        }
        return *this;
    }""")
    header = header.replace("return *(D *)(m_Buf.data() + m_Pointer - sizeof(D));",
        "static_assert(std::is_trivially_copyable_v<D>);\n        D value;\n        memcpy(&value, m_Buf.data() + m_Pointer - sizeof(D), sizeof(D));\n        return value;")
    header = header.replace("*(D*)(m_Buf.data() + m_Pointer) = zn;",
        "static_assert(std::is_trivially_copyable_v<D>);\n        memcpy(m_Buf.data() + m_Pointer, &zn, sizeof(D));")
    header = replace_body(header, "void Expand(size_t sz)", """void Expand(size_t sz) {
        if (sz > m_Buf.max_size() - m_Buf.size()) { ERROR_E; }
        m_Buf.resize(m_Buf.size() + sz);
    }""")
    header = replace_body(header, "inline void TestGet(size_t len)", """inline void TestGet(size_t len) {
        if (m_Pointer > m_Buf.size() || len > m_Buf.size() - m_Pointer) { ERROR_E; }
    }""")
    header = replace_body(header, "void TestAdd(size_t len)", """void TestAdd(size_t len) {
        if (m_Pointer > m_Buf.size() || len > m_Buf.max_size() - m_Pointer) { ERROR_E; }
        if (len > m_Buf.size() - m_Pointer) {
            if (len > m_Buf.max_size() - m_Buf.size()) { ERROR_E; }
            m_Buf.resize(m_Buf.size() + len); // Preserve original growth semantics.
        }
    }""")
    header = replace_body(header, "void WordLoop(uint16_t zn, size_t cnt)", """void WordLoop(uint16_t zn, size_t cnt) {
        if (cnt == 0) return;
        if (cnt > m_Buf.max_size() / 2) { ERROR_E; }
        TestAdd(cnt * 2);
        for (size_t i = 0; i < cnt; ++i, m_Pointer += 2)
            memcpy(m_Buf.data() + m_Pointer, &zn, sizeof(zn));
    }""")
    files["CBuf.hpp"] = header.encode("utf-8")
    implementation = files["CBuf.cpp"].decode("utf-8")
    file_start = implementation.index("void CBuf::LoadFromFile")
    file_end = implementation.index("}  // namespace Base", file_start)
    files["CBuf_file.cpp"] = (implementation[:implementation.index('#include "CBuf.hpp"')] + "#include \"CBuf.hpp\"\n#include \"CFile.hpp\"\nnamespace Base {\n" +
        implementation[file_start:file_end] + "} // namespace Base\n").encode("utf-8")
    implementation = implementation[:file_start] + implementation[file_end:]
    for signature in ["size_t CBuf::WStrLen(void)", "size_t CBuf::WStrTextLen(void)"]:
        implementation = replace_body(implementation, signature, "")
    for include in ['#include "Mem.hpp"\n', '#include "CFile.hpp"\n']:
        implementation = implementation.replace(include, "")
    files["CBuf.cpp"] = implementation.encode("utf-8")
    file_header = files["CFile.hpp"].decode("utf-8")
    file_header = file_header.replace('#include <windows.h>', '#include <cstdint>\n#include <memory>')
    file_header = file_header.replace("DWORD", "dword").replace("__int64", "std::int64_t")
    file_header = file_header.replace("shareMode = FILE_SHARE_READ", "shareMode = FileShareRead")
    file_header = file_header.replace("from = FILE_BEGIN", "from = FileBegin")
    file_header = file_header.replace("namespace Base {", """namespace Base {
inline constexpr dword FileShareRead = 1u;
inline constexpr int FileBegin = 0, FileCurrent = 1, FileEnd = 2;
""", 1)
    start = file_header.index("class BASE_API CFile")
    public = file_header.index("public:", start)
    file_header = (file_header[:start] + "class BASE_API CFile : public CMain {\n"
        "    struct Impl;\n    std::unique_ptr<Impl> impl_;\n\n" + file_header[public:])
    file_header = replace_body(file_header, "static void StaticInit(void)", "static void StaticInit(void);")
    file_header = replace_body(file_header, "void Init(const wchar *filename)", "void Init(const wchar *filename);")
    file_header = replace_body(file_header, "void Init(const std::wstring &filename)", "void Init(const std::wstring &filename);")
    file_header = replace_body(file_header, "bool IsOpen(void)", "bool IsOpen(void);")
    files["CFile.hpp"] = file_header.encode("utf-8")
    patch_storage(files, root, replace_body)
    patch_blockpar(files, replace_body)
    output.mkdir(parents=True, exist_ok=True)
    for name, data in files.items():
        (output / name).write_bytes(data)
    report = {
        "scope": "Original MatrixLib CRC/CBuf core and native CFile API; not a full engine build",
        "upstream_pin": "218115a72fa9c5d43269f4ca84a8596a0bcb82f1",
        "patch": "Fixed-width dword, explicit CBuf UTF-16LE, aligned scalar copies, checked sizes; CRC unchanged",
        "utf16_port": "CBuf explicit UTF-16LE; CStorage name hashing remains OPEN",
        "file_io": "Original CBuf_file.cpp connected to native CFile with existing bounded Package reader",
        "input_hash_format": "SHA256 of LF-normalized UTF-8 source",
        "inputs_sha256": INPUTS,
        "outputs_sha256": {name: hashlib.sha256(data).hexdigest() for name, data in files.items()},
    }
    (output / "source-provenance.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return report

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(prepare(args.output), indent=2))
