#include "Events.h"
#include "logger.h" 
#include "Settings.h"
#include "MotionTest.h"
#include <optional>
#include <ranges>

namespace PluginLogic {

    KeyManager::KeyManager() :
        _tapScheduler([this](std::stop_token stopToken) {
            TapSchedulerLoop(stopToken);
        })
    {}

    KeyManager::~KeyManager() {
        _tapScheduler.request_stop();
        _tapCondition.notify_all();
    }

    void KeyManager::RegisterAction(const std::string& name, ComboKey combo, std::function<void()> callback, std::function<void()> releaseCallback) {
        //logger::info("[KeyManager] Registrando acao: '{}' | MainKey: {} | ModKey: {}", name, combo.mainKey, combo.modifierKey);
        _bindings.push_back({ name, combo, callback, releaseCallback });
    }

    void KeyManager::CancelPendingTaps() {
        _bindingGeneration.fetch_add(1, std::memory_order_relaxed);
        {
            std::lock_guard lock(_tapMutex);
            _pendingTapSequences.clear();
            ++_tapScheduleRevision;
        }
        _tapCondition.notify_all();
    }

    void KeyManager::RebuildTapPlans() {
        CancelPendingTaps();
        _tapPlans.clear();

        for (const auto& binding : _bindings) {
            const bool mainIsTap = binding.combo.mainActionType == ActionState::kTap;
            const bool modifierIsTap = binding.combo.modifierActionType == ActionState::kTap;
            if (!mainIsTap && !modifierIsTap) {
                continue;
            }

            TapGroupKey groupKey;
            int requiredTaps = 1;

            // If both sides are taps, the main key remains the primary tap and
            // the modifier tap becomes the anchor.
            if (mainIsTap) {
                groupKey.tapKey = binding.combo.mainKey;
                requiredTaps = binding.combo.mainTapCount;
                groupKey.anchorKey = binding.combo.modifierKey;
                groupKey.anchorState = binding.combo.modifierKey != 0 ?
                    binding.combo.modifierActionType :
                    ActionState::kIgnored;
                groupKey.anchorTapCount = binding.combo.modTapCount;
            }
            else {
                groupKey.tapKey = binding.combo.modifierKey;
                requiredTaps = binding.combo.modTapCount;
                groupKey.anchorKey = binding.combo.mainKey;
                groupKey.anchorState = binding.combo.mainActionType;
                groupKey.anchorTapCount = binding.combo.mainTapCount;
            }
            if (groupKey.anchorState == ActionState::kIgnored) {
                groupKey.anchorKey = 0;
                groupKey.anchorTapCount = 1;
            }
            groupKey.tapBeforeHold =
                groupKey.anchorState == ActionState::kHold &&
                binding.combo.tapBeforeHold;
            groupKey.isGamepad = binding.combo.isGamepad;

            auto planIt = std::find_if(
                _tapPlans.begin(),
                _tapPlans.end(),
                [&](const TapPlan& plan) {
                    return plan.key == groupKey;
                });
            if (planIt == _tapPlans.end()) {
                TapPlan plan;
                plan.key = groupKey;
                _tapPlans.push_back(std::move(plan));
                planIt = std::prev(_tapPlans.end());
            }

            const int normalizedTaps = std::max(requiredTaps, 1);
            const float normalizedWindow = std::max(binding.combo.tapWindow, 0.01f);
            planIt->candidates.push_back({
                binding.name,
                normalizedTaps,
                normalizedWindow,
                std::max(binding.combo.holdDuration, 0.01f)
            });
            planIt->maximumTapCount =
                std::max(planIt->maximumTapCount, normalizedTaps);
            planIt->effectiveWindow =
                std::max(planIt->effectiveWindow, normalizedWindow);
        }

        for (auto& plan : _tapPlans) {
            std::stable_sort(
                plan.candidates.begin(),
                plan.candidates.end(),
                [](const TapCandidate& left, const TapCandidate& right) {
                    return left.requiredTaps < right.requiredTaps;
                });
        }
    }

    bool KeyManager::IsTapCandidateValid(
        const PendingTapSequence& sequence,
        const TapCandidate& candidate) const {

        if (sequence.samples.size() !=
            static_cast<std::size_t>(candidate.requiredTaps) ||
            sequence.samples.empty() ||
            sequence.tapPressInProgress) {
            return false;
        }

        const TimePoint firstTapTime = sequence.samples.front().upTime;
        const TimePoint lastTapTime = sequence.samples.back().upTime;
        const float sequenceDuration = std::chrono::duration<float>(
            lastTapTime - firstTapTime).count();
        if (sequenceDuration > candidate.tapWindow) {
            return false;
        }

        for (const auto& sample : sequence.samples) {
            if (sample.pressDuration >= candidate.holdDuration) {
                return false;
            }
        }

        switch (sequence.plan.key.anchorState) {
        case ActionState::kIgnored:
            return true;

        case ActionState::kPress: {
            if (!sequence.anchorSatisfied) {
                return false;
            }
            const TimePoint combinedStart = std::min(
                firstTapTime,
                sequence.anchorSatisfiedAt);
            const TimePoint combinedEnd = std::max(
                lastTapTime,
                sequence.anchorSatisfiedAt);
            return std::chrono::duration<float>(
                combinedEnd - combinedStart).count() <=
                candidate.tapWindow;
        }

        case ActionState::kHold:
            if (sequence.plan.key.tapBeforeHold) {
                if (!sequence.holdAnchorStarted ||
                    sequence.holdAnchorDownAt < lastTapTime) {
                    return false;
                }
                const float holdStartAfterTap =
                    std::chrono::duration<float>(
                        sequence.holdAnchorDownAt - lastTapTime).count();
                const float activeHoldDuration =
                    std::chrono::duration<float>(
                        Clock::now() - sequence.holdAnchorDownAt).count();
                return holdStartAfterTap <= candidate.tapWindow &&
                    activeHoldDuration >= candidate.holdDuration;
            }
            for (const auto& sample : sequence.samples) {
                if (!sample.anchorDown ||
                    sample.anchorHeldDuration < candidate.holdDuration) {
                    return false;
                }
            }
            return true;

        case ActionState::kTap: {
            const auto earliestAnchorTime = lastTapTime -
                std::chrono::duration_cast<Clock::duration>(
                    std::chrono::duration<float>(candidate.tapWindow));
            const auto latestAnchorTime = firstTapTime +
                std::chrono::duration_cast<Clock::duration>(
                    std::chrono::duration<float>(candidate.tapWindow));

            std::vector<TimePoint> matchingAnchorTaps;
            for (const auto tapTime : sequence.anchorTapTimes) {
                if (tapTime >= earliestAnchorTime &&
                    tapTime <= latestAnchorTime) {
                    matchingAnchorTaps.push_back(tapTime);
                }
            }
            if (matchingAnchorTaps.size() !=
                static_cast<std::size_t>(
                    std::max(sequence.plan.key.anchorTapCount, 1))) {
                return false;
            }

            TimePoint combinedStart = firstTapTime;
            TimePoint combinedEnd = lastTapTime;
            for (const auto tapTime : matchingAnchorTaps) {
                combinedStart = std::min(combinedStart, tapTime);
                combinedEnd = std::max(combinedEnd, tapTime);
            }
            return std::chrono::duration<float>(
                combinedEnd - combinedStart).count() <= candidate.tapWindow;
        }

        default:
            return false;
        }
    }

