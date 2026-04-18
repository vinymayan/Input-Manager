#include "logger.h"
#include "Events.h"
#include "Settings.h"
#include "InputManager.h"
#include "InputEventHandler.h"
#include "OverlayRenderer.h"

extern "C" __declspec(dllexport) void* GetInputManagerAPI() {
    return InputManagerAPI::InputManagerAPI_Impl::GetSingleton();
}

namespace InputManagerAPI {

    InputManagerAPI_Impl* InputManagerAPI_Impl::GetSingleton() {
        static InputManagerAPI_Impl singleton;
        return &singleton;
    }

    ActionInfo InputManagerAPI_Impl::GetActionInfo(int actionID) {
        ActionInfo info{};
        info.id = actionID;
        info.isValid = false;

        if (actionID >= 0 && actionID < ActionMenuUI::actionList.size()) {
            auto& entry = ActionMenuUI::actionList[actionID];
            info.name = entry.name;

            info.pcMainKey = entry.pcMainKey;
            info.pcMainAction = entry.pcMainAction;
            info.pcMainTapCount = entry.pcMainTapCount;

            info.pcModAction = entry.pcModAction;
            info.pcModTapCount = entry.pcModTapCount;

            info.gamepadMainKey = entry.gamepadMainKey;
            info.gamepadMainAction = entry.gamepadMainAction;
            info.gamepadMainTapCount = entry.gamepadMainTapCount;

            info.gamepadModAction = entry.gamepadModAction;
            info.gamepadModTapCount = entry.gamepadModTapCount;

            // Se for gesto, o ModifierKey transporta o index do gesto. Se não, transporta a tecla normal.
            info.pcModifierKey = (entry.pcModAction == 3) ? entry.gestureIndex : entry.pcModifierKey;
            info.gamepadModifierKey = (entry.gamepadModAction == 3) ? entry.gestureIndex : entry.gamepadModifierKey;

            info.gamepadGestureStick = entry.gamepadGestureStick;

            // Timings
            info.useCustomTimings = entry.useCustomTimings;
            info.holdDuration = entry.holdDuration;
            info.tapWindow = entry.tapWindow;

            info.isValid = true;
        }
        return info;
    }

