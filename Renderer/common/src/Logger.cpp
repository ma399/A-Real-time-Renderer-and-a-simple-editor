#include "Logger.h"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <iostream>
#include <iomanip>
#include <sstream>

std::unique_ptr<Logger> Logger::instance_ = nullptr;

Logger& Logger::get_instance() {
    if (!instance_) {
        instance_ = std::make_unique<Logger>();
    }
    return *instance_;
}

Logger::Logger() : debug_enabled_(false) {  // Debug disabled by default
    try {

        imgui_sink_ = std::make_shared<ImGuiSink_mt>(1000);
        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        
        // log file
        // auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/renderer.log", true);

        std::vector<spdlog::sink_ptr> sinks{consoleSink, imgui_sink_};
        logger_ = std::make_shared<spdlog::logger>("renderer", sinks.begin(), sinks.end());

        logger_->set_level(spdlog::level::debug);
        logger_->set_pattern("[%H:%M:%S.%e] [%l] %v");
        
       
        if (!spdlog::get("renderer")) {
            spdlog::register_logger(logger_);
            spdlog::set_default_logger(logger_);
            logger_->info("Logger initialized successfully (DEBUG output: {})", debug_enabled_ ? "ENABLED" : "DISABLED");
        } else {
            logger_->info("Logger re-initialized with ImGui sink (DEBUG output: {})", debug_enabled_ ? "ENABLED" : "DISABLED");
        }
        
    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Log initialization failed: " << ex.what() << std::endl;
    }
}