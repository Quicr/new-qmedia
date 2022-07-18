#pragma once

#pragma once

#include <type_traits>

#if defined(__linux__) || defined(__APPLE__)
#define EXPORT
#define CALL
#elif _WIN32
#define EXPORT __declspec(dllexport)
#define CALL __stdcall
#endif

template <typename T> using CallbackType = std::add_pointer_t<T>;

template <typename... Args> struct Callback
{
    using callback_type = CallbackType<void CALL(void *, Args...)>;

    callback_type callback_{};

    void *user_data_{};

    /// Invoke the callback with the given arguments |args|.
    constexpr void operator()(Args... args) const noexcept
    {
        if (callback_ && user_data_)
        {
            (*callback_)(user_data_, std::forward<Args>(args)...);
        }
    }
};