    bool InputManagerAPI_Impl::UpdateActionMapping(int actionID, const ActionInfo& newMapping) {
        if (actionID < 0 || actionID >= ActionMenuUI::actionList.size()) return false;

        if (newMapping.pcModAction == 3 && (newMapping.pcMainAction != 2 && newMapping.pcMainAction != 4)) return false;
        if (newMapping.gamepadModAction == 3 && (newMapping.gamepadMainAction != 2 && newMapping.gamepadMainAction != 4)) return false;

        // Extrair os valores reais baseados na tua lógica
        int newPcGestIndex = (newMapping.pcModAction == 3) ? newMapping.pcModifierKey : -1;
        int newPcModKey = (newMapping.pcModAction == 3) ? 0 : newMapping.pcModifierKey;

        int newPadGestIndex = (newMapping.gamepadModAction == 3) ? newMapping.gamepadModifierKey : -1;
        int newPadModKey = (newMapping.gamepadModAction == 3) ? 0 : newMapping.gamepadModifierKey;

        int finalGestureIndex = (newPcGestIndex != -1) ? newPcGestIndex : newPadGestIndex;

        // Verifica se o nome foi alterado e previne conflitos de nome de arquivo
        if (newMapping.name && strlen(newMapping.name) > 0) {
            std::string newSanitizedName = ActionMenuUI::SanitizeFileName(newMapping.name);
            for (size_t i = 0; i < ActionMenuUI::actionList.size(); ++i) {
                if (i == actionID) continue;
                if (ActionMenuUI::SanitizeFileName(ActionMenuUI::actionList[i].name) == newSanitizedName) {
                    return false; // Falha: geraria um arquivo JSON duplicado com outra action
                }
            }
        }

        auto CheckComboConflict = [](
            int aMKey, int aMAct, int aMTap, int aModKey, int aModAct, int aModTap, int aGest,
            int bMKey, int bMAct, int bMTap, int bModKey, int bModAct, int bModTap, int bGest) {

                if (aMKey == 0 || bMKey == 0) return false;

                // Normaliza Ignore e Gesture
                if (aModAct == 0 || aModAct == 3) aModKey = 0;
                if (bModAct == 0 || bModAct == 3) bModKey = 0;

                // Normaliza Press (4) para Hold (2) para avaliar conflitos justos
                int normAMAct = (aMAct == 4) ? 2 : aMAct;
                int normBMAct = (bMAct == 4) ? 2 : bMAct;
                int normAModAct = (aModAct == 4) ? 2 : aModAct;
                int normBModAct = (bModAct == 4) ? 2 : bModAct;

                // Se houver gestos, tratamos isoladamente
                if (aModAct == 3 || bModAct == 3) {
                    if (aMKey == bMKey && normAMAct == normBMAct && aModAct == bModAct) {
                        return aGest == bGest;
                    }
                    return false;
                }

                // Verifica se usam exatamente as mesmas duas teclas (ordem direta ou invertida)
                bool sameKeysDirect = (aMKey == bMKey && aModKey == bModKey);
                bool sameKeysReversed = (aMKey == bModKey && aModKey == bMKey);

                if (sameKeysDirect || sameKeysReversed) {
                    int aKey1State, aKey1Tap, aKey2State, aKey2Tap;
                    int bKey1State, bKey1Tap, bKey2State, bKey2Tap;

                    if (sameKeysDirect) {
                        aKey1State = normAMAct; aKey1Tap = aMTap;
                        aKey2State = normAModAct; aKey2Tap = aModTap;
                        bKey1State = normBMAct; bKey1Tap = bMTap;
                        bKey2State = normBModAct; bKey2Tap = bModTap;
                    }
                    else {
                        aKey1State = normAMAct; aKey1Tap = aMTap;
                        aKey2State = normAModAct; aKey2Tap = aModTap;
                        bKey1State = normBModAct; bKey1Tap = bModTap;
                        bKey2State = normBMAct; bKey2Tap = bMTap;
                    }

                    // Verifica permutação exata de estados
                    bool matchDirect = (aKey1State == bKey1State && (aKey1State != 1 || aKey1Tap == bKey1Tap)) &&
                        (aKey2State == bKey2State && (aKey2State != 1 || aKey2Tap == bKey2Tap));

                    bool matchCross = (aKey1State == bKey2State && (aKey1State != 1 || aKey1Tap == bKey2Tap)) &&
                        (aKey2State == bKey1State && (aKey2State != 1 || aKey2Tap == bKey1Tap));

                    if (matchDirect || matchCross) {
                        return true;
                    }
                }
                return false;
            };

        for (size_t i = 0; i < ActionMenuUI::actionList.size(); ++i) {
            if (i == actionID) continue;
            const auto& b = ActionMenuUI::actionList[i];

            // Check PC configuration
            if (CheckComboConflict(
                newMapping.pcMainKey, newMapping.pcMainAction, newMapping.pcMainTapCount,
                newPcModKey, newMapping.pcModAction, newMapping.pcModTapCount, newPcGestIndex,
                b.pcMainKey, b.pcMainAction, b.pcMainTapCount,
                b.pcModifierKey, b.pcModAction, b.pcModTapCount, b.gestureIndex)) {
                return false; 
            }

            // Check Gamepad configuration
            if (CheckComboConflict(
                newMapping.gamepadMainKey, newMapping.gamepadMainAction, newMapping.gamepadMainTapCount,
                newPadModKey, newMapping.gamepadModAction, newMapping.gamepadModTapCount, newPadGestIndex,
                b.gamepadMainKey, b.gamepadMainAction, b.gamepadMainTapCount,
                b.gamepadModifierKey, b.gamepadModAction, b.gamepadModTapCount, b.gestureIndex)) {
                return false; 
            }
        }

        auto& entry = ActionMenuUI::actionList[actionID];

        // Atualiza o nome copiando de forma segura
        if (newMapping.name && strlen(newMapping.name) > 0) {
            strncpy_s(entry.name, newMapping.name, sizeof(entry.name) - 1);
            entry.name[sizeof(entry.name) - 1] = '\0'; // Previne overflow
        }

        entry.pcMainKey = newMapping.pcMainKey;
        entry.pcMainAction = newMapping.pcMainAction;
        entry.pcMainTapCount = newMapping.pcMainTapCount;

        entry.pcModifierKey = newPcModKey;
        entry.pcModAction = newMapping.pcModAction;
        entry.pcModTapCount = newMapping.pcModTapCount;

        entry.gamepadMainKey = newMapping.gamepadMainKey;
        entry.gamepadMainAction = newMapping.gamepadMainAction;
        entry.gamepadMainTapCount = newMapping.gamepadMainTapCount;

        entry.gamepadModifierKey = newPadModKey;
        entry.gamepadModAction = newMapping.gamepadModAction;
        entry.gamepadModTapCount = newMapping.gamepadModTapCount;

        entry.gestureIndex = finalGestureIndex;
        entry.gamepadGestureStick = newMapping.gamepadGestureStick;

        entry.useCustomTimings = newMapping.useCustomTimings;
        entry.holdDuration = newMapping.holdDuration;
        entry.tapWindow = newMapping.tapWindow;

        ActionMenuUI::SaveActionsToJson();
        ActionMenuUI::SyncActionsWithEngine();

        return true;
    }

