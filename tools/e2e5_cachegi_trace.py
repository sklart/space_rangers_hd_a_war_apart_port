"""Disposable checkpoints around the original GameSettings GI rescale path."""

RESOURCE = 'ResourceKey == u"Bm.FormGameSet2.2bg"'


def apply(source: str) -> str:
    decode = "            WorkingImage->LoadRawGiFromBuffer(SourceBuffer);"
    if source.count(decode) != 1:
        raise RuntimeError("GI resource decode boundary changed")
    diagnostic = '''            if (EC_Str::FindTextOffsetW(ResourceKey, u"Bm.FormRuins."_wref.get(), 0) == 0) {
                const pas::AnsiString e2e_key = static_cast<pas::AnsiString>(ResourceKey);
                GR_gi::TgiHeaderGR e2e_header{};
                if (SourceBuffer == nullptr || SourceBuffer->Data == nullptr ||
                    SourceBuffer->DataSize < static_cast<std::int32_t>(sizeof(e2e_header))) {
                    srhd_awa::platform::e2e_stage::Log("FAIL stage=ruins-gi missing-header");
                    pas::raise(pas::make_exception<pas::Exception>("Ruins GI missing header"_a));
                }
                std::memcpy(&e2e_header, SourceBuffer->Data, sizeof(e2e_header));
                const auto e2e_width = static_cast<std::int64_t>(e2e_header.Bounds.Right) - e2e_header.Bounds.Left;
                const auto e2e_height = static_cast<std::int64_t>(e2e_header.Bounds.Bottom) - e2e_header.Bounds.Top;
                const auto e2e_bytes =
                    e2e_width > 0 && e2e_height > 0 &&
                    e2e_width <= 16384 && e2e_height <= 16384 ?
                    e2e_width * e2e_height * 4 : -1;
                char e2e_detail[320]{};
                std::snprintf(e2e_detail, sizeof(e2e_detail),
                    "ruins gi key=%s bytes=%d bounds=%d,%d,%d,%d width64=%lld height64=%lld rgba64=%lld",
                    e2e_key.c_str(), SourceBuffer->DataSize,
                    e2e_header.Bounds.Left, e2e_header.Bounds.Top,
                    e2e_header.Bounds.Right, e2e_header.Bounds.Bottom,
                    static_cast<long long>(e2e_width), static_cast<long long>(e2e_height),
                    static_cast<long long>(e2e_bytes));
                srhd_awa::platform::e2e_stage::Log(e2e_detail);
                if (e2e_width <= 0 || e2e_height <= 0 ||
                    e2e_width > 16384 || e2e_height > 16384 ||
                    e2e_bytes <= 0 || e2e_bytes > 256LL * 1024 * 1024) {
                    srhd_awa::platform::e2e_stage::Log("FAIL stage=ruins-gi invalid-bounds");
                    pas::raise(pas::make_exception<pas::Exception>("Ruins GI bounds invalid"_a));
                }
            }
'''
    source = source.replace(decode, diagnostic + decode, 1)
    source = '#include "e2e_stage.hpp"\n#include <cstring>\n#include <cstdio>\n#include <cstdint>\n' + source
    anchor = "            LogWideScreenGiRescaleStart();\n            SourceGraph = "
    start = source.rfind(anchor)
    end = source.find("            LogWideScreenGiRescaleDone();", start)
    if start < 0 or end < 0:
        raise RuntimeError("GameSettings generic rescale boundary changed")
    body = source[start:end]
    checkpoints = (
        ("            RenderGiBufferToGraphBuf(SourceBuffer, SourceGraph);",
         "decode_begin", "decode_done"),
        ("            SourceGraph->RescaleRGBA_HW(GR_Main::GameScreenWidth, GR_Main::GameScreenHeight, true, 1, VerticalAlign);",
         "scale_begin", "scale_done"),
        ("                    StoreGraphBufAsRawGiBuffer(SourceBuffer, SourceGraph, 1);",
         "repack_begin", "repack_done"),
    )
    for call, before, after in checkpoints:
        if body.count(call) != 1:
            raise RuntimeError(f"GameSettings rescale call changed: {before}")
        indent = call[:len(call) - len(call.lstrip())]
        def log(stage: str) -> str:
            return (f'{indent}if ({RESOURCE}) '
                    f'GR_Main::AppendLogLineThreadSafe("[E2E5] GameSettings background {stage}"_a);')
        body = body.replace(call, f"{log(before)}\n{call}\n{log(after)}", 1)
    return source[:start] + body + source[end:]
