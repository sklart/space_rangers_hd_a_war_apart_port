// Read-only observation of the original BuildTexUnions result; no algorithm replacement.
#include "CBitmap.hpp"
#include "CBuf.hpp"
#include <cstdlib>
#include <fstream>
#include <string>
#include <stdexcept>
void ObserveOriginalTextureUnion(const CBitmap& bitmap,int index){
    const char* folder=std::getenv("MATRIXGAME_UNION_OUTPUT");
    if(!folder)throw std::runtime_error("union observation output required");
    Base::CBuf buffer;bitmap.SaveInDDSUncompressed(buffer);
    std::ofstream output(std::string(folder)+"/union-"+std::to_string(index)+".dds",std::ios::binary);
    output.write(static_cast<const char*>(buffer.Get()),buffer.Len());
    if(!output)throw std::runtime_error("union observation write failed");
}
