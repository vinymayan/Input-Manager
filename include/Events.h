#pragma once
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <condition_variable>
#include <atomic>
#include <mutex>
#include <optional>
#include <thread>
#include "Manager.h"

namespace PluginLogic {


    enum class ActionState {
        kIgnored = 0,   // Usado se a tecla parceira não for necessária
        kTap,
        kHold,
        kGesture,
        kPress
    };

    struct ComboKey {
        uint32_t mainKey;                 // Primeira Tecla (Gatilho)
        ActionState mainActionType;       // Estado exigido para a 1ª Tecla
        int mainTapCount = 1;             // Quantidade de taps exigidos para a tecla principal
        uint32_t modifierKey;             // Segunda Tecla (Parceira)
        ActionState modifierActionType;   // Estado exigido para a 2ª Tecla
        int modTapCount = 1;              // Quantidade de taps exigidos para o modificador
        bool tapBeforeHold = false;       // Tap -> Hold quando a combinação contém ambos
        bool useCustomTimings = false;
        float holdDuration = 0.5f;        // Janela para considerar "Hold"
        float tapWindow = 0.35f;           // Janela da sequência de taps
        int gestureIndex = -1;
        int gamepadGestureStick = 0;
        bool isGamepad = false;
    };

    struct KeyBinding {
        std::string name;
        ComboKey combo;
        std::function<void()> callback;
        std::function<void()> releaseCallback;
        bool activeHold = false;
    };

    struct KeyState {
        bool isDown = false;
        bool isHeldFired = false;
        bool isPressFired = false;
        bool usedAsModifier = false;      // Evita as Ações Fantasma
        std::chrono::steady_clock::time_point lastDownTime;
        std::chrono::steady_clock::time_point lastUpTime;
        std::vector<std::chrono::steady_clock::time_point> tapHistory;
        int tapCount = 0;
    };

    struct ModListener {
        int actionID;
        std::string modName;
        std::string purpose;
        std::vector<int> validMainActions; 
        std::vector<int> validModActions; 
    };

    struct InputHistoryRecord {
        uint32_t keyID;
        std::chrono::steady_clock::time_point timestamp;
    };

    class KeyManager : public RE::BSTEventSink<RE::InputEvent*> {
    public:
        static KeyManager* GetSingleton() {
            static KeyManager singleton;
            return &singleton;
        }

        void RegisterSink();
        void RegisterAction(const std::string& name, ComboKey combo, std::function<void()> callback, std::function<void()> releaseCallback = nullptr);
        bool ProcessCoreLogic(RE::InputEvent* a_event);
        bool ProcessInput(RE::InputEvent* a_event);
        RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>* a_source) override;

        void ResetAllInputs();
        void ClearStates();
        void ClearBindings();
        void SortBindings();
        void UpdateModListener(int actionID, const std::string& modName, const std::string& purpose, bool isRegistering, const std::vector<int>& validMain = {}, const std::vector<int>& validMod = {});
        const std::vector<ModListener>& GetListeners() const { return _listeners; }
        bool IsDrawingGesture() const { return _isDrawingGesture; }
        void UpdateMotionModListener(int motionID, const std::string& modName, const std::string& purpose, bool isRegistering);
        const std::vector<ModListener>& GetMotionListeners() const { return _motionListeners; }

        std::vector<GestureMath::Point2D> GetActiveGesturePathCopy() const {
            std::lock_guard<std::mutex> lock(_gestureMutex);
            return _activeGesturePath;
        }

        void StartMotionRecording(int motionIndex, bool isGamepad);
        void StopMotionRecording();
        bool IsRecordingMotion() const { return _isRecordingMotion; }
        std::vector<uint32_t> GetRecordedMotion() const { return _tempMotionSequence; }

        void StartMotionTesting(int motionIndex, bool isGamepad);
        bool GetMotionTestSuccess() const { return _motionTestSuccess; }
        std::vector<uint32_t> GetMotionTestInputs() const { return _tempMotionTestSequence; }
        void ResetMotionTest() { _motionTestSuccess = false; _testingMotionIndex = -1; _tempMotionTestSequence.clear(); }
        bool IsTestingMotion() const { return _testingMotionIndex != -1; }

    private:
        using Clock = std::chrono::steady_clock;
        using TimePoint = Clock::time_point;

        struct TapGroupKey {
            uint32_t tapKey = 0;
            uint32_t anchorKey = 0;
            ActionState anchorState = ActionState::kIgnored;
            int anchorTapCount = 1;
            bool tapBeforeHold = false;
            bool isGamepad = false;

            bool operator==(const TapGroupKey&) const = default;
        };

