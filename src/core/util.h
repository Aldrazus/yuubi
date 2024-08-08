#pragma once

namespace yuubi {

// From Google's C++ style guide:
// https://google.github.io/styleguide/cppguide.html#Copyable_Movable_Types
class Copyable {
public:
    Copyable() = default;
    Copyable(const Copyable&) = default;
    Copyable& operator=(const Copyable&) = default;
};

class NonCopyable {
public:
    NonCopyable() = default;
    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;

    NonCopyable(NonCopyable&&) = default;
    NonCopyable& operator=(NonCopyable&&) = default;
};

class NonCopyableOrMovable {
public:
    NonCopyableOrMovable() = default;
    NonCopyableOrMovable(const NonCopyableOrMovable&) = delete;
    NonCopyableOrMovable& operator=(const NonCopyableOrMovable&) = delete;

    NonCopyableOrMovable(NonCopyableOrMovable&&) = delete;
    NonCopyableOrMovable& operator=(NonCopyableOrMovable&&) = delete;
};

}