    MotionInfo InputManagerAPI_Impl::GetMotionInfo(int motionID) {
        MotionInfo info{};
        info.isValid = false;
        if (motionID >= 0 && motionID < ActionMenuUI::motionList.size()) {
            auto& m = ActionMenuUI::motionList[motionID];
            info.id = motionID;
            info.name = m.name;
            info.timeWindow = m.timeWindow;

            info.pcSequenceLength = std::min(static_cast<int>(m.pcSequence.size()), 20);
            for (int i = 0; i < info.pcSequenceLength; ++i) info.pcSequence[i] = m.pcSequence[i];

            info.padSequenceLength = std::min(static_cast<int>(m.padSequence.size()), 20);
            for (int i = 0; i < info.padSequenceLength; ++i) info.padSequence[i] = m.padSequence[i];

            info.isValid = true;
        }
        return info;
    }

    bool InputManagerAPI_Impl::UpdateMotionMapping(int motionID, const MotionInfo& newMapping) {
        if (motionID < 0 || motionID >= ActionMenuUI::motionList.size()) return false;

        // VERIFICAÇÃO DE CONFLITOS VIA API (Nomes e Sequências Iguais)
        if (newMapping.name && strlen(newMapping.name) > 0) {
            std::string newSanitized = ActionMenuUI::SanitizeFileName(newMapping.name);
            for (size_t i = 0; i < ActionMenuUI::motionList.size(); ++i) {
                if (i == motionID) continue;

                if (ActionMenuUI::SanitizeFileName(ActionMenuUI::motionList[i].name) == newSanitized) return false;

                bool pcMatch = (newMapping.pcSequenceLength > 0 && newMapping.pcSequenceLength == ActionMenuUI::motionList[i].pcSequence.size());
                if (pcMatch) {
                    for (int j = 0; j < newMapping.pcSequenceLength; ++j) {
                        if (newMapping.pcSequence[j] != ActionMenuUI::motionList[i].pcSequence[j]) { pcMatch = false; break; }
                    }
                }

                bool padMatch = (newMapping.padSequenceLength > 0 && newMapping.padSequenceLength == ActionMenuUI::motionList[i].padSequence.size());
                if (padMatch) {
                    for (int j = 0; j < newMapping.padSequenceLength; ++j) {
                        if (newMapping.padSequence[j] != ActionMenuUI::motionList[i].padSequence[j]) { padMatch = false; break; }
                    }
                }

                if (pcMatch || padMatch) return false; // Bloqueia se a sequência já existir
            }
        }

        auto& m = ActionMenuUI::motionList[motionID];
        if (newMapping.name && strlen(newMapping.name) > 0) {
            strncpy_s(m.name, newMapping.name, sizeof(m.name) - 1);
            m.name[sizeof(m.name) - 1] = '\0';
        }
        m.timeWindow = newMapping.timeWindow;

        m.pcSequence.clear();
        for (int i = 0; i < newMapping.pcSequenceLength; ++i) m.pcSequence.push_back(newMapping.pcSequence[i]);

        m.padSequence.clear();
        for (int i = 0; i < newMapping.padSequenceLength; ++i) m.padSequence.push_back(newMapping.padSequence[i]);

        ActionMenuUI::SaveMotionsToJson();
        return true;
    }

    size_t InputManagerAPI_Impl::GetInputCount(int inputType) {
        if (inputType == 0) return ActionMenuUI::actionList.size();
        if (inputType == 1) return ActionMenuUI::motionList.size();
        if (inputType == 2) return ActionMenuUI::movementList.size();
        return 0;
    }

