// Print original startup failures before main; never swallow or continue them.
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <exception>
#include <stdlib.h>
namespace {
void Invalid(const wchar_t* expression,const wchar_t* function,
             const wchar_t* file,unsigned line,uintptr_t) {
 std::fwprintf(stderr,L"WINDOWS_INVALID_PARAMETER %ls %ls %ls:%u\n",
  expression?expression:L"",function?function:L"",file?file:L"",line);
 std::fflush(stderr);std::abort();
}
int Diagnostics() {
 _set_invalid_parameter_handler(Invalid);
 std::set_terminate([] {
  try {if(std::current_exception())std::rethrow_exception(std::current_exception());}
  catch(const std::exception& error){std::fprintf(stderr,"WINDOWS_UNCAUGHT %s\n",error.what());}
  catch(...){std::fputs("WINDOWS_UNCAUGHT non-std exception\n",stderr);}
  std::fflush(stderr);std::abort();
 });
 return 0;
}
typedef int(*Initializer)();
#pragma section(".CRT$XIB",long,read)
__declspec(allocate(".CRT$XIB")) Initializer diagnostic_initializer=Diagnostics;
}