    std::optional<KeyManager::TapResolution>
    KeyManager::FindTapResolutionLocked(
        uint64_t burstID,
        bool immediateOnly) const {

        std::optional<TapResolution> bestResolution;
        int bestSpecificity = -1;
        int bestTapWeight = -1;

        for (const auto& sequence : _pendingTapSequences) {
            if (sequence.burstID != burstID) {
                continue;
            }

            for (const auto& candidate : sequence.plan.candidates) {
                if (!IsTapCandidateValid(sequence, candidate)) {
                    continue;
                }

                if (immediateOnly) {
                    if (sequence.samples.size() <
                        static_cast<std::size_t>(
                            sequence.plan.maximumTapCount)) {
                        continue;
                    }

                    if (sequence.plan.key.anchorState ==
                        ActionState::kIgnored) {
                        const bool hasDeferredCombo = std::ranges::any_of(
                            _pendingTapSequences,
                            [&](const PendingTapSequence& other) {
                                return other.burstID == burstID &&
                                    (other.plan.key.anchorState ==
                                         ActionState::kPress ||
                                     other.plan.key.anchorState ==
                                         ActionState::kTap ||
                                     (other.plan.key.anchorState ==
                                          ActionState::kHold &&
                                      other.plan.key.tapBeforeHold));
                            });
                        const bool tapCanAnchorAnotherCombo =
                            std::ranges::any_of(
                                _tapPlans,
                                [&](const TapPlan& plan) {
                                    return plan.key.isGamepad ==
                                            sequence.plan.key.isGamepad &&
                                        plan.key.anchorState ==
                                            ActionState::kTap &&
                                        plan.key.anchorKey ==
                                            sequence.plan.key.tapKey;
                                });
                        if (hasDeferredCombo ||
                            tapCanAnchorAnotherCombo) {
                            continue;
                        }
                    }

                    if (sequence.plan.key.anchorState ==
                        ActionState::kTap) {
                        int maximumAnchorTaps =
                            sequence.plan.key.anchorTapCount;
                        for (const auto& other : _pendingTapSequences) {
                            if (other.burstID == burstID &&
                                other.plan.key.anchorState ==
                                    ActionState::kTap &&
                                other.plan.key.anchorKey ==
                                    sequence.plan.key.anchorKey) {
                                maximumAnchorTaps = std::max(
                                    maximumAnchorTaps,
                                    other.plan.key.anchorTapCount);
                            }
                        }
                        if (sequence.plan.key.anchorTapCount <
                            maximumAnchorTaps) {
                            continue;
                        }
                    }
                }

                const int specificity =
                    sequence.plan.key.anchorState ==
                        ActionState::kIgnored ?
                    0 : 1;
                const int tapWeight = candidate.requiredTaps +
                    (sequence.plan.key.anchorState == ActionState::kTap ?
                         std::max(sequence.plan.key.anchorTapCount, 1) :
                         0);
                if (!bestResolution ||
                    specificity > bestSpecificity ||
                    (specificity == bestSpecificity &&
                     tapWeight > bestTapWeight)) {
                    bestSpecificity = specificity;
                    bestTapWeight = tapWeight;
                    bestResolution = TapResolution{
                        candidate.bindingName,
                        sequence.bindingGeneration,
                        burstID,
                        sequence.plan.key.tapKey,
                        sequence.plan.key.anchorKey,
                        sequence.plan.key.anchorState,
                        sequence.plan.key.isGamepad
                    };
                }
            }
        }

        return bestResolution;
    }

    void KeyManager::RemoveTapBurstLocked(
        uint64_t burstID,
        const std::optional<TapResolution>& resolution) {

        _pendingTapSequences.erase(
            std::remove_if(
                _pendingTapSequences.begin(),
                _pendingTapSequences.end(),
                [&](const PendingTapSequence& sequence) {
                    if (sequence.burstID == burstID) {
                        return true;
                    }
                    return resolution &&
                        resolution->anchorState == ActionState::kTap &&
                        sequence.plan.key.isGamepad ==
                            resolution->isGamepad &&
                        sequence.plan.key.tapKey ==
                            resolution->anchorKey;
                }),
            _pendingTapSequences.end());
        ++_tapScheduleRevision;
    }

    void KeyManager::DispatchTapResolution(
        const TapResolution& resolution,
        bool alreadyOnGameThread) {

        if (alreadyOnGameThread) {
            CommitTapResolution(
                resolution.bindingName,
                resolution.bindingGeneration);
            return;
        }

        if (const auto tasks = SKSE::GetTaskInterface()) {
            const std::string bindingName = resolution.bindingName;
            const uint64_t generation = resolution.bindingGeneration;
            tasks->AddTask([this, bindingName, generation]() {
                CommitTapResolution(bindingName, generation);
            });
        }
    }

    void KeyManager::CommitTapResolution(
        const std::string& bindingName,
        uint64_t bindingGeneration) {

        if (bindingGeneration !=
            _bindingGeneration.load(std::memory_order_relaxed)) {
            return;
        }

        auto bindingIt = std::find_if(
            _bindings.begin(),
            _bindings.end(),
            [&](const KeyBinding& binding) {
                return binding.name == bindingName;
            });
        if (bindingIt == _bindings.end()) {
            return;
        }

        auto& binding = *bindingIt;
        ExecuteCallback(binding.name);

        if (binding.combo.mainActionType == ActionState::kHold) {
            _keyStates[binding.combo.mainKey].isHeldFired = true;
        }
        if (binding.combo.modifierActionType == ActionState::kHold) {
            _keyStates[binding.combo.modifierKey].isHeldFired = true;
        }
        if (binding.combo.mainActionType == ActionState::kPress) {
            _keyStates[binding.combo.mainKey].isPressFired = true;
        }
        if (binding.combo.modifierActionType == ActionState::kPress) {
            _keyStates[binding.combo.modifierKey].isPressFired = true;
        }

        const bool needsRelease =
            binding.combo.mainActionType == ActionState::kHold ||
            binding.combo.modifierActionType == ActionState::kHold ||
            binding.combo.mainActionType == ActionState::kPress ||
            binding.combo.modifierActionType == ActionState::kPress;
        if (!needsRelease) {
            return;
        }

        uint32_t anchorKey = 0;
        if (binding.combo.mainActionType == ActionState::kHold ||
            binding.combo.mainActionType == ActionState::kPress) {
            anchorKey = binding.combo.mainKey;
        }
        else if (binding.combo.modifierActionType == ActionState::kHold ||
                 binding.combo.modifierActionType == ActionState::kPress) {
            anchorKey = binding.combo.modifierKey;
        }

        if (anchorKey != 0 && _keyStates[anchorKey].isDown) {
            binding.activeHold = true;
        }
        else {
            ExecuteReleaseCallback(binding.name);
        }
    }

    void KeyManager::TapSchedulerLoop(std::stop_token stopToken) {
        while (!stopToken.stop_requested()) {
            std::vector<TapResolution> resolutions;
            std::unique_lock lock(_tapMutex);
            const uint64_t observedRevision = _tapScheduleRevision;

            if (_pendingTapSequences.empty()) {
                _tapCondition.wait(lock, [&]() {
                    return stopToken.stop_requested() ||
                        _tapScheduleRevision != observedRevision ||
                        !_pendingTapSequences.empty();
                });
            }
            else {
                const auto nextDeadline = std::ranges::min_element(
                    _pendingTapSequences,
                    {},
                    &PendingTapSequence::deadline)->deadline;
                _tapCondition.wait_until(lock, nextDeadline, [&]() {
                    return stopToken.stop_requested() ||
                        _tapScheduleRevision != observedRevision;
                });
            }

            if (stopToken.stop_requested()) {
                return;
            }

            const auto now = Clock::now();
            std::vector<uint64_t> expiredBurstIDs;
            for (const auto& sequence : _pendingTapSequences) {
                if (sequence.deadline <= now &&
                    std::find(
                        expiredBurstIDs.begin(),
                        expiredBurstIDs.end(),
                        sequence.burstID) == expiredBurstIDs.end()) {
                    expiredBurstIDs.push_back(sequence.burstID);
                }
            }

            for (const uint64_t burstID : expiredBurstIDs) {
                const auto resolution =
                    FindTapResolutionLocked(burstID, false);
                RemoveTapBurstLocked(burstID, resolution);
                if (resolution) {
                    resolutions.push_back(*resolution);
                }
            }
            lock.unlock();

            for (const auto& resolution : resolutions) {
                DispatchTapResolution(resolution, false);
            }
        }
    }