    const char* InputManagerAPI_Impl::GetInputName(int inputType, int inputID) {
        if (inputType == 0 && inputID >= 0 && inputID < ActionMenuUI::actionList.size()) return ActionMenuUI::actionList[inputID].name;
        if (inputType == 1 && inputID >= 0 && inputID < ActionMenuUI::motionList.size()) return ActionMenuUI::motionList[inputID].name;
        if (inputType == 2 && inputID >= 0 && inputID < ActionMenuUI::movementList.size()) return ActionMenuUI::movementList[inputID].name;
        return "Unknown";
    }

    int InputManagerAPI_Impl::CreateInput(int inputType, const char* inputName) {
        if (!inputName || strlen(inputName) == 0) return -1;
        std::string newSanitized = ActionMenuUI::SanitizeFileName(inputName);

        if (inputType == 0) {
            for (const auto& a : ActionMenuUI::actionList) {
                if (ActionMenuUI::SanitizeFileName(a.name) == newSanitized) return -1;
            }
            ActionMenuUI::ActionEntry newAction;
            strncpy_s(newAction.name, inputName, sizeof(newAction.name) - 1);
            newAction.name[sizeof(newAction.name) - 1] = '\0';
            ActionMenuUI::actionList.push_back(newAction);
            ActionMenuUI::SaveActionsToJson();
            ActionMenuUI::SyncActionsWithEngine();
            return static_cast<int>(ActionMenuUI::actionList.size() - 1);
        }
        else if (inputType == 1) {
            for (const auto& m : ActionMenuUI::motionList) {
                if (ActionMenuUI::SanitizeFileName(m.name) == newSanitized) return -1;
            }
            ActionMenuUI::MotionEntry newMotion;
            strncpy_s(newMotion.name, inputName, sizeof(newMotion.name) - 1);
            newMotion.name[sizeof(newMotion.name) - 1] = '\0';
            ActionMenuUI::motionList.push_back(newMotion);
            ActionMenuUI::SaveMotionsToJson();
            return static_cast<int>(ActionMenuUI::motionList.size() - 1);
        }
        return -1;
    }

    bool InputManagerAPI_Impl::DeleteInput(int inputType, int inputID) {
        // Lógica segura: Impede a deleção se > 1 Mod estiver a ouvir
        auto checkAndDelete = [](int id, auto& list, const auto& listeners, auto saveFunc) {
            if (id < 0 || id >= list.size()) return false;
            std::vector<std::string> uniqueMods;
            for (const auto& l : listeners) {
                if (l.actionID == id && std::find(uniqueMods.begin(), uniqueMods.end(), l.modName) == uniqueMods.end()) {
                    uniqueMods.push_back(l.modName);
                }
            }
            if (uniqueMods.size() <= 1) {
                list.erase(list.begin() + id);
                saveFunc();
                return true;
            }
            return false;
            };

        if (inputType == 0) {
            if (checkAndDelete(inputID, ActionMenuUI::actionList, PluginLogic::KeyManager::GetSingleton()->GetListeners(),
                []() { ActionMenuUI::SaveActionsToJson(); ActionMenuUI::SyncActionsWithEngine(); })) return true;
        }
        else if (inputType == 1) {
            if (checkAndDelete(inputID, ActionMenuUI::motionList, PluginLogic::KeyManager::GetSingleton()->GetMotionListeners(),
                []() { ActionMenuUI::SaveMotionsToJson(); })) return true;
        }
        return false;
    }

    void InputManagerAPI_Impl::UpdateListener(int inputType, int inputID, const char* modName, const char* purpose, bool isRegistering, const int* validMainActions, int mainCount, const int* validModActions, int modCount) {
        auto sanitizeArray = [](const int* arr, int count, std::vector<int>& out) {
            if (!arr || count <= 0) return;
            int zeroCount = 0;
            for (int i = 0; i < count; ++i) {
                if (arr[i] == 0) zeroCount++;
            }
            if (zeroCount <= 1) { 
                out.assign(arr, arr + count);
            }
            };
        std::vector<int> vMain;
        if (validMainActions && mainCount > 0) vMain.assign(validMainActions, validMainActions + mainCount);

        std::vector<int> vMod;
        if (validModActions && modCount > 0) vMod.assign(validModActions, validModActions + modCount);

        if (inputType == 0) PluginLogic::KeyManager::GetSingleton()->UpdateModListener(inputID, modName, purpose, isRegistering, vMain, vMod);
        else if (inputType == 1) PluginLogic::KeyManager::GetSingleton()->UpdateMotionModListener(inputID, modName, purpose, isRegistering);
    }

