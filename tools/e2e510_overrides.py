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
        text = once(text, marker_namespace, marker_namespace + '''
    static std::array<pas::WideString, 8> e2e_progress_labels;
    static std::array<int, 8> e2e_progress_states;
    static std::array<WindowsSdk::TPoint, 8> e2e_progress_sizes;
    static pas::WideString ProgressSuffix(int state) {
        // A language-neutral ordinal keeps the localized caption intact.
        if (state < 0)
            return u" #"_w + static_cast<pas::WideString>(SysUtils::IntToStr(-state));
        return static_cast<pas::WideString>(pas::concat_ansi({" ", SysUtils::IntToStr(state - 1), "%"}));
    }
    static void SetProgressText(TfIntroduction* form, GI_Label::TLabelGI* label,
                                const pas::WideString& caption, int state) {
        label->SetText(caption + ProgressSuffix(state));
        const auto content = label->MeasureContentSize(nullptr);
        if (content.X + 4 <= label->ClientSize.X) return;
        // Use the existing gap up to the next column's icon/text. Keep the
        // localized caption and font, without covering a neighbouring stage.
        int available = form->GetByName(u"MainPanel"sv)->ClientSize.X - label->LocalPosition.X - 16;
        for (int i = 1; i <= 8; ++i) {
            for (const auto& prefix : {"ICW"_a, "MCW"_a}) {
                auto* peer = form->GetByName(pas::view(static_cast<pas::WideString>(
                    pas::concat_ansi({prefix, SysUtils::IntToStr(i)}))));
                if (peer->LocalPosition.X > label->LocalPosition.X)
                    available = std::min(available, peer->LocalPosition.X - label->LocalPosition.X - 8);
            }
        }
        const auto width = std::max(label->ClientSize.X, std::min(content.X + 4, available));
        if (width != label->ClientSize.X)
            label->SetSize(ClassesImports::Point(width, label->ClientSize.Y));
    }''')
        text = '#include <array>\n#include <cstdlib>\n' + text
        text = once(text, '    void TfIntroduction::OnOpen() {', '''    void TfIntroduction::OnOpen() {
        for (int i = 0; i < 8; ++i) {
            auto* label = pas::checked_cast<GI_Label::TLabelGI*>(GetByName(pas::view(
                static_cast<pas::WideString>(pas::concat_ansi({"MCW", SysUtils::IntToStr(i + 1)})))));
            e2e_progress_labels[i] = label->GetText();
            e2e_progress_sizes[i] = label->ClientSize;
            e2e_progress_states[i] = 0;
#if defined(E2E5_HOST_SDL_WINDOW)
            if (std::getenv("E2E511_HOST_LABEL_BOUNDS")) {
                for (int state : {1, 2, 100, 101, -1, -9999, -2147483647}) {
                    // Only Progress(1), the star-layout search, publishes attempts.
                    if (state < 0 && i != 1) continue;
                    label->SetSize(e2e_progress_sizes[i]);
                    SetProgressText(this, label, e2e_progress_labels[i], state);
                    const auto size = label->MeasureContentSize(nullptr);
                    char detail[192]{};
                    std::snprintf(detail, sizeof(detail),
                        "HOST label-bounds stage=%d state=%d width=%d height=%d control_width=%d control_height=%d fits=%u",
                        i + 1, state, size.X, size.Y, label->ClientSize.X, label->ClientSize.Y,
                        static_cast<unsigned>(size.X + 4 <= label->ClientSize.X && size.Y <= label->ClientSize.Y));
                    srhd_awa::platform::e2e_stage::Log(detail);
                }
                label->SetText(e2e_progress_labels[i]);
                label->SetSize(e2e_progress_sizes[i]);
            }
#endif
        }''')
        text = once(text, '    void TfIntroduction::OnClose() {', '''    void TfIntroduction::OnClose() {
        for (int i = 0; i < 8; ++i) {
            auto* label = pas::checked_cast<GI_Label::TLabelGI*>(GetByName(pas::view(
                static_cast<pas::WideString>(pas::concat_ansi({"MCW", SysUtils::IntToStr(i + 1)})))));
            label->SetText(e2e_progress_labels[i]);
            label->SetSize(e2e_progress_sizes[i]);
        }''')
        marker = '        ProgressPulsePhase = ProgressPulsePhase + 0.05L;'
        text = once(text, marker, '''        for (int work_stage = 0; work_stage < 8; ++work_stage) {
            const auto state = srhd_awa::platform::e2e3_generation_state::Progress(work_stage);
            if (state == 0 || state == e2e_progress_states[work_stage]) continue;
            auto* label = pas::checked_cast<GI_Label::TLabelGI*>(GetByName(pas::view(
                static_cast<pas::WideString>(pas::concat_ansi({"MCW", SysUtils::IntToStr(work_stage + 1)})))));
            const auto& caption = e2e_progress_labels[work_stage];
            SetProgressText(this, label, caption, state);
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