    void KeyManager::HandleTapKeyDown(
        uint32_t keyCode,
        bool isGamepad,
        TimePoint now) {

        std::vector<TapResolution> resolutions;
        {
            std::lock_guard lock(_tapMutex);

            std::vector<uint64_t> expiredBurstIDs;
            for (const auto& sequence : _pendingTapSequences) {
                if (sequence.deadline <= now &&
                    std::find(
                        expiredBurstIDs.begin(),
                        expiredBurstIDs.end(),
                        sequence.burstID) == expiredBurstIDs.end()) {
                    expiredBurstIDs.push_back(sequence.burstID);
                }
            }
            for (const uint64_t burstID : expiredBurstIDs) {
                const auto resolution =
                    FindTapResolutionLocked(burstID, false);
                RemoveTapBurstLocked(burstID, resolution);
                if (resolution) {
                    resolutions.push_back(*resolution);
                }
            }

            std::vector<uint64_t> affectedBurstIDs;
            for (auto& sequence : _pendingTapSequences) {
                if (sequence.deadline <= now ||
                    sequence.plan.key.isGamepad != isGamepad) {
                    continue;
                }

                if (sequence.plan.key.tapKey == keyCode) {
                    sequence.tapPressInProgress = true;
                }
                if (sequence.plan.key.anchorState ==
                        ActionState::kPress &&
                    sequence.plan.key.anchorKey == keyCode &&
                    !sequence.samples.empty()) {
                    sequence.anchorSatisfied = true;
                    sequence.anchorSatisfiedAt = now;
                    if (std::find(
                            affectedBurstIDs.begin(),
                            affectedBurstIDs.end(),
                            sequence.burstID) ==
                        affectedBurstIDs.end()) {
                        affectedBurstIDs.push_back(sequence.burstID);
                    }
                }
                if (sequence.plan.key.anchorState ==
                        ActionState::kHold &&
                    sequence.plan.key.tapBeforeHold &&
                    sequence.plan.key.anchorKey == keyCode &&
                    !sequence.samples.empty() &&
                    !sequence.holdAnchorStarted) {
                    const auto lastTapTime =
                        sequence.samples.back().upTime;
                    const float holdStartAfterTap =
                        std::chrono::duration<float>(
                            now - lastTapTime).count();
                    float requiredHoldDuration = 0.0f;
                    bool hasExactCandidate = false;
                    for (const auto& candidate :
                         sequence.plan.candidates) {
                        if (candidate.requiredTaps ==
                            static_cast<int>(
                                sequence.samples.size())) {
                            requiredHoldDuration =
                                hasExactCandidate ?
                                std::min(
                                    requiredHoldDuration,
                                    candidate.holdDuration) :
                                candidate.holdDuration;
                            hasExactCandidate = true;
                        }
                    }
                    if (hasExactCandidate &&
                        holdStartAfterTap <=
                            sequence.plan.effectiveWindow) {
                        sequence.holdAnchorStarted = true;
                        sequence.holdAnchorDownAt = now;
                        const auto holdReadyAt = now +
                            std::chrono::duration_cast<Clock::duration>(
                                std::chrono::duration<float>(
                                    requiredHoldDuration));
                        for (auto& other : _pendingTapSequences) {
                            if (other.burstID == sequence.burstID) {
                                other.deadline = holdReadyAt;
                            }
                        }
                        if (std::find(
                                affectedBurstIDs.begin(),
                                affectedBurstIDs.end(),
                                sequence.burstID) ==
                            affectedBurstIDs.end()) {
                            affectedBurstIDs.push_back(
                                sequence.burstID);
                        }
                    }
                }
            }

            for (const uint64_t burstID : affectedBurstIDs) {
                const auto resolution =
                    FindTapResolutionLocked(burstID, true);
                if (!resolution) {
                    continue;
                }
                RemoveTapBurstLocked(burstID, resolution);
                resolutions.push_back(*resolution);
            }
            ++_tapScheduleRevision;
        }
        _tapCondition.notify_all();

        for (const auto& resolution : resolutions) {
            DispatchTapResolution(resolution, true);
        }
    }