    size_t InputManagerAPI_Impl::GetListenerCount(int inputType, int inputID) {
        size_t count = 0;
        if (inputType == 0) {
            for (const auto& l : PluginLogic::KeyManager::GetSingleton()->GetListeners()) if (l.actionID == inputID) count++;
        }
        else if (inputType == 1) {
            for (const auto& l : PluginLogic::KeyManager::GetSingleton()->GetMotionListeners()) if (l.actionID == inputID) count++;
        }
        // inputType == 2 (Gestos) não têm listeners diretos, retorna 0 com segurança
        return count;
    }

    const char* InputManagerAPI_Impl::GetListenerModName(int inputType, int inputID, size_t index) {
        size_t current = 0;
        if (inputType == 0) {
            for (const auto& l : PluginLogic::KeyManager::GetSingleton()->GetListeners()) {
                if (l.actionID == inputID) { if (current == index) return l.modName.c_str(); current++; }
            }
        }
        else if (inputType == 1) {
            for (const auto& l : PluginLogic::KeyManager::GetSingleton()->GetMotionListeners()) {
                if (l.actionID == inputID) { if (current == index) return l.modName.c_str(); current++; }
            }
        }
        return "";
    }

    // Wrappers Papyrus

    int GetInputCount_Papyrus(RE::StaticFunctionTag*, int inputType) {
        return static_cast<int>(InputManagerAPI_Impl::GetSingleton()->GetInputCount(inputType));
    }
    RE::BSFixedString GetInputName_Papyrus(RE::StaticFunctionTag*, int inputType, int inputID) {
        return RE::BSFixedString(InputManagerAPI_Impl::GetSingleton()->GetInputName(inputType, inputID));
    }
    int CreateInput_Papyrus(RE::StaticFunctionTag*, int inputType, RE::BSFixedString inputName) {
        return InputManagerAPI_Impl::GetSingleton()->CreateInput(inputType, inputName.c_str());
    }
    bool DeleteInput_Papyrus(RE::StaticFunctionTag*, int inputType, int inputID) {
        return InputManagerAPI_Impl::GetSingleton()->DeleteInput(inputType, inputID);
    }
    void UpdateListener_Papyrus(RE::StaticFunctionTag*, int inputType, int inputID, RE::BSFixedString modName, RE::BSFixedString purpose, bool isRegistering, std::vector<int> validMainActions, std::vector<int> validModActions) {
        InputManagerAPI_Impl::GetSingleton()->UpdateListener(
            inputType, inputID, modName.c_str(), purpose.c_str(), isRegistering,
            validMainActions.data(), static_cast<int>(validMainActions.size()),
            validModActions.data(), static_cast<int>(validModActions.size())
        );
    }
    std::vector<RE::BSFixedString> GetListeners_Papyrus(RE::StaticFunctionTag*, int inputType, int inputID) {
        std::vector<RE::BSFixedString> result;
        if (inputType == 0) {
            for (const auto& l : PluginLogic::KeyManager::GetSingleton()->GetListeners()) {
                if (l.actionID == inputID) result.push_back(RE::BSFixedString(l.modName));
            }
        }
        else if (inputType == 1) {
            for (const auto& l : PluginLogic::KeyManager::GetSingleton()->GetMotionListeners()) {
                if (l.actionID == inputID) result.push_back(RE::BSFixedString(l.modName));
            }
        }
        return result;
    }

    std::vector<int> GetMotionSequence_Papyrus(RE::StaticFunctionTag*, int motionID, bool isGamepad) {
        std::vector<int> data;
        auto info = InputManagerAPI_Impl::GetSingleton()->GetMotionInfo(motionID);
        if (info.isValid) {
            if (isGamepad) {
                for (int i = 0; i < info.padSequenceLength; ++i) data.push_back(info.padSequence[i]);
            }
            else {
                for (int i = 0; i < info.pcSequenceLength; ++i) data.push_back(info.pcSequence[i]);
            }
        }
        return data;
    }

    float GetMotionTimeWindow_Papyrus(RE::StaticFunctionTag*, int motionID) {
        auto info = InputManagerAPI_Impl::GetSingleton()->GetMotionInfo(motionID);
        return info.isValid ? info.timeWindow : 0.0f;
    }