        struct TapCandidate {
            std::string bindingName;
            int requiredTaps = 1;
            float tapWindow = 0.35f;
            float holdDuration = 0.5f;
        };

        struct TapPlan {
            TapGroupKey key;
            std::vector<TapCandidate> candidates;
            int maximumTapCount = 0;
            float effectiveWindow = 0.0f;
        };

        struct TapSample {
            TimePoint upTime;
            float pressDuration = 0.0f;
            bool anchorDown = false;
            float anchorHeldDuration = 0.0f;
        };

        struct PendingTapSequence {
            uint64_t burstID = 0;
            TapPlan plan;
            TimePoint deadline;
            std::vector<TapSample> samples;
            bool anchorSatisfied = false;
            TimePoint anchorSatisfiedAt;
            std::vector<TimePoint> anchorTapTimes;
            bool holdAnchorStarted = false;
            TimePoint holdAnchorDownAt;
            bool tapPressInProgress = false;
            uint64_t bindingGeneration = 0;
        };

        struct TapResolution {
            std::string bindingName;
            uint64_t bindingGeneration = 0;
            uint64_t burstID = 0;
            uint32_t tapKey = 0;
            uint32_t anchorKey = 0;
            ActionState anchorState = ActionState::kIgnored;
            bool isGamepad = false;
        };

        KeyManager();
        ~KeyManager();
        KeyManager(const KeyManager&) = delete;
        KeyManager& operator=(const KeyManager&) = delete;

        uint32_t GetUnifiedKeyCode(RE::ButtonEvent* a_event);

        bool IsConditionMet(uint32_t keyCode, ActionState requiredState, int requiredTapCount, std::chrono::steady_clock::time_point now, float tapWindow, float holdDuration, bool isModifier = false);
        void RebuildTapPlans();
        void HandleTapKeyDown(uint32_t keyCode, bool isGamepad, TimePoint now);
        void HandleTapRelease(uint32_t keyCode, bool isGamepad, float pressDuration, TimePoint now);
        bool IsTapCandidateValid(const PendingTapSequence& sequence, const TapCandidate& candidate) const;
        std::optional<TapResolution> FindTapResolutionLocked(uint64_t burstID, bool immediateOnly) const;
        void RemoveTapBurstLocked(uint64_t burstID, const std::optional<TapResolution>& resolution);
        void DispatchTapResolution(const TapResolution& resolution, bool alreadyOnGameThread);
        void CommitTapResolution(const std::string& bindingName, uint64_t bindingGeneration);
        void TapSchedulerLoop(std::stop_token stopToken);
        void CancelPendingTaps();

        void ExecuteCallback(const std::string& name);
        void ExecuteReleaseCallback(const std::string& name);
        uint32_t GetDirectionVKey(bool u, bool d, bool l, bool r);
        bool IsMotionPrefix(const std::vector<uint32_t>& candidate, bool isGamepad) const;
        void TrimMotionHistory(bool isGamepad);
        void CheckMotionMatches(std::chrono::steady_clock::time_point now);

        std::deque<InputHistoryRecord> _inputHistory;
        bool _dirUp = false, _dirDown = false, _dirLeft = false, _dirRight = false;

        bool _isRecordingMotion = false;
        bool _isRecordingGamepad = false;
        int _recordingMotionIndex = -1;
        std::vector<uint32_t> _tempMotionSequence;
        std::chrono::steady_clock::time_point _recordingStartTime;

        // --- ADIÇÃO: VARIÁVEIS DE TESTE ---
        int _testingMotionIndex = -1;
        bool _motionTestSuccess = false;
        std::vector<uint32_t> _tempMotionTestSequence;

        std::unordered_map<uint32_t, KeyState> _keyStates;
        std::vector<KeyBinding> _bindings;
        std::vector<ModListener> _listeners;
        std::vector<ModListener> _motionListeners;

        std::vector<TapPlan> _tapPlans;
        std::mutex _tapMutex;
        std::condition_variable _tapCondition;
        std::vector<PendingTapSequence> _pendingTapSequences;
        uint64_t _nextTapBurstID = 1;
        uint64_t _tapScheduleRevision = 0;
        std::atomic<uint64_t> _bindingGeneration{ 1 };
        std::jthread _tapScheduler;

        mutable std::mutex _gestureMutex; // Mutex para Thread-Safety
        bool _isDrawingGesture = false;
        uint32_t _activeGestureBrushKey = 0;
        int _activeGestureStick = 0;
        std::vector<GestureMath::Point2D> _activeGesturePath;
        float _virtualX = 0.0f;
        float _virtualY = 0.0f;
    };

}

