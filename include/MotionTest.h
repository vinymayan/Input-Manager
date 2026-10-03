#pragma once
#include <algorithm>
#include <cstdint>
#include <span>

namespace PluginLogic {
    enum class MotionTestResult { kPending, kSuccess, kFailed };

    inline MotionTestResult EvaluateMotionTest(
        std::span<const uint32_t> entered,
        std::span<const uint32_t> expected,
        float elapsed,
        float timeWindow) {

        if (expected.empty() || elapsed > timeWindow ||
            entered.size() > expected.size() ||
            !std::equal(entered.begin(), entered.end(), expected.begin())) {
            return MotionTestResult::kFailed;
        }
        return entered.size() == expected.size() ?
            MotionTestResult::kSuccess : MotionTestResult::kPending;
    }
}