    bool UpdateMotionMapping_Papyrus(RE::StaticFunctionTag*, int motionID, std::vector<int> pcSeq, std::vector<int> padSeq, float timeWindow, RE::BSFixedString newName) {
        auto info = InputManagerAPI_Impl::GetSingleton()->GetMotionInfo(motionID);
        if (!info.isValid) return false;

        if (!newName.empty()) info.name = newName.c_str();
        info.timeWindow = timeWindow;

        info.pcSequenceLength = std::min(static_cast<int>(pcSeq.size()), 20);
        for (int i = 0; i < info.pcSequenceLength; ++i) info.pcSequence[i] = pcSeq[i];

        info.padSequenceLength = std::min(static_cast<int>(padSeq.size()), 20);
        for (int i = 0; i < info.padSequenceLength; ++i) info.padSequence[i] = padSeq[i];

        return InputManagerAPI_Impl::GetSingleton()->UpdateMotionMapping(motionID, info);
    }

    RE::BSFixedString GetActionName_Papyrus(RE::StaticFunctionTag*, int actionID) {
        auto info = InputManagerAPI_Impl::GetSingleton()->GetActionInfo(actionID);
        return info.isValid ? RE::BSFixedString(info.name) : RE::BSFixedString("");
    }

    std::vector<int> GetActionInfo_Papyrus(RE::StaticFunctionTag*, int actionID) {
        std::vector<int> data;
        auto info = InputManagerAPI_Impl::GetSingleton()->GetActionInfo(actionID);

        if (info.isValid) {
            data.push_back(info.pcMainKey);           // Index 0
            data.push_back(info.pcMainAction);        // Index 1
            data.push_back(info.pcMainTapCount);      // Index 2

            data.push_back(info.pcModifierKey);       // Index 3
            data.push_back(info.pcModAction);         // Index 4
            data.push_back(info.pcModTapCount);       // Index 5

            data.push_back(info.gamepadMainKey);      // Index 6
            data.push_back(info.gamepadMainAction);   // Index 7
            data.push_back(info.gamepadMainTapCount); // Index 8

            data.push_back(info.gamepadModifierKey);  // Index 9
            data.push_back(info.gamepadModAction);    // Index 10
            data.push_back(info.gamepadModTapCount);  // Index 11

            data.push_back(info.gamepadGestureStick); // Index 12
        }
        return data;
    }

    bool UpdateActionMapping_Papyrus(RE::StaticFunctionTag*, int actionID,
        int pcMainKey, int pcMainAction, int pcMainTap, int pcModifierKey, int pcModAction, int pcModTap,
        int gamepadMainKey, int gamepadMainAction, int padMainTap, int gamepadModifierKey, int gamepadModAction, int padModTap,
        int gamepadGestureStick, RE::BSFixedString newName)
    {
        ActionInfo info = InputManagerAPI_Impl::GetSingleton()->GetActionInfo(actionID);
        if (!info.isValid) return false;

        if (!newName.empty()) info.name = newName.c_str();

        info.pcMainKey = pcMainKey;
        info.pcMainAction = pcMainAction;
        info.pcMainTapCount = pcMainTap;

        info.pcModifierKey = pcModifierKey;
        info.pcModAction = pcModAction;
        info.pcModTapCount = pcModTap;

        info.gamepadMainKey = gamepadMainKey;
        info.gamepadMainAction = gamepadMainAction;
        info.gamepadMainTapCount = padMainTap;

        info.gamepadModifierKey = gamepadModifierKey;
        info.gamepadModAction = gamepadModAction;
        info.gamepadModTapCount = padModTap;

        info.gamepadGestureStick = gamepadGestureStick;

        return InputManagerAPI_Impl::GetSingleton()->UpdateActionMapping(actionID, info);
    }

    std::vector<float> GetActionTimings_Papyrus(RE::StaticFunctionTag*, int actionID) {
        std::vector<float> data;
        auto info = InputManagerAPI_Impl::GetSingleton()->GetActionInfo(actionID);
        if (info.isValid) {
            data.push_back(info.useCustomTimings ? 1.0f : 0.0f); // Index 0 (Boolean)
            data.push_back(info.tapWindow);                      // Index 1
            data.push_back(info.holdDuration);                   // Index 2
        }
        return data;
    }

    bool UpdateActionTimings_Papyrus(RE::StaticFunctionTag*, int actionID, bool useCustom, float tapWindow, float holdDuration) {
        ActionInfo info = InputManagerAPI_Impl::GetSingleton()->GetActionInfo(actionID);
        if (!info.isValid) return false;

        info.useCustomTimings = useCustom;
        info.tapWindow = tapWindow;
        info.holdDuration = holdDuration;

        return InputManagerAPI_Impl::GetSingleton()->UpdateActionMapping(actionID, info);
    }

