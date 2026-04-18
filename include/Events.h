#pragma once
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <mutex>
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
        bool pcDelayTap = false;
        bool needsDelay = false;
        uint32_t modifierKey;             // Segunda Tecla (Parceira)
        ActionState modifierActionType;   // Estado exigido para a 2ª Tecla
        int modTapCount = 1;              // Quantidade de taps exigidos para o modificador
        bool gamepadDelayTap = false;
        bool useCustomTimings = false;      // Checkbox: Esperar pelo Double Tap?
        float holdDuration = 0.5f;        // Janela para considerar "Hold"
        float tapWindow = 0.35f;     // Janela para o 2º clique ou para o cruzamento de Taps
        int gestureIndex = -1;
        int gamepadGestureStick = 0;
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

    class KeyManager {
    public:
        static KeyManager* GetSingleton() {
            static KeyManager singleton;
            return &singleton;
        }

        void RegisterAction(const std::string& name, ComboKey combo, std::function<void()> callback, std::function<void()> releaseCallback = nullptr);
        bool ProcessInput(RE::InputEvent* a_event);
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

        void StartMotionTesting(int motionIndex);
        bool GetMotionTestSuccess() const { return _motionTestSuccess; }
        void ResetMotionTest() { _motionTestSuccess = false; _testingMotionIndex = -1; }
        bool IsTestingMotion() const { return _testingMotionIndex != -1; }

    private:
        KeyManager() = default;
        ~KeyManager() = default;
        KeyManager(const KeyManager&) = delete;
        KeyManager& operator=(const KeyManager&) = delete;

        uint32_t GetUnifiedKeyCode(RE::ButtonEvent* a_event);

        bool IsConditionMet(uint32_t keyCode, ActionState requiredState, int requiredTapCount, std::chrono::steady_clock::time_point now, float tapWindow, float holdDuration, bool isModifier = false);

        void ExecuteCallback(const std::string& name);
        void ExecuteReleaseCallback(const std::string& name);
        uint32_t GetDirectionVKey(bool u, bool d, bool l, bool r);
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

        std::unordered_map<uint32_t, KeyState> _keyStates;
        std::vector<KeyBinding> _bindings;
        std::vector<ModListener> _listeners;
        std::vector<ModListener> _motionListeners;

        mutable std::mutex _gestureMutex; // Mutex para Thread-Safety
        bool _isDrawingGesture = false;
        uint32_t _activeGestureBrushKey = 0;
        int _activeGestureStick = 0;
        std::vector<GestureMath::Point2D> _activeGesturePath;
        float _virtualX = 0.0f;
        float _virtualY = 0.0f;
    };

    static uint32_t DefaultkeyUp = RE::BSKeyboardDevice::Keys::kW;
    static uint32_t DefaultkeyDown = RE::BSKeyboardDevice::Keys::kS;
    static uint32_t DefaultkeyLeft = RE::BSKeyboardDevice::Keys::kA;
    static uint32_t DefaultkeyRight = RE::BSKeyboardDevice::Keys::kD;
    void GetMovementKeys();
}

