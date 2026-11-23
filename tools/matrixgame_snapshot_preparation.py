"""Roll back a failed in-memory snapshot before the world is detached."""

def apply(text: str) -> str:
    entry = '    void SaveGameToMemorySnapshot() {'
    allocation = '        MemorySnapshotBuffer = pas::construct_call<EC_Buf::TBufEC>(EC_Buf::TBufEC_Create);'
    mutation = '        aPlayer::GetPlayer()->SetMoney(0);'
    assert text.count(entry) == text.count(allocation) == text.count(mutation) == 1
    precondition = '''
        if (GlobalsV::MemorySnapshotActive || MemorySnapshotBuffer || !aGalaxy::Galaxy || !aPlayer::GetPlayer())
            pas::raise(pas::make_exception<pas::Exception>("invalid in-memory snapshot state"_a));'''
    guard = '''
        // Until world mutation begins, a failed serialization is disposable:
        // keep the live world and its save counter, and release the partial
        // snapshot. Once mutation starts, retain the buffer for recovery.
        struct SnapshotPreparationGuard {
            aGalaxy::TGalaxy* world;
            std::int32_t save_count;
            bool mutation_started{};
            ~SnapshotPreparationGuard() noexcept {
                if (!mutation_started && !GlobalsV::MemorySnapshotActive && aGalaxy::Galaxy == world) {
                    world->SaveCount = save_count;
                    if (MemorySnapshotBuffer) pas::free(MemorySnapshotBuffer);
                    MemorySnapshotBuffer = nullptr;
                }
            }
        } snapshot_preparation{aGalaxy::Galaxy, aGalaxy::Galaxy->SaveCount};'''
    text = text.replace(entry, entry + precondition, 1)
    text = text.replace(allocation, allocation + guard, 1)
    text = text.replace(mutation, '        snapshot_preparation.mutation_started = true;\n' + mutation, 1)
    return text
