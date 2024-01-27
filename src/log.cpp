#include "pch.h"
#include "log.h"
#include "spdlog/spdlog.h"

#include <spdlog/sinks/stdout_color_sinks.h>

std::shared_ptr<spdlog::logger> Log::logger_;

void Log::Init() {
    spdlog::set_pattern("%^[%T] %n: %v%$");
    logger_ = spdlog::stdout_color_mt("APPLICATION");
    logger_->set_level(spdlog::level::trace);
}
