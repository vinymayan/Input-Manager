#pragma once
#include "Events.h"
#include "InputManagerAPI.h"
namespace InputManagerAPI {

    constexpr uint32_t kMessage_UpdateListener = 8080;
    // Apenas declaramos a classe e funções aqui (a lógica vai para o .cpp)
    class InputManagerAPI_Impl : public IInputManager {
    public:
        static InputManagerAPI_Impl* GetSingleton();

        size_t GetInputCount(int inputType) override;
        const char* GetInputName(int inputType, int inputID) override;
        int CreateInput(int inputType, const char* inputName) override;
        bool DeleteInput(int inputType, int inputID) override;
        void UpdateListener(int inputType, int inputID, const char* modName, const char* purpose, bool isRegistering, const int* validMainActions = nullptr, int mainCount = 0, const int* validModActions = nullptr, int modCount = 0) override;
        size_t GetListenerCount(int inputType, int inputID) override;
        const char* GetListenerModName(int inputType, int inputID, size_t index) override;

        ActionInfo GetActionInfo(int actionID) override;
        bool UpdateActionMapping(int actionID, const ActionInfo& newMapping) override;
        MotionInfo GetMotionInfo(int motionID) override;
        bool UpdateMotionMapping(int motionID, const MotionInfo& newMapping) override;
    };

    // Este não precisa do Settings.h, então pode ficar aqui.
    inline void SendActionTriggeredEvent(int actionID, const std::string& actionName) {
        auto dispatcher = SKSE::GetModCallbackEventSource();
        if (dispatcher) {
            SKSE::ModCallbackEvent modEvent{
                RE::BSFixedString("InputManager_ActionTriggered"),
                RE::BSFixedString(actionName),
                static_cast<float>(actionID),
                nullptr
            };
            dispatcher->SendEvent(&modEvent);
        }
    }

    inline void SendActionReleasedEvent(int actionID, const std::string& actionName) {
        auto dispatcher = SKSE::GetModCallbackEventSource();
        if (dispatcher) {
            SKSE::ModCallbackEvent modEvent{
                RE::BSFixedString("InputManager_ActionReleased"), 
                RE::BSFixedString(actionName),
                static_cast<float>(actionID),
                nullptr
            };
            dispatcher->SendEvent(&modEvent);
        }
    }

    inline void SendMotionTriggeredEvent(int motionID, const std::string& motionName) {
        auto dispatcher = SKSE::GetModCallbackEventSource();
        if (dispatcher) {
            SKSE::ModCallbackEvent modEvent{
                RE::BSFixedString("InputManager_MotionTriggered"),
                RE::BSFixedString(motionName),
                static_cast<float>(motionID),
                nullptr
            };
            dispatcher->SendEvent(&modEvent);
        }
    }

    // Apenas declaração para o Papyrus
    bool RegisterPapyrusFunctions(RE::BSScript::IVirtualMachine* vm);
}