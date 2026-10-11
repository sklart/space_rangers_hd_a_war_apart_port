"""Experimental Switch-only adapters for the pinned GR_Sound implementation."""

AUDIO_ON_FUNCTIONS = {
    "GR_Music.cpp": {
        "void TMusicControl_Create(TMusicControl* Self)": '''
        EC_Thread::TThreadEC_Create(Self);
        Self->Current = nullptr;
        Self->Queued = nullptr;
        Self->ControlLock = pas::make_critical_section<pas::CriticalSection>();
        if (!GlobalsV::MusicEnabled) return;
        Self->CompletionEvent = WindowsImports::CreateEvent(nullptr, 0, 0, nullptr);
        if (Self->CompletionEvent == 0)
            pas::raise(pas::make_exception<pas::Exception>("CreateEvent"_a));
        Self->Current = pas::construct_call<TMusicUnit>(
            TMusicUnit_Create, pas::literal_pointer(u"vorbisfile.dll"));
        Self->Queued = pas::construct_call<TMusicUnit>(
            TMusicUnit_Create, pas::literal_pointer(u"vorbisfile.dll"));
        Self->Current->CompletionEvent = Self->CompletionEvent;
        Self->Queued->CompletionEvent = 0u;
        Self->SetPriority(EC_Thread::ThreadPriorityAboveNormal);
        Self->Start();
        GR_Main::AppendLogLineThreadSafe("[AUDIO] music workers ready"_a);''',
    },
    "GR_Sound.cpp": {
        "void TSoundControl_Create(TSoundControl* Self)": '''
        EC_Struct::TObjectEx_Create(Self);
        Self->Lock = pas::make_critical_section<pas::CriticalSection>();
        Self->FirstBuffer = nullptr;
        Self->LastBuffer = nullptr;
        Self->DirectSound = nullptr;
        Self->PrimaryBuffer = nullptr;
        if (GlobalsV::SoundEnabled || GlobalsV::MusicEnabled) {
            std::string error;
            if (!srhd_awa::platform::e2e4_game_audio::Open(&error)) {
                GlobalsV::SoundEnabled = false;
                GlobalsV::MusicEnabled = false;
                GR_Main::AppendLogLineThreadSafe(
                    pas::concat_ansi({"[AUDIO] SDL device open failed: ",
                                      pas::AnsiString(error.c_str())}));
            } else {
                GR_Main::AppendLogLineThreadSafe("[AUDIO] SDL device ready"_a);
            }
        }''',
        "void TSoundControl_Destroy(TSoundControl* Self)": '''
        Self->Clear();
        srhd_awa::platform::e2e4_game_audio::Close();
        Self->PrimaryBuffer = nullptr;
        Self->DirectSound = nullptr;
        pas::free(Self->Lock);
        EC_Struct::TObjectEx_Destroy(Self);''',
        "void TSoundBuffer::Clear()": '''
        pas::critical_enter(GR_Main::SoundManager->Lock);
        pas::ScopeExit unlock = [&]() noexcept {
            pas::critical_leave(GR_Main::SoundManager->Lock);
        };
        if (Controller != nullptr) {
            Controller->Buffer = nullptr;
            Controller->Volume = 0.0f;
            Controller = nullptr;
        }
        srhd_awa::platform::e2e4_game_audio::Clear(this);
        VolumeTimer = 0;
        Notify = nullptr;
        DirectBuffer = nullptr;
        BufferBytes = 0;
        Streaming = false;
        Started = false;
        pas::fill_memory(&WaveFormat,
            static_cast<std::int32_t>(sizeof(WaveFormat)), static_cast<std::uint8_t>(0));''',
        "void TSoundBuffer::Init(std::int32_t ByteCount, void* Format)": '''
        if (ByteCount <= 0 || Format == nullptr)
            pas::raise(pas::make_exception<pas::Exception>("Invalid PCM buffer"_a));
        Clear();
        BufferBytes = ByteCount;
        Windows::CopyMemory(&WaveFormat, Format,
            static_cast<std::int32_t>(sizeof(WaveFormat)));''',
        "void TSoundBuffer::InitStream(std::int32_t ChunkBytes, void* Format)": '''
        if (ChunkBytes <= 0 || Format == nullptr)
            pas::raise(pas::make_exception<pas::Exception>("Invalid stream buffer"_a));
        Clear();
        Windows::CopyMemory(&WaveFormat, Format,
            static_cast<std::int32_t>(sizeof(WaveFormat)));
        BufferBytes = ChunkBytes;
        if (!srhd_awa::platform::e2e4_game_audio::PrepareStream(
                this, static_cast<std::size_t>(ChunkBytes), E2E4AudioFormat(&WaveFormat)))
            pas::raise(pas::make_exception<pas::Exception>("SDL stream creation failed"_a));
        WriteOffset = 0;
        LastPlayCursor = 0;
        Streaming = true;
        SetVolume(static_cast<float>(GlobalsV::MusicVolume) *
                  GlobalsV::MusicVolumeScale);
        SetVolumeScale(1.0f);''',
        "void TSoundBuffer::ClearBuf()": '''
        // The bounded stream ring is initialized with PCM silence. Prime it
        // with decoder output before Play. No sound is active here.''',
        "void TSoundBuffer::Write(void* Data, std::uint32_t ByteCount, void* Format)": '''
        if (ByteCount > static_cast<std::uint32_t>(INT32_MAX) ||
            Data == nullptr || Format == nullptr)
            pas::raise(pas::make_exception<pas::Exception>("Invalid PCM sample"_a));
        Init(static_cast<std::int32_t>(ByteCount), Format);
        if (!srhd_awa::platform::e2e4_game_audio::PrepareEffect(
                this, Data, ByteCount, E2E4AudioFormat(&WaveFormat)))
            pas::raise(pas::make_exception<pas::Exception>("SDL PCM preparation failed"_a));''',
        "std::uint8_t TSoundBuffer::WriteStream(std::int32_t Chunk, VorbisFile::TOggWorker*& Decoder)": '''
        if (!Streaming || BufferBytes <= 0 || Decoder == nullptr) return false;
        std::vector<std::uint8_t> pcm(static_cast<std::size_t>(BufferBytes));
        const int first = Chunk == SoundStreamPrimeAll ? 0 : (Chunk + 2) % 3;
        const int count = Chunk == SoundStreamPrimeAll ? 3 : 1;
        if (first < 0 || first > 2) return false;
        for (int i = 0; i < count; ++i) {
            std::int32_t bytes = BufferBytes;
            VorbisFile::ReadVorbisSamples(Decoder, pcm.data(), bytes);
            if (bytes < 0 || bytes > BufferBytes ||
                !srhd_awa::platform::e2e4_game_audio::WriteStreamChunk(
                    this, static_cast<std::size_t>((first + i) % 3),
                    pcm.data(), static_cast<std::size_t>(bytes))) {
                GR_Main::AppendLogLineThreadSafe("[MUSIC] stream refill failed"_a);
                return false;
            }
            if (bytes < BufferBytes) return false;
        }
        return true;''',
        "void TSoundBuffer::Play(std::uint8_t Looping)": '''
        WindowsSdk::ResetEvent(StopEvent);
        if (!srhd_awa::platform::e2e4_game_audio::Play(
                this, Looping || Streaming, Volume * VolumeScale))
            pas::raise(pas::make_exception<pas::Exception>("SDL playback failed"_a));
        Started = true;''',
        "std::int32_t TSoundBuffer::WaitForChunk()": '''
        if (!Streaming) return 0;
        const auto started = MMSystem::timeGetTime();
        while (true) {
            if (WindowsSdk::WaitForSingleObject(StopEvent, 0) ==
                WindowsSdk::WAIT_OBJECT_0) return -1;
            if (VolumeTimer != 0 &&
                MMSystem::timeGetTime() - LastPlayCursor >= VolumeTimer) {
                LastPlayCursor = MMSystem::timeGetTime();
                SetVolumeScale(VolumeScale + VolumeStep);
                if (VolumeScale == 0.0f) return -1;
            }
            const int event = srhd_awa::platform::e2e4_game_audio::PollCompletion(this);
            if (event >= 0) return event;
            if (MMSystem::timeGetTime() - started >= SoundEventPollMs) return 0;
            SysUtilsImports::Sleep(10u);
        }''',
        "std::uint8_t TSoundBuffer::IsPlaying()": '''
        return srhd_awa::platform::e2e4_game_audio::IsPlaying(this);''',
        "void TSoundBuffer::SetVolume(float Value)": '''
        Volume = Value;
        srhd_awa::platform::e2e4_game_audio::SetGain(this, Volume * VolumeScale);''',
        "void TSoundBuffer::StartVolumeRamp(std::uint32_t Interval, float Step)": '''
        VolumeTimer = std::max<std::uint32_t>(Interval, 1u);
        VolumeStep = Step;
        LastPlayCursor = MMSystem::timeGetTime();''',
        "void TSoundBuffer::SetPan(float Value)": '''
        srhd_awa::platform::e2e4_game_audio::SetPan(this, Value);''',
    },
}
