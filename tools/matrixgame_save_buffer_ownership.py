"""Guard original save buffers until the writer accepts ownership."""

def apply(text: str) -> str:
    declarations = '        EC_Buf::TBufEC* FilmEntry{};'
    queue = '                SaveWriter->QueueSave(FileName, Header, Preview, SecondaryPreview, GameState, Films);'
    free_entry = '''            if (FilmEntry != nullptr) {
                pas::free(FilmEntry);
            }'''
    assert text.count(declarations) == text.count(queue) == text.count(free_entry) == 1
    guard = '''
        // Buffers belong to this caller until QueueSave returns; afterwards
        // the saver thread owns the five queued buffers. FilmEntry is always
        // a local scratch buffer and is released on every exceptional path.
        struct SaveBufferGuard {
            EC_Buf::TBufEC*& header; EC_Buf::TBufEC*& preview;
            EC_Buf::TBufEC*& secondary; EC_Buf::TBufEC*& state;
            EC_Buf::TBufEC*& films; EC_Buf::TBufEC*& entry;
            bool queued{};
            ~SaveBufferGuard() noexcept {
                if (entry) pas::free(entry);
                if (!queued) {
                    if (header) pas::free(header);
                    if (preview) pas::free(preview);
                    if (secondary) pas::free(secondary);
                    if (state) pas::free(state);
                    if (films) pas::free(films);
                }
            }
        } save_buffers{Header, Preview, SecondaryPreview, GameState, Films, FilmEntry};'''
    text = text.replace(declarations, declarations + guard, 1)
    text = text.replace(queue, queue + '\n                save_buffers.queued = true;', 1)
    text = text.replace(free_entry, '''            if (FilmEntry != nullptr) {
                pas::free(FilmEntry);
                FilmEntry = nullptr;
            }''', 1)
    start = text.index('    void TSaver::QueueSave(')
    end = text.index('    void TSaver::virtual_TThreadEC_Execute()', start)
    body = text[start:end]
    marker = '        Start();'
    assert body.count(marker) == 1
    # Start() may fail before signalling the worker. In that case return all
    # five buffers to the caller's guard instead of retaining dangling fields.
    rollback = '''        try { Start(); }
        catch (...) {
            HeaderBuffer = nullptr; PreviewBuffer = nullptr;
            SecondaryPreviewBuffer = nullptr; GameStateBuffer = nullptr;
            FilmBuffer = nullptr;
            throw;
        }'''
    return text[:start] + body.replace(marker, rollback, 1) + text[end:]
