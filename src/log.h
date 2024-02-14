#pragma once

#pragma warning(push, 0)
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>
#pragma warning(pop)

class Log {
public:
    static void Init();

    static std::shared_ptr<spdlog::logger>& GetLogger() { return logger_; }

private:
    static std::shared_ptr<spdlog::logger> logger_;
};

#define UB_TRACE(...) Log::GetLogger()->trace(__VA_ARGS__)
#define UB_INFO(...) Log::GetLogger()->info(__VA_ARGS__)
#define UB_WARN(...) Log::GetLogger()->warn(__VA_ARGS__)
#define UB_ERROR(...) Log::GetLogger()->error(__VA_ARGS__)
#define UB_CRITICAL(...) Log::GetLogger()->critical(__VA_ARGS__)
