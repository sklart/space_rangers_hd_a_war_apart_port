#pragma once
#include <cstdint>
namespace srhd_awa::platform::perf515 {
void Initialize(const char* root);
void Shutdown();
int Mode(); // 0 off, 1 basic, 2 detail; read once before gameplay
bool SceneEnabled(); // explicit scene-mode.txt=on, also requires performance mode
// Copies a bounded RGB565 sample to RAM; only the writer performs filesystem I/O.
void SceneImage(const char* event,int screen,std::uint64_t generation,
                const void* owner,const void* buffer,const void* pixels,
                int pitch,int width,int height,int cursor_x,int cursor_y,
                std::int64_t turn=-1,std::int64_t film=-1,std::int64_t order=-1,
                std::uint64_t frame=0);
void AsyncLog(const char* prefix,const char* message);
void Record(const char* stage,std::uint64_t wall=0,std::int64_t cpu=-1,
            std::int64_t a=0,std::int64_t b=0);
void Frame(int screen);
void TickBatch(std::uint64_t pending,std::uint32_t delivered);
std::int64_t CpuMicros();
class Scope {
 public:
  explicit Scope(const char* stage,bool detail=false);
  ~Scope();
 private:
  const char* stage_{};const char* parent_{};
  std::uint64_t start_{},parent_start_{};std::int64_t cpu_{-1};
};
struct Session {
 explicit Session(const char* root){Initialize(root);}
 ~Session(){Shutdown();}
};
} // namespace srhd_awa::platform::perf515