    void KeyManager::HandleTapRelease(
        uint32_t keyCode,
        bool isGamepad,
        float pressDuration,
        TimePoint now) {

        std::vector<const TapPlan*> matchingPlans;
        float burstWindow = 0.0f;
        for (const auto& plan : _tapPlans) {
            const bool isPrimaryTap =
                plan.key.tapKey == keyCode &&
                plan.key.isGamepad == isGamepad;
            const bool canStartTapTapCombo =
                plan.key.anchorState == ActionState::kTap &&
                plan.key.anchorKey == keyCode &&
                plan.key.isGamepad == isGamepad;
            if (isPrimaryTap) {
                matchingPlans.push_back(&plan);
            }
            if (isPrimaryTap || canStartTapTapCombo) {
                burstWindow = std::max(
                    burstWindow,
                    plan.effectiveWindow);
            }
        }

        std::vector<TapResolution> resolutions;
        {
            std::lock_guard lock(_tapMutex);

            std::vector<uint64_t> expiredBurstIDs;
            for (const auto& sequence : _pendingTapSequences) {
                if (sequence.deadline <= now &&
                    std::find(
                        expiredBurstIDs.begin(),
                        expiredBurstIDs.end(),
                        sequence.burstID) == expiredBurstIDs.end()) {
                    expiredBurstIDs.push_back(sequence.burstID);
                }
            }
            for (const uint64_t burstID : expiredBurstIDs) {
                const auto resolution =
                    FindTapResolutionLocked(burstID, false);
                RemoveTapBurstLocked(burstID, resolution);
                if (resolution) {
                    resolutions.push_back(*resolution);
                }
            }

            std::vector<uint64_t> affectedBurstIDs;
            for (const auto& sequence : _pendingTapSequences) {
                if (sequence.plan.key.isGamepad == isGamepad &&
                    sequence.plan.key.anchorState ==
                        ActionState::kHold &&
                    sequence.plan.key.tapBeforeHold &&
                    sequence.plan.key.anchorKey == keyCode &&
                    sequence.holdAnchorStarted &&
                    std::find(
                        affectedBurstIDs.begin(),
                        affectedBurstIDs.end(),
                        sequence.burstID) == affectedBurstIDs.end()) {
                    affectedBurstIDs.push_back(sequence.burstID);
                }
            }
            _pendingTapSequences.erase(
                std::remove_if(
                    _pendingTapSequences.begin(),
                    _pendingTapSequences.end(),
                    [&](const PendingTapSequence& sequence) {
                        return sequence.plan.key.isGamepad == isGamepad &&
                            sequence.plan.key.anchorState ==
                                ActionState::kHold &&
                            sequence.plan.key.tapBeforeHold &&
                            sequence.plan.key.anchorKey == keyCode &&
                            sequence.holdAnchorStarted;
                    }),
                _pendingTapSequences.end());

            for (auto& sequence : _pendingTapSequences) {
                if (sequence.deadline > now &&
                    sequence.plan.key.isGamepad == isGamepad &&
                    sequence.plan.key.anchorState == ActionState::kTap &&
                    sequence.plan.key.anchorKey == keyCode) {
                    if (std::find(
                            sequence.anchorTapTimes.begin(),
                            sequence.anchorTapTimes.end(),
                            now) == sequence.anchorTapTimes.end()) {
                        sequence.anchorTapTimes.push_back(now);
                    }
                    if (std::find(
                            affectedBurstIDs.begin(),
                            affectedBurstIDs.end(),
                            sequence.burstID) ==
                        affectedBurstIDs.end()) {
                        affectedBurstIDs.push_back(sequence.burstID);
                    }
                }
            }

            if (!matchingPlans.empty()) {
                auto activeBurstIt = std::find_if(
                    _pendingTapSequences.begin(),
                    _pendingTapSequences.end(),
                    [&](const PendingTapSequence& sequence) {
                        return sequence.plan.key.tapKey == keyCode &&
                            sequence.plan.key.isGamepad == isGamepad &&
                            sequence.deadline > now;
                    });

                const uint64_t burstID =
                    activeBurstIt != _pendingTapSequences.end() ?
                    activeBurstIt->burstID : _nextTapBurstID++;
                const auto deadline =
                    activeBurstIt != _pendingTapSequences.end() ?
                    activeBurstIt->deadline :
                    now + std::chrono::duration_cast<Clock::duration>(
                        std::chrono::duration<float>(
                            std::max(burstWindow, 0.01f)));

                for (const TapPlan* plan : matchingPlans) {
                    auto sequenceIt = std::find_if(
                        _pendingTapSequences.begin(),
                        _pendingTapSequences.end(),
                        [&](const PendingTapSequence& sequence) {
                            return sequence.burstID == burstID &&
                                sequence.plan.key == plan->key;
                        });

                    if (sequenceIt == _pendingTapSequences.end()) {
                        PendingTapSequence sequence;
                        sequence.burstID = burstID;
                        sequence.plan = *plan;
                        sequence.deadline = deadline;
                        sequence.bindingGeneration =
                            _bindingGeneration.load(
                                std::memory_order_relaxed);

                        const auto anchorIt =
                            _keyStates.find(plan->key.anchorKey);
                        if (anchorIt != _keyStates.end()) {
                            const auto& anchor = anchorIt->second;
                            if (plan->key.anchorState ==
                                    ActionState::kPress &&
                                anchor.isDown) {
                                sequence.anchorSatisfied = true;
                                sequence.anchorSatisfiedAt =
                                    anchor.lastDownTime;
                            }
                            else if (plan->key.anchorState ==
                                     ActionState::kTap) {
                                for (const auto tapTime :
                                     anchor.tapHistory) {
                                    if (std::chrono::duration<float>(
                                            now - tapTime).count() <=
                                        plan->effectiveWindow) {
                                        sequence.anchorTapTimes.push_back(
                                            tapTime);
                                    }
                                }
                            }
                        }

                        _pendingTapSequences.push_back(
                            std::move(sequence));
                        sequenceIt =
                            std::prev(_pendingTapSequences.end());
                    }
                    TapSample sample;
                    sample.upTime = now;
                    sample.pressDuration = pressDuration;
                    const auto anchorIt =
                        _keyStates.find(plan->key.anchorKey);
                    if (anchorIt != _keyStates.end()) {
                        const auto& anchor = anchorIt->second;
                        sample.anchorDown = anchor.isDown;
                        if (anchor.isDown) {
                            sample.anchorHeldDuration =
                                std::chrono::duration<float>(
                                    now - anchor.lastDownTime).count();
                        }
                        if (plan->key.anchorState ==
                                ActionState::kPress &&
                            anchor.isDown) {
                            sequenceIt->anchorSatisfied = true;
                            sequenceIt->anchorSatisfiedAt =
                                anchor.lastDownTime;
                        }
                        else if (plan->key.anchorState ==
                                 ActionState::kTap) {
                            for (const auto tapTime :
                                 anchor.tapHistory) {
                                if (std::chrono::duration<float>(
                                        now - tapTime).count() <=
                                        plan->effectiveWindow &&
                                    std::find(
                                        sequenceIt->anchorTapTimes.begin(),
                                        sequenceIt->anchorTapTimes.end(),
                                        tapTime) ==
                                        sequenceIt->anchorTapTimes.end()) {
                                    sequenceIt->anchorTapTimes.push_back(
                                        tapTime);
                                }
                            }
                        }
                    }
                    sequenceIt->samples.push_back(sample);
                    sequenceIt->tapPressInProgress = false;
                }

                float tapBeforeHoldWindow = 0.0f;
                for (const auto& sequence : _pendingTapSequences) {
                    if (sequence.burstID == burstID &&
                        sequence.plan.key.anchorState ==
                            ActionState::kHold &&
                        sequence.plan.key.tapBeforeHold &&
                        !sequence.holdAnchorStarted) {
                        tapBeforeHoldWindow = std::max(
                            tapBeforeHoldWindow,
                            sequence.plan.effectiveWindow);
                    }
                }
                if (tapBeforeHoldWindow > 0.0f) {
                    const auto holdStartDeadline = now +
                        std::chrono::duration_cast<Clock::duration>(
                            std::chrono::duration<float>(
                                tapBeforeHoldWindow));
                    for (auto& sequence : _pendingTapSequences) {
                        if (sequence.burstID == burstID) {
                            sequence.deadline = std::max(
                                sequence.deadline,
                                holdStartDeadline);
                        }
                    }
                }

                for (const auto& sequence : _pendingTapSequences) {
                    if (sequence.burstID == burstID &&
                        sequence.plan.key.anchorState ==
                            ActionState::kTap) {
                        for (auto& other : _pendingTapSequences) {
                            if (other.plan.key.isGamepad == isGamepad &&
                                other.plan.key.tapKey ==
                                    sequence.plan.key.anchorKey) {
                                other.deadline = std::max(
                                    other.deadline,
                                    deadline);
                            }
                        }
                    }
                }

                if (std::find(
                        affectedBurstIDs.begin(),
                        affectedBurstIDs.end(),
                        burstID) == affectedBurstIDs.end()) {
                    affectedBurstIDs.push_back(burstID);
                }
            }

            for (const uint64_t burstID : affectedBurstIDs) {
                const auto resolution =
                    FindTapResolutionLocked(burstID, true);
                if (!resolution) {
                    continue;
                }
                RemoveTapBurstLocked(burstID, resolution);
                resolutions.push_back(*resolution);
            }
            ++_tapScheduleRevision;
        }
        _tapCondition.notify_all();

        for (const auto& resolution : resolutions) {
            DispatchTapResolution(resolution, true);
        }
    }

