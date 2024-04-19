#pragma once

namespace yuubi {

// Non-copyable mixin to be inherited by classes that should not be copied, e.g. Device.
// https://en.wikibooks.org/wiki/More_C%2B%2B_Idioms/Non-copyable_Mixin
class NonCopyable {
public:
    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;

protected:
    NonCopyable() = default;
    ~NonCopyable() = default;
};
}
