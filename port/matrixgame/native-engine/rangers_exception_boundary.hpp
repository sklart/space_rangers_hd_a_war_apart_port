// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "runtime_support.hpp"
#include <exception>
#include <new>
#include <utility>

namespace srhd_awa::matrixgame {
// The original Robot/Gov handlers recognize pas::Raised and std::bad_alloc
// through pas::caught_object(). Native device, resource and loop code also
// throws standard exceptions; translate those only at the Robot ABI boundary,
// after the native stack has unwound. Preserve existing Pascal exceptions.
template<class F> decltype(auto) TranslateRangersExceptions(F&& operation) {
    try { return std::forward<F>(operation)(); }
    catch(const pas::Raised&) { throw; }
    catch(const std::bad_alloc&) { throw; }
    catch(const std::exception& error) {
        pas::raise(pas::make_exception<pas::Exception>(pas::AnsiString(error.what())));
    }
}
}
