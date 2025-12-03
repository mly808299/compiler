// Tracer.h
#ifndef TRACER_H
#define TRACER_H

#include <chrono>
#include <fstream>
#include <string>
#include <vector>
#include "nlohmann/json.hpp"

struct TraceEvent {
    std::string name; // نام مرحله
    std::string type; // start / end
    double timestamp; // زمان بر حسب میلی‌ثانیه
    int nodeCount;    // تعداد نودهای AST (در صورت نیاز)
};

class Tracer {
private:
    std::vector<TraceEvent> events;
    std::chrono::steady_clock::time_point startTime;

    double currentTimeMs() {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::milli>(now - startTime).count();
    }

public:
    Tracer() { startTime = std::chrono::steady_clock::now(); }

    void startEvent(const std::string &name, int nodeCount = 0) {
        events.push_back({name, "start", currentTimeMs(), nodeCount});
    }

    void endEvent(const std::string &name, int nodeCount = 0) {
        events.push_back({name, "end", currentTimeMs(), nodeCount});
    }

    void saveToFile(const std::string &filename) {
        nlohmann::json j;
        for (auto &e : events) {
            j.push_back({{"name", e.name}, {"type", e.type}, {"timestamp", e.timestamp}, {"nodeCount", e.nodeCount}});
        }
        std::ofstream f(filename);
        f << j.dump(4);
    }
};

#endif
