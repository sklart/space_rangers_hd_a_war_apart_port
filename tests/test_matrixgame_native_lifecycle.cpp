// Exercise the actual pinned base/form lifecycle. RecordingForm is a test fixture,
// not CFormMatrixGame or a substitute for a battle/update implementation.
#include "Form.hpp"
#include "CReminder.hpp"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

static std::vector<std::string> events;
struct RecordingForm final : CForm {
    std::string id;
    explicit RecordingForm(const char* value):id(value) {}
    void Enter() override { events.push_back(id+".enter"); }
    void Leave() override { events.push_back(id+".leave"); }
    void Draw() override { events.push_back(id+".draw"); }
    void Takt(int delta) override { assert(delta==20);events.push_back(id+".tick"); }
    void MouseMove(int x,int y) override { assert(x==7&&y==8);events.push_back(id+".move"); }
    void MouseKey(ButtonStatus edge,int key,int x,int y) override {
        assert(edge==B_DOWN&&key==1&&x==7&&y==8);events.push_back(id+".mouse");
    }
    void Keyboard(bool down,uint8_t code) override { assert(down&&code==32);events.push_back(id+".key"); }
    void SystemEvent(ESysEvent event) override { assert(event==SYSEV_DEACTIVATING);events.push_back(id+".deactivate"); }
};
int main() {
    for(int cycle=0;cycle<100;++cycle) {
        Base::CMain::BaseInit();CForm::StaticInit();events.clear();
        assert(!g_FormFirst&&!g_FormLast&&!g_FormCur);
        {
            RecordingForm a("a");assert(g_FormFirst==&a&&g_FormLast==&a);
            FormChange(&a);assert(g_FormCur==&a);
            g_FormCur->Takt(20);g_FormCur->Draw();g_FormCur->MouseMove(7,8);
            g_FormCur->MouseKey(B_DOWN,1,7,8);g_FormCur->Keyboard(true,32);
            g_FormCur->SystemEvent(SYSEV_DEACTIVATING);
            {
                RecordingForm b("b");assert(g_FormFirst==&a&&g_FormLast==&b);
                FormChange(&b);assert(g_FormCur==&b);
                FormChange(&b); // Preserve original same-form Leave/Enter ordering.
                FormChange(nullptr);assert(!g_FormCur);
            }
            assert(g_FormFirst==&a&&g_FormLast==&a);
        }
        assert(!g_FormFirst&&!g_FormLast&&!g_FormCur);
        const std::vector<std::string> expected={"a.enter","a.tick","a.draw","a.move","a.mouse","a.key",
            "a.deactivate","a.leave","b.enter","b.leave","b.enter","b.leave"};
        assert(events==expected);
        Base::CMain::BaseDeInit();
    }
    for(const auto& event:events) std::printf("%s ",event.c_str());
    std::puts("\nPASS actual base/form lifecycle cycles=100; battle_executed=false");
}
