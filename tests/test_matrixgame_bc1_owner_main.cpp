#include <cassert>
#include "native_texture.hpp"
namespace nt=srhd_awa::matrixgame::native_texture;
void VerifyOriginalBC1Owner(const std::shared_ptr<nt::Session>&,bool&);
struct CpuImage final:nt::Image{};
int main(){bool fail=false;auto session=std::make_shared<nt::Session>();
    session->upload=[&](const nt::Texture& t)->std::shared_ptr<nt::Image>{nt::ValidateUpload(t);if(fail)throw std::bad_alloc();return std::make_shared<CpuImage>();};
    VerifyOriginalBC1Owner(session,fail);session->Shutdown();assert(nt::HandleCount()==0);
}
