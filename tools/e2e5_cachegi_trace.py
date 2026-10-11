"""Disposable checkpoints around the original GameSettings GI rescale path."""

RESOURCE = 'ResourceKey == u"Bm.FormGameSet2.2bg"'


def apply(source: str) -> str:
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
