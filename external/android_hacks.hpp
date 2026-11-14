#pragma once
#include <functional>

// hack to bypass missing C++23 features in LLVM 12-16 libc++
// defunct
#ifdef __ANDROID__
namespace std {
    template <class T>
    using move_only_function = std::function<T>;
}
#endif
