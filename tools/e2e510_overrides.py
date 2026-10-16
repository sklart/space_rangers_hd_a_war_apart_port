"""Resource reserve, measured arithmetic reuse, and generation work counters."""


def once(text, old, new):
    if text.count(old) != 1:
        raise RuntimeError(f"Follow-up boundary changed: {old[:100]!r}")
    return text.replace(old, new, 1)


def apply(path, text):
    if path == 'ab_WorldLine.cpp':
        # Projection already stored these coordinates in binary64. Reuse the
        # existing mode-aware ARM64 round helper without widening them again.
        text = '#include "e2e5_round.hpp"\n' + text.replace('System::Round(',
            'srhd_awa::platform::e2e5_round::Round(')
    if path == 'aMyFunction.cpp':
        marker = '    std::int32_t IntegerPointDistancePlusOne(Types::TPoint PointA, Types::TPoint PointB) {'
        text = once(text, marker, marker + '''
#if defined(__aarch64__)
        const std::int64_t dx = static_cast<std::int64_t>(PointA.X) - PointB.X;
        const std::int64_t dy = static_cast<std::int64_t>(PointA.Y) - PointB.Y;
        if (dx >= -46340 && dx <= 46340 && dy >= -46340 && dy <= 46340) {
            const auto squared = dx * dx + dy * dy;
            if (squared <= 2147483647) {
                // For an integer in this range, sqrt cannot round across an
                // integer boundary. Perfect squares are represented exactly.
                return static_cast<std::int32_t>(std::sqrt(static_cast<double>(squared))) + 1;
            }
        }
#endif''')
        # Subtract directly into the original binary32 locals. An exponent
        # gap large enough to exceed Extended precision cannot affect binary32
        # rounding. Products need at most 48 bits and are exact in binary64.
        # Keep the original Extended sum and square root after the exact products.
        start = text.index('    float PointDistanceSquared(')
        end = text.index('\n    //', start)
        body = text[start:end]
        original = body
        body = body.replace('static_cast<long double>(PointA.X) - PointB.X', 'PointA.X - PointB.X')
        body = body.replace('static_cast<long double>(PointA.Y) - PointB.Y', 'PointA.Y - PointB.Y')
        body = body.replace('static_cast<long double>(X) * X + static_cast<long double>(Y) * Y',
            'static_cast<long double>(static_cast<double>(X) * X) + static_cast<long double>(static_cast<double>(Y) * Y)')
        # x87 precision-control bits can round each product to Single. The
        # ARM64 original uses binary128 intermediates and has no such mode.
        text = text[:start] + '#if defined(__aarch64__)\n' + body + '\n#else\n' + original + '\n#endif\n' + text[end:]
    if path == 'fGameSettings.cpp':
        # These counters describe completed work, not an elapsed-time estimate.
        patterns = [(2, 'cpp_range_4', 'Stars'), (3, 'cpp_range_16', 'Constellations'),
                    (4, 'cpp_range_17', 'Planets')]
        for stage, loop, collection in patterns:
            marker = f'cpp_range_{loop.split("_")[-1]}.next(I); ) {{'
            text = once(text, marker, marker + f'\n                srhd_awa::platform::e2e3_generation_state::Work({stage}, I, pas::list_count(aGalaxy::Galaxy->{collection}));')
        # Only count a warmup turn after its galaxy calculation completes.
        done = '                e2e_galaxy_turn_us += std::chrono::duration_cast<std::chrono::microseconds>(\n                    std::chrono::steady_clock::now() - e2e_galaxy_turn_begin).count();'
        text = once(text, done, done + '\n                srhd_awa::platform::e2e3_generation_state::Work(6, I, aGalaxyStruct::GalaxyWarmupTurns);')
        for stage, collection in [(2, 'Stars'), (3, 'Constellations'), (4, 'Planets')]:
            marker = f'            srhd_awa::platform::e2e3_generation_state::SetStage({stage});'
            text = once(text, marker, marker + f'\n            srhd_awa::platform::e2e3_generation_state::Work({stage}, 0, pas::list_count(aGalaxy::Galaxy->{collection}));')
        marker = '            srhd_awa::platform::e2e3_generation_state::SetStage(6);'
        text = once(text, marker, marker + '\n            srhd_awa::platform::e2e3_generation_state::Work(6, 0, aGalaxyStruct::GalaxyWarmupTurns);')
    if path == 'aGalaxy.cpp':
        start = text.index('    void TGalaxy::GenerateGalaxyLayout(')
        end = text.index('\n    }', start)
        body = text[start:end]
        body = once(body, '        do {\n            WorkingPolygon =',
            '        do {\n            srhd_awa::platform::e2e3_generation_state::Attempt(1, GenerationAttempts + 1);\n            WorkingPolygon =')
        body = once(body, 'cpp_range_13.next(I); ) {', 'cpp_range_13.next(I); ) {\n                srhd_awa::platform::e2e3_generation_state::Work(1, I, pas::list_count(Galaxy->Stars));')
        text = '#include "e2e3_generation_state.hpp"\n' + text[:start] + body + text[end:]
    if path == 'fIntroduction.cpp':
        # Place storage after includes, where pas::WideString is defined.
        marker_namespace = 'namespace fIntroduction {'
        text = once(text, marker_namespace, marker_namespace + '\n    static std::array<pas::WideString, 8> e2e_progress_labels;\n    static std::array<int, 8> e2e_progress_states;')
        text = '#include <array>\n' + text
        text = once(text, '    void TfIntroduction::OnOpen() {', '''    void TfIntroduction::OnOpen() {
        for (int i = 0; i < 8; ++i) {
            auto* label = pas::checked_cast<GI_Label::TLabelGI*>(GetByName(pas::view(
                static_cast<pas::WideString>(pas::concat_ansi({"MCW", SysUtils::IntToStr(i + 1)})))));
            e2e_progress_labels[i] = label->GetText();
            e2e_progress_states[i] = 0;
        }''')
        text = once(text, '    void TfIntroduction::OnClose() {', '''    void TfIntroduction::OnClose() {
        for (int i = 0; i < 8; ++i) {
            auto* label = pas::checked_cast<GI_Label::TLabelGI*>(GetByName(pas::view(
                static_cast<pas::WideString>(pas::concat_ansi({"MCW", SysUtils::IntToStr(i + 1)})))));
            label->SetText(e2e_progress_labels[i]);
        }''')
        marker = '        ProgressPulsePhase = ProgressPulsePhase + 0.05L;'
        text = once(text, marker, '''        for (int work_stage = 0; work_stage < 8; ++work_stage) {
            const auto state = srhd_awa::platform::e2e3_generation_state::Progress(work_stage);
            if (state == 0 || state == e2e_progress_states[work_stage]) continue;
            auto* label = pas::checked_cast<GI_Label::TLabelGI*>(GetByName(pas::view(
                static_cast<pas::WideString>(pas::concat_ansi({"MCW", SysUtils::IntToStr(work_stage + 1)})))));
            const auto& caption = e2e_progress_labels[work_stage];
            pas::WideString suffix;
            if (state < 0)
                suffix = u" (попытка "_w + static_cast<pas::WideString>(SysUtils::IntToStr(-state)) + u")"_w;
            else
                suffix = static_cast<pas::WideString>(pas::concat_ansi({" (", SysUtils::IntToStr(state - 1), "%)"}));
            auto updated = caption + suffix;
            label->SetText(updated);
            e2e_progress_states[work_stage] = state;
#if defined(E2E5_HOST_SDL_WINDOW)
            const auto content = label->MeasureContentSize(nullptr);
            char progress_detail[144]{};
            std::snprintf(progress_detail, sizeof(progress_detail),
                "HOST generation label=%d state=%d content_width=%d control_width=%d",
                work_stage + 1, state, content.X, label->ClientSize.X);
            srhd_awa::platform::e2e_stage::Log(progress_detail);
#endif
        }
''' + marker)
    return text
