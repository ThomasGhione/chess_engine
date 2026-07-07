#pragma once

#ifdef DEBUG

#include <chrono>
#include <cstdint>
#include <iostream>

class DebugTimer {
public:
    void start() noexcept { t0_ = std::chrono::steady_clock::now(); }

    void us(const char* label) const noexcept {
        const auto t = std::chrono::steady_clock::now();
        const std::chrono::duration<double, std::micro> d = t - t0_;
        std::cout << "[DEBUG] " << label << ": " << d.count() << " us\n";
    }

    void ms(const char* label) const noexcept {
        const auto t = std::chrono::steady_clock::now();
        const std::chrono::duration<double, std::milli> d = t - t0_;
        std::cout << "[DEBUG] " << label << ": " << d.count() << " ms\n";
    }

private:
    std::chrono::steady_clock::time_point t0_{};
};

#define DBG_TIMER_DECLARE(name) DebugTimer name
#define DBG_TIMER_START(name)   (name).start()
#define DBG_TIMER_US(name, msg) (name).us(msg)
#define DBG_TIMER_MS(name, msg) (name).ms(msg)
#define DBG_LOG_STREAM(expr)    do { std::cout << expr; } while (0)
#define DBG_ONLY(...)           do { __VA_ARGS__ } while (0)




#else

#define DBG_TIMER_DECLARE(name)
#define DBG_TIMER_START(name)   ((void)0)
#define DBG_TIMER_US(name, msg) ((void)0)
#define DBG_TIMER_MS(name, msg) ((void)0)
#define DBG_LOG_STREAM(expr)    ((void)0)
#define DBG_ONLY(...)           ((void)0)

#endif