    bool RegisterPapyrusFunctions(RE::BSScript::IVirtualMachine* vm) {
        // Funções Unificadas
        vm->RegisterFunction("GetInputCount", "InputManager", GetInputCount_Papyrus);
        vm->RegisterFunction("GetInputName", "InputManager", GetInputName_Papyrus);
        vm->RegisterFunction("CreateInput", "InputManager", CreateInput_Papyrus);
        vm->RegisterFunction("DeleteInput", "InputManager", DeleteInput_Papyrus);
        vm->RegisterFunction("UpdateListener", "InputManager", UpdateListener_Papyrus);
        vm->RegisterFunction("GetListeners", "InputManager", GetListeners_Papyrus);

        // Funções Específicas Action
        vm->RegisterFunction("GetActionKeyData", "InputManager", GetActionInfo_Papyrus);
        vm->RegisterFunction("UpdateActionMapping", "InputManager", UpdateActionMapping_Papyrus);
        vm->RegisterFunction("GetActionTimings", "InputManager", GetActionTimings_Papyrus);
        vm->RegisterFunction("UpdateActionTimings", "InputManager", UpdateActionTimings_Papyrus);

        // Funções Específicas Motion
        vm->RegisterFunction("GetMotionSequence", "InputManager", GetMotionSequence_Papyrus);
        vm->RegisterFunction("GetMotionTimeWindow", "InputManager", GetMotionTimeWindow_Papyrus);
        vm->RegisterFunction("UpdateMotionMapping", "InputManager", UpdateMotionMapping_Papyrus);

        logger::info("[InputManager API] Papyrus APIs registradas com sucesso.");
        return true;
    }
}

struct ProcessInputQueueHook {
    static void thunk(RE::BSTEventSource<RE::InputEvent*>* a_dispatcher, RE::InputEvent* const* a_event) {
        a_event = InputEventHandler::Process(const_cast<RE::InputEvent**>(a_event));
        originalFunction(a_dispatcher, a_event);
    }
    static inline REL::Relocation<decltype(thunk)> originalFunction;

    static void install() {
        auto& trampoline = SKSE::GetTrampoline();
        // Hook na BSInputDeviceManager para capturar os inputs antes do jogo processar
        originalFunction = trampoline.write_call<5>(REL::RelocationID(67315, 68617, 67315).address() + REL::Relocate(0x7B, 0x7B, 0x81), thunk);
    }
};

// ==============================================================================
// 2. FUNÇÃO DE CALLBACK PARA O INPUT EVENT HANDLER
// ==============================================================================
// Criamos uma função estática com __stdcall para bater perfeitamente com o 
// typedef exigido no seu InputEventHandler.h e evitar erros de compilação.
bool __stdcall OnManagerInputReceived(RE::InputEvent* event) {
    return PluginLogic::KeyManager::GetSingleton()->ProcessInput(event);
}



void OnMessage(SKSE::MessagingInterface::Message* message) {
    if (message->type == SKSE::MessagingInterface::kDataLoaded) {
        ActionMenuUI::Register();
        SKSE::AllocTrampoline(14);
        ProcessInputQueueHook::install();
        InputEventHandler::Register(OnManagerInputReceived);
        OverlayRenderer::GetSingleton()->Install();
        PluginLogic::GetMovementKeys();
    }
    else if (message->type == InputManagerAPI::kMessage_RequestAPI) {
        auto api = InputManagerAPI::InputManagerAPI_Impl::GetSingleton();
        SKSE::GetMessagingInterface()->Dispatch(
            InputManagerAPI::kMessage_ProvideAPI,
            api,
            sizeof(api),
            message->sender
        );
        logger::info("[InputManager API] Interface fornecida para o mod: {}", message->sender);
    }
    else if (message->type == SKSE::MessagingInterface::kNewGame || message->type == SKSE::MessagingInterface::kPostLoadGame) {
    }
}

SKSEPluginLoad(const SKSE::LoadInterface *skse) {

    SetupLog();
    logger::info("Plugin loaded");
    SKSE::Init(skse);
    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
    SKSE::GetPapyrusInterface()->Register(InputManagerAPI::RegisterPapyrusFunctions);
    return true;
}
