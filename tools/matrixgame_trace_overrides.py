"""Compile full original map tracing; preserve bit tests without aliasing UB."""
import hashlib


def prepare_trace(output):
    name='MatrixGame/src/MatrixMapTrace.cpp';path=output/name
    text=path.read_text(encoding='utf-8')
    old='(*((std::uint32_t *)(&data.dx)) | *((std::uint32_t *)(&data.dy)))'
    if text.count(old)!=3:raise ValueError('Original map trace axis bit tests changed')
    text=text.replace(old,'(std::bit_cast<std::uint32_t>(data.dx) | std::bit_cast<std::uint32_t>(data.dy))')
    path.write_bytes(('#include <bit>\n'+text).encode())
    return {name:hashlib.sha256(path.read_bytes()).hexdigest()}
