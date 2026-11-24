#include <stdexcept>
#include <cstdio>

int main() {
    try { throw std::runtime_error("unrelated failure"); }
    catch (const std::runtime_error&) { std::puts("unrelated exception caught"); }
    try { throw std::runtime_error("COM call through nil interface"); }
    catch (const std::runtime_error&) { std::puts("nil COM exception caught"); }
    try { throw std::logic_error("native frame already acquired"); }
    catch (const std::logic_error&) { std::puts("frame ownership exception caught"); }
}
