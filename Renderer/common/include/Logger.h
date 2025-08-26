#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <format>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>

template<typename Mutex>
class ImGuiSink : public spdlog::sinks::base_sink<Mutex> {
public:
    struct LogEntry {
        std::string timestamp;
        spdlog::level::level_enum level;
        std::string message;
        
        LogEntry(spdlog::level::level_enum lvl, const std::string& msg, const std::string& time)
            : timestamp(time), level(lvl), message(msg) {}
    };

private:
    std::vector<LogEntry> entries_;
    size_t max_entries_;
    bool auto_scroll_;

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        std::string formatted;
        formatted.resize(msg.payload.size());
        std::copy(msg.payload.begin(), msg.payload.end(), formatted.begin());
        
        auto time_t = std::chrono::system_clock::to_time_t(msg.time);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            msg.time.time_since_epoch()) % 1000;
        
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%H:%M:%S");
        ss << "." << std::setfill('0') << std::setw(3) << ms.count();
        
        if (entries_.size() >= max_entries_) {
            entries_.erase(entries_.begin());
        }
        
        entries_.emplace_back(msg.level, formatted, ss.str());
    }

    void flush_() override {}

public:
    explicit ImGuiSink(size_t max_entries = 10000) 
        : max_entries_(max_entries), auto_scroll_(true) {}
    
    const std::vector<LogEntry>& get_entries() const { return entries_; }
    
    void clear() {
        std::lock_guard<Mutex> lock(this->mutex_);
        entries_.clear();
    }
    
    void set_auto_scroll(bool auto_scroll) { auto_scroll_ = auto_scroll; }
    bool get_auto_scroll() const { return auto_scroll_; }
    
    static void get_level_color(spdlog::level::level_enum level, float* color) {
        switch (level) {
            case spdlog::level::trace:
            case spdlog::level::debug:
                color[0] = 0.5f; color[1] = 0.5f; color[2] = 0.5f; color[3] = 1.0f; // gray
                break;
            case spdlog::level::info:
                color[0] = 0.0f; color[1] = 0.0f; color[2] = 0.0f; color[3] = 1.0f; // black
                break;
            case spdlog::level::warn:
                color[0] = 1.0f; color[1] = 1.0f; color[2] = 0.0f; color[3] = 1.0f; // yellow
                break;
            case spdlog::level::err:
            case spdlog::level::critical:
                color[0] = 1.0f; color[1] = 0.2f; color[2] = 0.2f; color[3] = 1.0f; // red
                break;
            default:
                color[0] = 1.0f; color[1] = 1.0f; color[2] = 1.0f; color[3] = 1.0f; // white
                break;
        }
    }
    
    static const char* getLevelString(spdlog::level::level_enum level) {
        switch (level) {
            case spdlog::level::trace:   return "[TRACE]";
            case spdlog::level::debug:   return "[DEBUG]";
            case spdlog::level::info:    return "[INFO] ";
            case spdlog::level::warn:    return "[WARN] ";
            case spdlog::level::err:     return "[ERROR]";
            case spdlog::level::critical:return "[CRIT] ";
            default:                     return "[DEFAULT]";
        }
    }
};

using ImGuiSink_mt = ImGuiSink<std::mutex>;
using ImGuiSink_st = ImGuiSink<spdlog::details::null_mutex>;

class Logger {
private:
    std::shared_ptr<spdlog::logger> logger_;
    std::shared_ptr<ImGuiSink_mt> imgui_sink_;
    bool debug_enabled_;
    
    static std::unique_ptr<Logger> instance_;

public:
    static Logger& get_instance();
    

    Logger();
    
    std::shared_ptr<spdlog::logger> get_logger() { return logger_; }
    
    std::shared_ptr<ImGuiSink_mt> get_imgui_sink() { return imgui_sink_; }
    
    // Debug control methods
    void set_debug_enabled(bool enabled) { 
        debug_enabled_ = enabled; 
        logger_->info("DEBUG output {}", enabled ? "ENABLED" : "DISABLED");
    }
    bool is_debug_enabled() const { return debug_enabled_; }
    void toggle_debug() { 
        debug_enabled_ = !debug_enabled_; 
        logger_->info("DEBUG output {}", debug_enabled_ ? "ENABLED" : "DISABLED");
    }
    void enable_debug() { 
        if (!debug_enabled_) {
            debug_enabled_ = true; 
            logger_->info("DEBUG output ENABLED");
        }
    }
    void disable_debug() { 
        if (debug_enabled_) {
            debug_enabled_ = false; 
            logger_->info("DEBUG output DISABLED");
        }
    }
    
    template<typename... Args>
    void info(const std::string& fmt, Args&&... args) {
        if constexpr (sizeof...(args) > 0) {
            std::string formatted = std::vformat(fmt, std::make_format_args(args...));
            logger_->info(formatted);
        } else {
            logger_->info(fmt);
        }
    }
    
    template<typename... Args>
    void warn(const std::string& fmt, Args&&... args) {
        if constexpr (sizeof...(args) > 0) {
            std::string formatted = std::vformat(fmt, std::make_format_args(args...));
            logger_->warn(formatted);
        } else {
            logger_->warn(fmt);
        }
    }
    
    template<typename... Args>
    void error(const std::string& fmt, Args&&... args) {
        if constexpr (sizeof...(args) > 0) {
            std::string formatted = std::vformat(fmt, std::make_format_args(args...));
            logger_->error(formatted);
        } else {
            logger_->error(fmt);
        }
    }
    
    template<typename... Args>
    void debug(const std::string& fmt, Args&&... args) {
        if (!debug_enabled_) {
            return; // Skip debug output when disabled
        }
        
        if constexpr (sizeof...(args) > 0) {
            std::string formatted = std::vformat(fmt, std::make_format_args(args...));
            logger_->debug(formatted);
        } else {
            logger_->debug(fmt);
        }
    }
    
    void clear() {
        if (imgui_sink_) {
            imgui_sink_->clear();
        }
    }
};


#define LOG_INFO(...) Logger::get_instance().info(__VA_ARGS__)
#define LOG_WARN(...) Logger::get_instance().warn(__VA_ARGS__)
#define LOG_ERROR(...) Logger::get_instance().error(__VA_ARGS__)
#define LOG_DEBUG(...) Logger::get_instance().debug(__VA_ARGS__)