    bool KeyManager::ProcessCoreLogic(RE::InputEvent* a_event, bool singleEvent) {
        bool consumed = false;
        auto now = std::chrono::steady_clock::now();

        // --- LIMPEZA DO BUFFER ---
        while (!_inputHistory.empty() && std::chrono::duration<float>(now - _inputHistory.front().timestamp).count() > 4.0f) {
            _inputHistory.pop_front();
        }

        // --- TIMER DE GRAVAÇÃO ---
        if (_isRecordingMotion && _recordingMotionIndex >= 0) {
            float maxTime = ActionMenuUI::motionList[_recordingMotionIndex].timeWindow;
            if (std::chrono::duration<float>(now - _recordingStartTime).count() > maxTime) {
                _isRecordingMotion = false;
            }
        }
        UpdateMotionTest();

        // Percorre a Linked List de eventos de entrada desse exato frame
        for (auto* e = a_event; e != nullptr; e = singleEvent ? nullptr : e->next) {

            bool newUp = _dirUp, newDown = _dirDown, newLeft = _dirLeft, newRight = _dirRight;
            bool dirChanged = false;
            uint32_t rawKeyID = 0;
            bool isGamepadEvent = (e->GetDevice() == RE::INPUT_DEVICE::kGamepad);
            bool isKeyDown = false;

            // 1. AVALIAÇÃO DE DIREÇÕES E BOTÕES BRUTOS
            if (e->GetEventType() == RE::INPUT_EVENT_TYPE::kThumbstick) {
                auto* stick = static_cast<RE::ThumbstickEvent*>(e);
                if (stick->IsLeft()) {
                    newUp = stick->yValue > 0.5f;
                    newDown = stick->yValue < -0.5f;
                    newLeft = stick->xValue < -0.5f;
                    newRight = stick->xValue > 0.5f;
                }
            }
            else if (e->GetEventType() == RE::INPUT_EVENT_TYPE::kButton) {
                auto* btn = static_cast<RE::ButtonEvent*>(e);
                rawKeyID = GetUnifiedKeyCode(btn);
                const uint32_t originalRawKeyID = rawKeyID;
                isKeyDown = btn->IsDown();

                if ((_isRecordingMotion || _testingMotionIndex >= 0) && _isRecordingGamepad) {
                    bool wasMapped = true;
                    // Se estiver gravando para Gamepad, mapeamos a Tecla do PC digitada para a do Gamepad
                    if (rawKeyID == ActionMenuUI::mappingPad_A && rawKeyID != 0) rawKeyID = RE::BSWin32GamepadDevice::Keys::kA + 266;
                    else if (rawKeyID == ActionMenuUI::mappingPad_B && rawKeyID != 0) rawKeyID = RE::BSWin32GamepadDevice::Keys::kB + 266;
                    else if (rawKeyID == ActionMenuUI::mappingPad_X && rawKeyID != 0) rawKeyID = RE::BSWin32GamepadDevice::Keys::kX + 266;
                    else if (rawKeyID == ActionMenuUI::mappingPad_Y && rawKeyID != 0) rawKeyID = RE::BSWin32GamepadDevice::Keys::kY + 266;
                    else if (rawKeyID == ActionMenuUI::mappingPad_RB && rawKeyID != 0) rawKeyID = RE::BSWin32GamepadDevice::Keys::kRightShoulder + 266;
                    else if (rawKeyID == ActionMenuUI::mappingPad_RT && rawKeyID != 0) rawKeyID = RE::BSWin32GamepadDevice::Keys::kRightTrigger + 266;
                    else if (rawKeyID == ActionMenuUI::mappingPad_LB && rawKeyID != 0) rawKeyID = RE::BSWin32GamepadDevice::Keys::kLeftShoulder + 266;
                    else if (rawKeyID == ActionMenuUI::mappingPad_LT && rawKeyID != 0) rawKeyID = RE::BSWin32GamepadDevice::Keys::kLeftTrigger + 266;
                    else if (rawKeyID == ActionMenuUI::mappingPad_Up && rawKeyID != 0) rawKeyID = RE::BSWin32GamepadDevice::Keys::kUp + 266;
                    else if (rawKeyID == ActionMenuUI::mappingPad_Down && rawKeyID != 0) rawKeyID = RE::BSWin32GamepadDevice::Keys::kDown + 266;
                    else if (rawKeyID == ActionMenuUI::mappingPad_Left && rawKeyID != 0) rawKeyID = RE::BSWin32GamepadDevice::Keys::kLeft + 266;
                    else if (rawKeyID == ActionMenuUI::mappingPad_Right && rawKeyID != 0) rawKeyID = RE::BSWin32GamepadDevice::Keys::kRight + 266;
                    // Permite que as teclas do "PC Movement Keys" gerem inputs direcionais válidos para a gravação do Gamepad!
                    else if (rawKeyID == ActionMenuUI::motionPC_Up && rawKeyID != 0) wasMapped = true;
                    else if (rawKeyID == ActionMenuUI::motionPC_Down && rawKeyID != 0) wasMapped = true;
                    else if (rawKeyID == ActionMenuUI::motionPC_Left && rawKeyID != 0) wasMapped = true;
                    else if (rawKeyID == ActionMenuUI::motionPC_Right && rawKeyID != 0) wasMapped = true;
                    else wasMapped = false;

                    if (wasMapped) {
                        isGamepadEvent = true;
                    }
                }
                // Movimento via User Events
                const bool usesMotionPCMapping =
                    (originalRawKeyID == ActionMenuUI::motionPC_Up && ActionMenuUI::motionPC_Up != 0) ||
                    (originalRawKeyID == ActionMenuUI::motionPC_Down && ActionMenuUI::motionPC_Down != 0) ||
                    (originalRawKeyID == ActionMenuUI::motionPC_Left && ActionMenuUI::motionPC_Left != 0) ||
                    (originalRawKeyID == ActionMenuUI::motionPC_Right && ActionMenuUI::motionPC_Right != 0);

                auto userEvent = btn->GetUserEvent();
                if (!usesMotionPCMapping && userEvent != "") {
                    bool isPressed = btn->IsPressed();
                    if (userEvent == "Forward" || userEvent == "Up") newUp = isPressed;
                    else if (userEvent == "Back" || userEvent == "Down") newDown = isPressed;
                    else if (userEvent == "Strafe Left" || userEvent == "Left") newLeft = isPressed;
                    else if (userEvent == "Strafe Right" || userEvent == "Right") newRight = isPressed;
                }

                if (originalRawKeyID == ActionMenuUI::motionPC_Up && ActionMenuUI::motionPC_Up != 0) newUp = btn->IsDown() || btn->IsHeld();
                else if (originalRawKeyID == ActionMenuUI::motionPC_Down && ActionMenuUI::motionPC_Down != 0) newDown = btn->IsDown() || btn->IsHeld();
                else if (originalRawKeyID == ActionMenuUI::motionPC_Left && ActionMenuUI::motionPC_Left != 0) newLeft = btn->IsDown() || btn->IsHeld();
                else if (originalRawKeyID == ActionMenuUI::motionPC_Right && ActionMenuUI::motionPC_Right != 0) newRight = btn->IsDown() || btn->IsHeld();
                // D-PAD Gamepad
                if (rawKeyID == RE::BSWin32GamepadDevice::Keys::kUp + 266) newUp = btn->IsDown() || btn->IsHeld();
                else if (rawKeyID == RE::BSWin32GamepadDevice::Keys::kDown + 266) newDown = btn->IsDown() || btn->IsHeld();
                else if (rawKeyID == RE::BSWin32GamepadDevice::Keys::kLeft + 266) newLeft = btn->IsDown() || btn->IsHeld();
                else if (rawKeyID == RE::BSWin32GamepadDevice::Keys::kRight + 266) newRight = btn->IsDown() || btn->IsHeld();
            }

            if (newUp != _dirUp || newDown != _dirDown || newLeft != _dirLeft || newRight != _dirRight) {
                _dirUp = newUp; _dirDown = newDown; _dirLeft = newLeft; _dirRight = newRight;
                dirChanged = true;
            }

            // --- REGISTRO NO BUFFER ---
            uint32_t eventToLog = 0;

            if (dirChanged) {
                eventToLog = GetDirectionVKey(_dirUp, _dirDown, _dirLeft, _dirRight);
            }
            else if (isKeyDown && rawKeyID != 0) {
                bool isMovementUserEvent = false;
                if (e->GetEventType() == RE::INPUT_EVENT_TYPE::kButton) {
                    auto userEvent = static_cast<RE::ButtonEvent*>(e)->GetUserEvent();
                    if (userEvent == "Forward" || userEvent == "Back" || userEvent == "Strafe Left" || userEvent == "Strafe Right" ||
                        userEvent == "Up" || userEvent == "Down" || userEvent == "Left" || userEvent == "Right") {
                        isMovementUserEvent = true;
                    }
                }

                if (!isMovementUserEvent &&
                    rawKeyID != (RE::BSWin32GamepadDevice::Keys::kUp + 266) && rawKeyID != (RE::BSWin32GamepadDevice::Keys::kDown + 266) &&
                    rawKeyID != (RE::BSWin32GamepadDevice::Keys::kLeft + 266) && rawKeyID != (RE::BSWin32GamepadDevice::Keys::kRight + 266)) {
                    eventToLog = rawKeyID;
                }
            }

            if (eventToLog != 0) {
                if (_testingMotionIndex >= 0 && (isGamepadEvent == _isRecordingGamepad)) {
                    if (_tempMotionTestSequence.size() < 20) {
                        _tempMotionTestSequence.push_back(eventToLog);
                    }
                    // Validate only the selected motion; gameplay matches may
                    // consume shorter or identical sequences from the history.
                    UpdateMotionTest();
                    continue;
                }
                if (_testingMotionIndex >= 0) {
                    continue;
                }

                _inputHistory.push_back({ eventToLog, now });
                TrimMotionHistory(isGamepadEvent);

                std::string partialPayload = isGamepadEvent ? "pad|" : "pc|";
                const std::size_t startIndex = _inputHistory.size() > 12 ? _inputHistory.size() - 12 : 0;
                for (std::size_t i = startIndex; i < _inputHistory.size(); ++i) {
                    if (i != startIndex) {
                        partialPayload += ",";
                    }
                    partialPayload += std::to_string(_inputHistory[i].keyID);
                }
                InputManagerAPI::SendMotionInputUpdatedEvent(eventToLog, partialPayload);

                if (_isRecordingMotion && (isGamepadEvent == _isRecordingGamepad)) {
                    _tempMotionSequence.push_back(eventToLog);
                }
                else if (!_isRecordingMotion) {
                    CheckMotionMatches(now);
                }
            }

            // -------------------------------------------------------------------
            // EVENTO 1: LEITURA DO RATO (Pincel)
            // -------------------------------------------------------------------
            if (e->GetEventType() == RE::INPUT_EVENT_TYPE::kMouseMove && _isDrawingGesture && _activeGestureStick == -1) {
                auto* mouseEvent = static_cast<RE::MouseMoveEvent*>(e);

                std::lock_guard<std::mutex> lock(_gestureMutex);
                _virtualX += mouseEvent->mouseInputX;
                _virtualY += mouseEvent->mouseInputY;

                if (!_activeGesturePath.empty()) {
                    float dx = _virtualX - _activeGesturePath.back().x;
                    float dy = _virtualY - _activeGesturePath.back().y;
                    if (dx * dx + dy * dy > 4.0f) {
                        _activeGesturePath.push_back({ _virtualX, _virtualY });
                    }
                }
                else {
                    _activeGesturePath.push_back({ _virtualX, _virtualY });
                }
            }

            // -------------------------------------------------------------------
            // EVENTO 2: LEITURA DO THUMBSTICK (Pincel)
            // -------------------------------------------------------------------
            else if (e->GetEventType() == RE::INPUT_EVENT_TYPE::kThumbstick && _isDrawingGesture && _activeGestureStick != -1) {
                auto* stickEvent = static_cast<RE::ThumbstickEvent*>(e);
                bool isLeft = stickEvent->IsLeft();

                if ((_activeGestureStick == 0 && isLeft) || (_activeGestureStick == 1 && !isLeft)) {
                    if (std::abs(stickEvent->xValue) > 0.15f || std::abs(stickEvent->yValue) > 0.15f) {

                        std::lock_guard<std::mutex> lock(_gestureMutex);
                        _virtualX += stickEvent->xValue * 8.0f;
                        _virtualY -= stickEvent->yValue * 8.0f;

                        if (!_activeGesturePath.empty()) {
                            float dx = _virtualX - _activeGesturePath.back().x;
                            float dy = _virtualY - _activeGesturePath.back().y;
                            if (dx * dx + dy * dy > 4.0f) {
                                _activeGesturePath.push_back({ _virtualX, _virtualY });
                            }
                        }
                        else {
                            _activeGesturePath.push_back({ _virtualX, _virtualY });
                        }
                    }
                }
            }

            // -------------------------------------------------------------------
            // EVENTO 3: LEITURA DOS BOTÕES E TECLADO
            // -------------------------------------------------------------------
            if (e->GetEventType() == RE::INPUT_EVENT_TYPE::kButton) {
                auto* buttonEvent = static_cast<RE::ButtonEvent*>(e);
                uint32_t id = GetUnifiedKeyCode(buttonEvent);
                auto& state = _keyStates[id];
                bool isBusy = _isRecordingMotion || (_testingMotionIndex >= 0);
                bool completedRelease = false;
                float releasedPressDuration = 0.0f;
                if (isBusy) {
                    if (buttonEvent->IsDown()) state.isDown = true;
                    else if (buttonEvent->IsUp()) state.isDown = false;
                    continue; // Ignora o processamento de macros/callbacks para este evento de botão
                }
                if (buttonEvent->IsDown()) {
                    if (!state.isDown) {
                        state.lastDownTime = now;
                        state.isPressFired = false;
                    }
                    state.isDown = true;
                    HandleTapKeyDown(
                        id,
                        buttonEvent->GetDevice() ==
                            RE::INPUT_DEVICE::kGamepad,
                        now);

                    for (const auto& binding : _bindings) {
                        if (binding.combo.modifierActionType == ActionState::kGesture && binding.combo.mainKey == id) {
                            if (!_isDrawingGesture) {
                                std::lock_guard<std::mutex> lock(_gestureMutex);
                                _isDrawingGesture = true;
                                _activeGestureBrushKey = id;
                                _activeGestureStick = (buttonEvent->GetDevice() == RE::INPUT_DEVICE::kGamepad) ? binding.combo.gamepadGestureStick : -1;
                                _activeGesturePath.clear();
                                _virtualX = 0.0f;
                                _virtualY = 0.0f;
                                _activeGesturePath.push_back({ 0.0f, 0.0f });
                            }
                            break;
                        }
                    }
                }
                else if (buttonEvent->IsUp()) {
                    if (state.isDown) {
                        releasedPressDuration =
                            std::chrono::duration<float>(
                                now - state.lastDownTime).count();
                        completedRelease = true;
                        for (auto& binding : _bindings) {
                            if (binding.activeHold) {
                                bool mainReleased = (binding.combo.mainKey == id && (binding.combo.mainActionType == ActionState::kHold || binding.combo.mainActionType == ActionState::kPress));
                                bool modReleased = (binding.combo.modifierKey == id && (binding.combo.modifierActionType == ActionState::kHold || binding.combo.modifierActionType == ActionState::kPress));

                                if (mainReleased || modReleased) {
                                    ExecuteReleaseCallback(binding.name);
                                    binding.activeHold = false;
                                }
                            }
                        }
                        state.isDown = false;
                        state.lastUpTime = now;
                        state.isHeldFired = false;
                        state.usedAsModifier = false;
                        state.tapHistory.push_back(now);

                        auto removeIt = std::remove_if(state.tapHistory.begin(), state.tapHistory.end(),
                            [&](const auto& t) { return std::chrono::duration<float>(now - t).count() > 2.0f; });
                        state.tapHistory.erase(removeIt, state.tapHistory.end());

                        if (_isDrawingGesture && _activeGestureBrushKey == id) {

                            std::vector<GestureMath::Point2D> pathToProcess;
                            {
                                std::lock_guard<std::mutex> lock(_gestureMutex);
                                _isDrawingGesture = false;
                                pathToProcess = _activeGesturePath;
                                _activeGesturePath.clear();
                            }

                            if (pathToProcess.size() > 3) {
                                auto candidate = GestureMath::NormalizeGesture(pathToProcess);

                                for (const auto& binding : _bindings) {
                                    if (binding.combo.modifierActionType == ActionState::kGesture && binding.combo.mainKey == id) {
                                        if (binding.combo.gestureIndex >= 0 && binding.combo.gestureIndex < ActionMenuUI::movementList.size()) {

                                            auto& targetGesture = ActionMenuUI::movementList[binding.combo.gestureIndex];
                                            float score = GestureMath::GetMatchScore(targetGesture.normalizedPoints, candidate);

                                            if (score >= targetGesture.requiredAccuracy) {
                                                ExecuteCallback(binding.name);
                                                consumed = true;
                                                state.usedAsModifier = true;
                                                break;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                if (completedRelease) {
                    HandleTapRelease(
                        id,
                        buttonEvent->GetDevice() ==
                            RE::INPUT_DEVICE::kGamepad,
                        releasedPressDuration,
                        now);
                }

                // -------------------------------------------------------------------
                // AVALIAÇÃO DINÂMICA DE TODAS AS AÇÕES REGISTADAS
                // -------------------------------------------------------------------
                for (auto& binding : _bindings) {

                    if (binding.combo.modifierActionType == ActionState::kGesture) continue;
                    if (binding.combo.mainActionType == ActionState::kTap ||
                        binding.combo.modifierActionType == ActionState::kTap) {
                        continue;
                    }

                    if (binding.combo.mainKey == id || binding.combo.modifierKey == id) {

                        bool mainIsAnchor = false;
                        bool modIsAnchor = false;

                        if (binding.combo.modifierKey != 0) {
                            if (binding.combo.modifierActionType == ActionState::kTap) {
                                mainIsAnchor = true;
                                modIsAnchor = false;
                            }
                            else if (binding.combo.mainActionType == ActionState::kTap) {
                                mainIsAnchor = false;
                                modIsAnchor = true;
                            }
                            else {
                                mainIsAnchor = false;
                                modIsAnchor = true;
                            }
                        }

                        if (IsConditionMet(binding.combo.mainKey, binding.combo.mainActionType, binding.combo.mainTapCount, now, binding.combo.tapWindow, binding.combo.holdDuration, mainIsAnchor) &&
                            IsConditionMet(binding.combo.modifierKey, binding.combo.modifierActionType, binding.combo.modTapCount, now, binding.combo.tapWindow, binding.combo.holdDuration, modIsAnchor)) {

                            if (binding.combo.mainActionType == ActionState::kHold) _keyStates[binding.combo.mainKey].isHeldFired = true;
                            if (binding.combo.modifierActionType == ActionState::kHold) _keyStates[binding.combo.modifierKey].isHeldFired = true;
                            if (binding.combo.mainActionType == ActionState::kPress) _keyStates[binding.combo.mainKey].isPressFired = true;
                            if (binding.combo.modifierActionType == ActionState::kPress) _keyStates[binding.combo.modifierKey].isPressFired = true;

                            _keyStates[binding.combo.mainKey].usedAsModifier = true;
                            if (binding.combo.modifierKey != 0) _keyStates[binding.combo.modifierKey].usedAsModifier = true;

                            consumed = true;

                            ExecuteCallback(binding.name);

                            if (binding.combo.mainActionType == ActionState::kHold || binding.combo.modifierActionType == ActionState::kHold ||
                                binding.combo.mainActionType == ActionState::kPress || binding.combo.modifierActionType == ActionState::kPress) {
                                binding.activeHold = true;
                            }
                        }
                    }
                }

                if (buttonEvent->IsUp()) state.usedAsModifier = false;
            }
        }

        return false;
    }

    bool KeyManager::ProcessInput(RE::InputEvent* a_event)
    {
        if (!a_event) return false;

        // Verifica se está gravando/testando Motions ou executando Gestures
        bool forceHook = _isRecordingMotion || (_testingMotionIndex >= 0);

        // Se a UI diz para usar a Sink (useHook == false) E NÃO estamos gravando/testando,
        // o Hook ignora o input e deixa passar (return false). A Sink vai pegar isso depois.
        if (!ActionMenuUI::useHook && !forceHook) {
            return false;
        }

        // InputEventHandler calls us once per node; the Sink supplies the list.
        ProcessCoreLogic(a_event, true);

        if (forceHook) return true;

        return false;
    }


    void KeyManager::ClearStates() {
        //logger::info("[KeyManager] Limpando todos os estados de teclas.");
        CancelPendingTaps();
        _keyStates.clear();
    }

    void KeyManager::ClearBindings() {
        //logger::info("[KeyManager] Limpando todos os bindings e estados registrados.");
        CancelPendingTaps();
        _tapPlans.clear();
        _bindings.clear();
        _keyStates.clear();
    }

    void KeyManager::SortBindings() {
        std::stable_sort(_bindings.begin(), _bindings.end(), [](const KeyBinding& a, const KeyBinding& b) {
            bool aHasMod = (a.combo.modifierKey != 0);
            bool bHasMod = (b.combo.modifierKey != 0);
            return aHasMod > bHasMod; // true (1) vem antes de false (0)
            });
        RebuildTapPlans();
        //logger::info("[KeyManager] Bindings ordenados: Combos tem prioridade sobre Teclas Isoladas.");
    }

    void KeyManager::UpdateModListener(int actionID, const std::string& modName, const std::string& purpose, bool isRegistering, const std::vector<int>& validMain, const std::vector<int>& validMod) {
        if (isRegistering) {
            for (auto& listener : _listeners) {
                if (listener.modName == modName && listener.purpose == purpose) {
                    logger::info("[Input Manager] Atualizado! Mod: '{}' | Proposito: '{}' mudou para Acao ID: {}", modName, purpose, actionID);
                    listener.actionID = actionID;
                    listener.validMainActions = validMain; 
                    listener.validModActions = validMod;   
                    return;
                }
            }
            logger::info("[Input Manager] Novo Mod Registrado! ID da Acao: {} | Mod: '{}' | Utilizado para: '{}'", actionID, modName, purpose);
            _listeners.push_back({ actionID, modName, purpose, validMain, validMod });
        }
        else {
            auto it = std::remove_if(_listeners.begin(), _listeners.end(), [&](const ModListener& l) {
                return l.modName == modName && l.purpose == purpose;
                });

            if (it != _listeners.end()) {
                _listeners.erase(it, _listeners.end());
                logger::info("[Input Manager] Registro Removido! Mod: '{}' deixou de ouvir o proposito: '{}'", modName, purpose);
            }
        }
    }

    void KeyManager::UpdateMotionModListener(int motionID, const std::string& modName, const std::string& purpose, bool isRegistering) {
        if (isRegistering) {
            for (auto& listener : _motionListeners) {
                if (listener.modName == modName && listener.purpose == purpose) {
                    listener.actionID = motionID; // Reaproveitamos o campo actionID da struct para o motionID
                    return;
                }
            }
            _motionListeners.push_back({ motionID, modName, purpose });
        }
        else {
            auto it = std::remove_if(_motionListeners.begin(), _motionListeners.end(), [&](const ModListener& l) {
                return l.modName == modName && l.purpose == purpose;
                });
            if (it != _motionListeners.end()) _motionListeners.erase(it, _motionListeners.end());
        }
    }

    uint32_t KeyManager::GetUnifiedKeyCode(RE::ButtonEvent* a_event) {
        uint32_t keyID = a_event->GetIDCode(); // Pega o ID bruto da Engine
        auto device = a_event->GetDevice();

        if (device == RE::INPUT_DEVICE::kMouse) {
            return keyID + 256; // Mouse: 0 + 256, 1 + 256...
        }

        if (device == RE::INPUT_DEVICE::kGamepad) {
            return keyID + 266; // Gamepad: 1 + 266, 4096 + 266...
        }

        return keyID; // Teclado: 0-255 (Scan Codes brutos)
    }

    void KeyManager::ExecuteCallback(const std::string& name) {
        //logger::info("[KeyManager] Executando callback para a acao: '{}'", name);
        for (auto& binding : _bindings) {
            if (binding.name == name) {
                binding.callback();
                return;
            }
        }
    }

    void KeyManager::ExecuteReleaseCallback(const std::string& name) {
        //logger::info("[KeyManager] Executando RELEASE callback para a acao: '{}'", name);
        for (auto& binding : _bindings) {
            if (binding.name == name) {
                if (binding.releaseCallback) {
                    binding.releaseCallback();
                }
                return;
            }
        }
    }

    // O CÉREBRO DA MÁQUINA DE ESTADOS
    bool KeyManager::IsConditionMet(uint32_t keyCode, ActionState requiredState, int requiredTapCount, std::chrono::steady_clock::time_point now, float tapWindow, float holdDuration, bool isModifier) {
        if (keyCode == 0 || requiredState == ActionState::kIgnored) return true;

        auto& state = _keyStates[keyCode];

        switch (requiredState) {
        case ActionState::kPress:
            if (isModifier) return state.isDown;
            return state.isDown && !state.isPressFired;

        case ActionState::kTap: {
            if (state.isDown) return false;
            if (std::chrono::duration<float>(now - state.lastUpTime).count() > tapWindow) return false;
            if (std::chrono::duration<float>(state.lastUpTime - state.lastDownTime).count() >= holdDuration) return false;
            int validTaps = 0;
            for (auto it = state.tapHistory.rbegin(); it != state.tapHistory.rend(); ++it) {
                if (std::chrono::duration<float>(now - *it).count() <= tapWindow) {
                    validTaps++;
                }
                else {
                    break;
                }
            }
            return (!state.usedAsModifier && validTaps == requiredTapCount);
        }

        case ActionState::kHold:
            if (isModifier) return state.isDown && (std::chrono::duration<float>(now - state.lastDownTime).count() >= holdDuration);

            return state.isDown && !state.isHeldFired &&
                (std::chrono::duration<float>(now - state.lastDownTime).count() >= holdDuration);
        }

        return false;
    }

    uint32_t KeyManager::GetDirectionVKey(bool u, bool d, bool l, bool r) {
        if (u && r) return InputManagerAPI::VKEY_DIR_UPRIGHT;
        if (u && l) return InputManagerAPI::VKEY_DIR_UPLEFT;
        if (d && r) return InputManagerAPI::VKEY_DIR_DOWNRIGHT;
        if (d && l) return InputManagerAPI::VKEY_DIR_DOWNLEFT;
        if (u) return InputManagerAPI::VKEY_DIR_UP;
        if (d) return InputManagerAPI::VKEY_DIR_DOWN;
        if (l) return InputManagerAPI::VKEY_DIR_LEFT;
        if (r) return InputManagerAPI::VKEY_DIR_RIGHT;
        return 0;
    }

    void KeyManager::StartMotionRecording(int motionIndex, bool isGamepad) {
        _isRecordingMotion = true;
        _isRecordingGamepad = isGamepad;
        _recordingMotionIndex = motionIndex;
        _tempMotionSequence.clear();
        _inputHistory.clear(); // Limpa para evitar lixo do menu
        _recordingStartTime = std::chrono::steady_clock::now();
    }

    void KeyManager::StopMotionRecording() {
        _isRecordingMotion = false;
    }



    bool KeyManager::IsMotionPrefix(const std::vector<uint32_t>& candidate, bool isGamepad) const {
        if (candidate.empty()) return false;

        for (const auto& motionEntry : ActionMenuUI::motionList) {
            const auto& requiredSeq = isGamepad ? motionEntry.padSequence : motionEntry.pcSequence;
            if (requiredSeq.empty() || candidate.size() > requiredSeq.size()) {
                continue;
            }

            bool matches = true;
            for (std::size_t i = 0; i < candidate.size(); ++i) {
                if (candidate[i] != requiredSeq[i]) {
                    matches = false;
                    break;
                }
            }

            if (matches) {
                return true;
            }
        }

        return false;
    }

    void KeyManager::TrimMotionHistory(bool isGamepad) {
        if (_inputHistory.empty()) return;

        std::size_t bestStart = _inputHistory.size();
        for (std::size_t start = 0; start < _inputHistory.size(); ++start) {
            std::vector<uint32_t> candidate;
            candidate.reserve(_inputHistory.size() - start);
            for (std::size_t i = start; i < _inputHistory.size(); ++i) {
                candidate.push_back(_inputHistory[i].keyID);
            }

            if (IsMotionPrefix(candidate, isGamepad)) {
                bestStart = start;
                break;
            }
        }

        if (bestStart == 0) {
            return;
        }

        if (bestStart >= _inputHistory.size()) {
            _inputHistory.clear();
            return;
        }

        _inputHistory.erase(_inputHistory.begin(), _inputHistory.begin() + static_cast<std::ptrdiff_t>(bestStart));
    }

    void KeyManager::CheckMotionMatches(std::chrono::steady_clock::time_point now) {
        if (_inputHistory.empty()) return;

        for (size_t m = 0; m < ActionMenuUI::motionList.size(); ++m) {
            const auto& motionEntry = ActionMenuUI::motionList[m];

            for (int pass = 0; pass < 2; ++pass) {
                const auto& requiredSeq = (pass == 0) ? motionEntry.pcSequence : motionEntry.padSequence;

                if (requiredSeq.empty()) continue;
                if (_inputHistory.size() < requiredSeq.size()) continue;

                const auto startIdx = _inputHistory.size() - requiredSeq.size();
                bool matches = true;
                for (std::size_t i = 0; i < requiredSeq.size(); ++i) {
                    if (_inputHistory[startIdx + i].keyID != requiredSeq[i]) {
                        matches = false;
                        break;
                    }
                }
                if (!matches) continue;

                // Calcula o tempo levado entre o PRIMEIRO e o ÚLTIMO botão da sequência
                float timeTaken = std::chrono::duration<float>(_inputHistory.back().timestamp - _inputHistory[startIdx].timestamp).count();

                // Verifica se o jogador executou dentro da janela permitida
                if (timeTaken <= motionEntry.timeWindow) {

                    if (!_isRecordingMotion) {
                        if (ActionMenuUI::showDebugLogs) {
                            std::string msg = "Motion triggered: " + std::string(motionEntry.name);
                            logger::info("[SUCCESS] {}", msg);
                            RE::SendHUDMessage::ShowHUDMessage(msg.c_str());
                        }
                        InputManagerAPI::SendMotionTriggeredEvent(static_cast<int>(m), motionEntry.name);
                    }

                    // Limpa o histórico em todos os casos de sucesso
                    _inputHistory.clear();
                    return;
                }
            }
        }
    }


    void KeyManager::StartMotionTesting(int motionIndex, bool isGamepad) {
        _testingMotionIndex = motionIndex;
        _isRecordingGamepad = isGamepad;
        _motionTestSuccess = false;
        _tempMotionTestSequence.clear();
        _inputHistory.clear();
        _recordingStartTime = std::chrono::steady_clock::now();
    }

    void KeyManager::UpdateMotionTest() {
        if (_testingMotionIndex < 0) return;
        if (static_cast<std::size_t>(_testingMotionIndex) >=
            ActionMenuUI::motionList.size()) {
            _testingMotionIndex = -1;
            return;
        }

        const auto& motion = ActionMenuUI::motionList[_testingMotionIndex];
        const auto& expected = _isRecordingGamepad ?
            motion.padSequence : motion.pcSequence;
        const float elapsed = std::chrono::duration<float>(
            Clock::now() - _recordingStartTime).count();
        const auto result = EvaluateMotionTest(
            _tempMotionTestSequence, expected, elapsed, motion.timeWindow);
        if (result != MotionTestResult::kPending) {
            _motionTestSuccess = result == MotionTestResult::kSuccess;
            _testingMotionIndex = -1;
        }
    }

    void KeyManager::RegisterSink() {
        if (auto inputManager = RE::BSInputDeviceManager::GetSingleton()) {
            inputManager->AddEventSink(this);
            logger::info("[KeyManager] Input sink registrado com sucesso nativamente.");
        }
    }

    RE::BSEventNotifyControl KeyManager::ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>* a_source) {
        if (!a_event || !*a_event) {
            return RE::BSEventNotifyControl::kContinue;
        }

        bool forceHook = _isRecordingMotion || (_testingMotionIndex >= 0);

        // Regra 1 e 2 invertidas: Se o Hook estiver ativo (useHook == true) OU se estivermos
        // forçando o uso do Hook (gravação/teste), a SINK DEVE IGNORAR o evento para não processar duplicado.
        if (ActionMenuUI::useHook || forceHook) {
            return RE::BSEventNotifyControl::kContinue;
        }

        // Processa usando a Sink normalmente (*a_event pega o ponteiro base da lista)
        ProcessCoreLogic(*a_event);

        return RE::BSEventNotifyControl::kContinue;
    }

    void KeyManager::ResetAllInputs() {
        CancelPendingTaps();

        // 1. Dispara o Release Callback para todas as ações que ficaram ativas (Hold/Press)
        for (auto& binding : _bindings) {
            if (binding.activeHold) {
                ExecuteReleaseCallback(binding.name);
                binding.activeHold = false;
            }
        }

        // 2. Reseta o estado de todas as teclas físicas conhecidas
        for (auto& pair : _keyStates) {
            pair.second.isDown = false;
            pair.second.isPressFired = false;
            pair.second.isHeldFired = false;
            pair.second.usedAsModifier = false;
            pair.second.tapHistory.clear();
        }

        // 3. Cancela qualquer Gesture (Pincel) em andamento
        {
            std::lock_guard<std::mutex> lock(_gestureMutex);
            _isDrawingGesture = false;
            _activeGesturePath.clear();
        }

        // 4. Limpa os buffers de Motion e Direcionais
        _inputHistory.clear();
        _tempMotionSequence.clear();
        _dirUp = false; _dirDown = false; _dirLeft = false; _dirRight = false;
    }
}
