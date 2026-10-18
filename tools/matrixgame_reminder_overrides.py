"""Pinned resource cleanup scheduler: native time and callback lifetime safety."""
INPUTS = {
    'CReminder.hpp':'c02ad3ecad355dd1daa170faa384773cf924a13be27fb83d41855fd1f3a5b0c5',
    'CReminder.cpp':'12294822e2c4bbed8dcef7dbc648151b0244ef550addaef82a71137b349f1a22',
}


def patch_reminder(files, body):
    h=files['CReminder.hpp'].decode()
    h=h.replace('#include <cstdint>', '#include <cstdint>\n#include <limits>\n#include <stdexcept>')
    h=h.replace('static int gtime;', 'static std::int64_t gtime;').replace('static int ctime;', 'static std::int64_t ctime;')
    h=h.replace('    int time;', '    std::int64_t time;',1)
    h=h.replace('false if the scheduler has to be deleted or true otherwise.',
        'false requests unlinking the still-registered owner; true leaves unlinking to the callback.')
    h=h.replace('    static SRemindCore *first;', '''    // Single engine/render thread. Callbacks may unlink/delete resources;
    // recursively ticking or resetting a live scheduler is rejected.
    static void NotifyDown(SRemindCore*) noexcept;
    static bool IsIdle() noexcept;
    SRemindCore(const SRemindCore&)=delete;
    SRemindCore& operator=(const SRemindCore&)=delete;
    static SRemindCore *first;''',1)
    h=h.replace('    static void StaticInit(void) {', '''    static void StaticInit(void) {
        if(first || !IsIdle()) throw std::logic_error("reset of live MatrixGame resource scheduler");''',1)
    h=h.replace('    void Down(void) {', '    void Down(void) {\n        NotifyDown(this);',1)
    h=h.replace('    void Use(int nexttime) {', '''    void Use(int nexttime) {
        if(!handler || nexttime<0) throw std::invalid_argument("invalid resource reminder");
        if(gtime<0 || gtime>std::numeric_limits<std::int64_t>::max()-nexttime)
            throw std::overflow_error("resource reminder time");''',1)
    files['CReminder.hpp']=h.encode()
    c=files['CReminder.cpp'].decode()
    c=c.replace('namespace Base {','''namespace Base {
namespace {
bool ticking=false;
SRemindCore* dispatch_owner=nullptr;
bool owner_unlinked=false;
struct TickGuard {
    TickGuard(){ticking=true;}
    ~TickGuard(){dispatch_owner=nullptr;owner_unlinked=false;ticking=false;}
};
}
void SRemindCore::NotifyDown(SRemindCore* core) noexcept {
    if(core==dispatch_owner) owner_unlinked=true;
}
bool SRemindCore::IsIdle() noexcept {return !ticking;}
''',1)
    c=c.replace('int SRemindCore::gtime;', 'std::int64_t SRemindCore::gtime;').replace('int SRemindCore::ctime;', 'std::int64_t SRemindCore::ctime;')
    c=body(c,'void SRemindCore::Takt(int ms)', '''void SRemindCore::Takt(int ms) {
    if(ticking) throw std::logic_error("recursive MatrixGame resource tick");
    if(ms<0 || gtime<0 || ctime<0) throw std::invalid_argument("invalid resource tick time");
    if(first==nullptr) return; // Preserve original idle-clock behavior.
    if(gtime>std::numeric_limits<std::int64_t>::max()-ms)
        throw std::overflow_error("resource tick time");
    TickGuard guard;
    if(current==nullptr) current=first;
    gtime+=ms;
    while(gtime>ctime) {
        int checks=ms/CHECK_TIME;
        if(checks==0) checks=1;
        const auto advance=std::int64_t(checks)*CHECK_TIME;
        if(ctime>std::numeric_limits<std::int64_t>::max()-advance)
            throw std::overflow_error("resource check time");
        ctime+=advance;
        while(checks-->0) {
            current=current->next;
            if(current==nullptr) current=first;
            if(gtime>current->time) {
                auto* target=current;
                dispatch_owner=target;owner_unlinked=false;
                const bool dead=target->handler(target->param);
                const bool was_unlinked=owner_unlinked;
                dispatch_owner=nullptr;owner_unlinked=false;
                // Down/destruction may have advanced current, freed target or
                // reused its address. Never unlink that next/new owner.
                if(!dead && !was_unlinked) target->Down();
                if(current==nullptr) current=first;
                if(current==nullptr) return;
            }
        }
    }
}''')
    files['CReminder.cpp']=c.encode()
