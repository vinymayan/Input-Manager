#pragma once
#include "SKSEMCP/SKSEMenuFramework.hpp"
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <iterator> 
#include <map>    
#include <cctype> 

// RapidJSON headers
#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/FileWriteStream.h>
#include <rapidjson/FileReadStream.h>

#include "InputManager.h"
#include "Manager.h"

namespace ActionMenuUI {
    namespace fs = std::filesystem;

    // =========================================================================
    // FOLDER STRUCTURE
    // =========================================================================
    const std::string BASE_DIR = "Data/SKSE/Plugins/Input Manager/";
    const std::string SETTINGS_PATH = BASE_DIR + "Settings.json";
    const std::string INPUTS_DIR = BASE_DIR + "Inputs/";
    const std::string GESTURES_DIR = BASE_DIR + "Gestures/";
    const std::string MOTIONS_DIR = BASE_DIR + "Motion Inputs/";
    const std::string CACHE_PATH = BASE_DIR + "IDCache.json";
    const std::string LANG_PATH = BASE_DIR + "Language.json";

    inline std::unordered_map<std::string, std::string> LangMap;

    inline void LoadLanguage() {
        LangMap.clear();
        std::ifstream file(LANG_PATH, std::ios::binary);
        if (!file.is_open()) {
            logger::warn("[Input Manager] Language.json nao encontrado. Usando textos padroes.");
            return;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string jsonStr = buffer.str();
        file.close();

        if (jsonStr.size() >= 3 && (unsigned char)jsonStr[0] == 0xEF &&
            (unsigned char)jsonStr[1] == 0xBB && (unsigned char)jsonStr[2] == 0xBF) {
            jsonStr.erase(0, 3);
        }

        rapidjson::Document doc;
        doc.Parse(jsonStr.c_str());

        if (doc.HasParseError()) return;

        if (doc.IsObject()) {
            for (auto itr = doc.MemberBegin(); itr != doc.MemberEnd(); ++itr) {
                if (itr->value.IsObject()) {
                    std::string category = itr->name.GetString();
                    for (auto jtr = itr->value.MemberBegin(); jtr != itr->value.MemberEnd(); ++jtr) {
                        if (jtr->value.IsString()) {
                            LangMap[category + "." + jtr->name.GetString()] = jtr->value.GetString();
                        }
                    }
                }
                else if (itr->value.IsString()) {
                    LangMap[itr->name.GetString()] = itr->value.GetString();
                }
            }
        }
    }

    inline const char* GetLoc(const std::string& key, const char* defaultVal) {
        auto it = LangMap.find(key);
        if (it != LangMap.end()) return it->second.c_str();
        return defaultVal;
    }

    inline std::map<std::string, int> actionCache;
    inline std::map<std::string, int> gestureCache;
    inline std::map<std::string, int> motionCache;

    inline bool showGestureTrail = true;
    inline float globalTapWindow = 0.35f;
    inline float globalHoldDuration = 0.30f;
    inline bool advancedMode = false;
    inline bool defaultActionsGenerated = false;
    inline bool showDebugLogs = false;

    inline std::string SanitizeFileName(const std::string& input) {
        std::string output = input;
        std::string invalidChars = "<>:\"/\\|?*";
        for (char c : invalidChars) {
            output.erase(std::remove(output.begin(), output.end(), c), output.end());
        }
        if (output.empty() || output.find_first_not_of(' ') == std::string::npos) {
            output = "Unnamed_File";
        }
        return output + ".json";
    }

    struct MovementEntry {
        char name[64] = "New Gesture";

        std::vector<GestureMath::Point2D> rawPoints;
        std::vector<GestureMath::Point2D> normalizedPoints;
        float requiredAccuracy = 0.80f;
        int targetActionID = -1;
    };

    inline std::vector<MovementEntry> movementList;

    struct ActionEntry {
        char name[64] = "New Action";

        int pcMainKey = 0;
        int pcMainAction = 1;
        int pcMainTapCount = 1;
        bool pcDelayTap = false;
        int pcModifierKey = 0;
        int pcModAction = 0;
        int pcModTapCount = 1;

        int gamepadMainKey = 0;
        int gamepadMainAction = 1;
        int gamepadMainTapCount = 1;
        bool gamepadDelayTap = false;
        int gamepadModifierKey = 0;
        int gamepadModAction = 0;
        int gamepadModTapCount = 1;

        int gamepadGestureStick = 0;

        bool useCustomTimings = false;
        float holdDuration = 0.3f;
        float tapWindow = 0.35f;
        int gestureIndex = -1;
    };

    inline std::vector<ActionEntry> actionList;
    inline const char* GetStateName(int state) {
        switch (state) {
        case 0: return GetLoc("state.ignore", "Ignore");
        case 1: return GetLoc("state.tap", "Tap");
        case 2: return GetLoc("state.hold", "Hold");
        case 3: return GetLoc("state.gesture", "Gesture");
        case 4: return GetLoc("state.press", "Press");
        }
        return GetLoc("common.unknown", "Unknown");
    }

    constexpr uint32_t MOUSE_OFFSET = 256;
    constexpr uint32_t GAMEPAD_OFFSET = 266;

    struct MotionEntry {
        char name[64] = "New Motion";
        std::vector<uint32_t> pcSequence;
        std::vector<uint32_t> padSequence;
        float timeWindow = 2.0f;
    };
    inline std::vector<MotionEntry> motionList;
    inline std::vector<std::string> loadedMotionFiles;

    // --- KEY LISTS ---
    inline const char* pcKeyNames[] = {
        "None",
        // Mouse
        "Mouse 1 (Left)", "Mouse 2 (Right)", "Mouse 3 (Middle)", "Mouse 4", "Mouse 5", "Mouse 6", "Mouse 7", "Mouse 8",
        "Mouse Wheel Up", "Mouse Wheel Down",
        // Alfabeto
        "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
        // Números
        "1", "2", "3", "4", "5", "6", "7", "8", "9", "0",
        // Pontuação e Símbolos
        "Minus ( - )", "Equals ( = )", "Bracket Left ( [ )", "Bracket Right ( ] )", "Semicolon ( ; )", "Apostrophe ( ' )", "Tilde ( ~ )", "Backslash ( \\ )", "Comma ( , )", "Period ( . )", "Slash ( / )",
        // F-Keys
        "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12",
        // Especiais e Modificadores
        "Esc", "Tab", "Caps Lock", "Shift (Left)", "Shift (Right)", "Ctrl (Left)", "Ctrl (Right)", "Alt (Left)", "Alt (Right)",
        "Space", "Enter", "Backspace", "Print Screen", "Scroll Lock", "Pause", "Num Lock",
        // Navegação
        "Up Arrow", "Down Arrow", "Left Arrow", "Right Arrow", "Insert", "Delete", "Home", "End", "Page Up", "Page Down",
        // Numpad
        "Num 0", "Num 1", "Num 2", "Num 3", "Num 4", "Num 5", "Num 6", "Num 7", "Num 8", "Num 9",
        "Num +", "Num -", "Num *", "Num /", "Num Enter", "Num Dot"
    };

    inline const int pcKeyIDs[] = {
        0,
        // Mouse (Base + 256)
        RE::BSWin32MouseDevice::Keys::kLeftButton + MOUSE_OFFSET, RE::BSWin32MouseDevice::Keys::kRightButton + MOUSE_OFFSET,
        RE::BSWin32MouseDevice::Keys::kMiddleButton + MOUSE_OFFSET, RE::BSWin32MouseDevice::Keys::kButton3 + MOUSE_OFFSET,
        RE::BSWin32MouseDevice::Keys::kButton4 + MOUSE_OFFSET, RE::BSWin32MouseDevice::Keys::kButton5 + MOUSE_OFFSET,
        RE::BSWin32MouseDevice::Keys::kButton6 + MOUSE_OFFSET, RE::BSWin32MouseDevice::Keys::kButton7 + MOUSE_OFFSET,
        RE::BSWin32MouseDevice::Keys::kWheelUp + MOUSE_OFFSET, RE::BSWin32MouseDevice::Keys::kWheelDown + MOUSE_OFFSET,
        // Alfabeto
        RE::BSKeyboardDevice::Keys::kA, RE::BSKeyboardDevice::Keys::kB, RE::BSKeyboardDevice::Keys::kC, RE::BSKeyboardDevice::Keys::kD,
        RE::BSKeyboardDevice::Keys::kE, RE::BSKeyboardDevice::Keys::kF, RE::BSKeyboardDevice::Keys::kG, RE::BSKeyboardDevice::Keys::kH,
        RE::BSKeyboardDevice::Keys::kI, RE::BSKeyboardDevice::Keys::kJ, RE::BSKeyboardDevice::Keys::kK, RE::BSKeyboardDevice::Keys::kL,
        RE::BSKeyboardDevice::Keys::kM, RE::BSKeyboardDevice::Keys::kN, RE::BSKeyboardDevice::Keys::kO, RE::BSKeyboardDevice::Keys::kP,
        RE::BSKeyboardDevice::Keys::kQ, RE::BSKeyboardDevice::Keys::kR, RE::BSKeyboardDevice::Keys::kS, RE::BSKeyboardDevice::Keys::kT,
        RE::BSKeyboardDevice::Keys::kU, RE::BSKeyboardDevice::Keys::kV, RE::BSKeyboardDevice::Keys::kW, RE::BSKeyboardDevice::Keys::kX,
        RE::BSKeyboardDevice::Keys::kY, RE::BSKeyboardDevice::Keys::kZ,
        // Números
        RE::BSKeyboardDevice::Keys::kNum1, RE::BSKeyboardDevice::Keys::kNum2, RE::BSKeyboardDevice::Keys::kNum3, RE::BSKeyboardDevice::Keys::kNum4,
        RE::BSKeyboardDevice::Keys::kNum5, RE::BSKeyboardDevice::Keys::kNum6, RE::BSKeyboardDevice::Keys::kNum7, RE::BSKeyboardDevice::Keys::kNum8,
        RE::BSKeyboardDevice::Keys::kNum9, RE::BSKeyboardDevice::Keys::kNum0,
        // Pontuação e Símbolos
        RE::BSKeyboardDevice::Keys::kMinus, RE::BSKeyboardDevice::Keys::kEquals, RE::BSKeyboardDevice::Keys::kBracketLeft,
        RE::BSKeyboardDevice::Keys::kBracketRight, RE::BSKeyboardDevice::Keys::kSemicolon, RE::BSKeyboardDevice::Keys::kApostrophe,
        RE::BSKeyboardDevice::Keys::kTilde, RE::BSKeyboardDevice::Keys::kBackslash, RE::BSKeyboardDevice::Keys::kComma,
        RE::BSKeyboardDevice::Keys::kPeriod, RE::BSKeyboardDevice::Keys::kSlash,
        // F-Keys
        RE::BSKeyboardDevice::Keys::kF1, RE::BSKeyboardDevice::Keys::kF2, RE::BSKeyboardDevice::Keys::kF3, RE::BSKeyboardDevice::Keys::kF4,
        RE::BSKeyboardDevice::Keys::kF5, RE::BSKeyboardDevice::Keys::kF6, RE::BSKeyboardDevice::Keys::kF7, RE::BSKeyboardDevice::Keys::kF8,
        RE::BSKeyboardDevice::Keys::kF9, RE::BSKeyboardDevice::Keys::kF10, RE::BSKeyboardDevice::Keys::kF11, RE::BSKeyboardDevice::Keys::kF12,
        // Especiais e Modificadores
        RE::BSKeyboardDevice::Keys::kEscape, RE::BSKeyboardDevice::Keys::kTab, RE::BSKeyboardDevice::Keys::kCapsLock,
        RE::BSKeyboardDevice::Keys::kLeftShift, RE::BSKeyboardDevice::Keys::kRightShift,
        RE::BSKeyboardDevice::Keys::kLeftControl, RE::BSKeyboardDevice::Keys::kRightControl,
        RE::BSKeyboardDevice::Keys::kLeftAlt, RE::BSKeyboardDevice::Keys::kRightAlt,
        RE::BSKeyboardDevice::Keys::kSpacebar, RE::BSKeyboardDevice::Keys::kEnter, RE::BSKeyboardDevice::Keys::kBackspace,
        RE::BSKeyboardDevice::Keys::kPrintScreen, RE::BSKeyboardDevice::Keys::kScrollLock, RE::BSKeyboardDevice::Keys::kPause,
        RE::BSKeyboardDevice::Keys::kNumLock,
        // Navegação
        RE::BSKeyboardDevice::Keys::kUp, RE::BSKeyboardDevice::Keys::kDown, RE::BSKeyboardDevice::Keys::kLeft, RE::BSKeyboardDevice::Keys::kRight,
        RE::BSKeyboardDevice::Keys::kInsert, RE::BSKeyboardDevice::Keys::kDelete, RE::BSKeyboardDevice::Keys::kHome, RE::BSKeyboardDevice::Keys::kEnd,
        RE::BSKeyboardDevice::Keys::kPageUp, RE::BSKeyboardDevice::Keys::kPageDown,
        // Numpad
        RE::BSKeyboardDevice::Keys::kKP_0, RE::BSKeyboardDevice::Keys::kKP_1, RE::BSKeyboardDevice::Keys::kKP_2, RE::BSKeyboardDevice::Keys::kKP_3,
        RE::BSKeyboardDevice::Keys::kKP_4, RE::BSKeyboardDevice::Keys::kKP_5, RE::BSKeyboardDevice::Keys::kKP_6, RE::BSKeyboardDevice::Keys::kKP_7,
        RE::BSKeyboardDevice::Keys::kKP_8, RE::BSKeyboardDevice::Keys::kKP_9,
        RE::BSKeyboardDevice::Keys::kKP_Plus, RE::BSKeyboardDevice::Keys::kKP_Subtract, RE::BSKeyboardDevice::Keys::kKP_Multiply,
        RE::BSKeyboardDevice::Keys::kKP_Divide, RE::BSKeyboardDevice::Keys::kKP_Enter, RE::BSKeyboardDevice::Keys::kKP_Decimal
    };

    inline const char* gamepadKeyNames[] = {
        "None",
        "D-Pad Up", "D-Pad Down", "D-Pad Left", "D-Pad Right",
        "Start / Options", "Back / Share / Select", "LS / L3 (Left Stick)", "RS / R3 (Right Stick)",
        "LB / L1 (Left Bumper)", "RB / R1 (Right Bumper)",
        "LT / L2 (Left Trigger)", "RT / R2 (Right Trigger)",
        "A / Cross", "B / Circle", "X / Square", "Y / Triangle"
    };

    inline const int gamepadKeyIDs[] = {
        0,
        RE::BSWin32GamepadDevice::Keys::kUp + GAMEPAD_OFFSET,
        RE::BSWin32GamepadDevice::Keys::kDown + GAMEPAD_OFFSET,
        RE::BSWin32GamepadDevice::Keys::kLeft + GAMEPAD_OFFSET,
        RE::BSWin32GamepadDevice::Keys::kRight + GAMEPAD_OFFSET,
        RE::BSWin32GamepadDevice::Keys::kStart + GAMEPAD_OFFSET,
        RE::BSWin32GamepadDevice::Keys::kBack + GAMEPAD_OFFSET,
        RE::BSWin32GamepadDevice::Keys::kLeftThumb + GAMEPAD_OFFSET,
        RE::BSWin32GamepadDevice::Keys::kRightThumb + GAMEPAD_OFFSET,
        RE::BSWin32GamepadDevice::Keys::kLeftShoulder + GAMEPAD_OFFSET,
        RE::BSWin32GamepadDevice::Keys::kRightShoulder + GAMEPAD_OFFSET,
        RE::BSWin32GamepadDevice::Keys::kLeftTrigger + GAMEPAD_OFFSET,
        RE::BSWin32GamepadDevice::Keys::kRightTrigger + GAMEPAD_OFFSET,
        RE::BSWin32GamepadDevice::Keys::kA + GAMEPAD_OFFSET,
        RE::BSWin32GamepadDevice::Keys::kB + GAMEPAD_OFFSET,
        RE::BSWin32GamepadDevice::Keys::kX + GAMEPAD_OFFSET,
        RE::BSWin32GamepadDevice::Keys::kY + GAMEPAD_OFFSET
    };

    inline int GetIndexFromID(int id, const int* idArray, int arraySize) {
        for (int i = 0; i < arraySize; i++) {
            if (idArray[i] == id) return i;
        }
        return 0;
    }

    inline void SyncActionsWithEngine() {
        auto keyManager = PluginLogic::KeyManager::GetSingleton();
        keyManager->ClearBindings();

        for (size_t i = 0; i < actionList.size(); ++i) {
            const auto& entry = actionList[i];
            if (entry.pcMainKey == 0 && entry.gamepadMainKey == 0) continue;

            auto createCallback = [](int id, const std::string& name, const std::string& source) {
                return [id, name, source]() {
                    if (showDebugLogs) {
                        std::string mensagemDebug = "Action triggered: " + name + " via " + source;
                        logger::info("[SUCCESS] {}", mensagemDebug);
                        RE::SendHUDMessage::ShowHUDMessage(mensagemDebug.c_str());
                    }
                    InputManagerAPI::SendActionTriggeredEvent(id, name);
                    };
                };

            auto createReleaseCallback = [](int id, const std::string& name, const std::string& source) {
                return [id, name, source]() {
                    if (showDebugLogs) {
                        std::string mensagemDebug = "Action released: " + name + " via " + source;
                        logger::info("[SUCCESS] {}", mensagemDebug);
                        RE::SendHUDMessage::ShowHUDMessage(mensagemDebug.c_str());
                    }
                    InputManagerAPI::SendActionReleasedEvent(id, name);
                    };
                };

            if (entry.pcMainKey != 0 && entry.pcMainAction != 0) {
                PluginLogic::ComboKey pcCombo;
                pcCombo.mainKey = entry.pcMainKey;
                pcCombo.mainActionType = static_cast<PluginLogic::ActionState>(entry.pcMainAction);
                pcCombo.mainTapCount = entry.pcMainTapCount;
                pcCombo.needsDelay = entry.pcDelayTap;
                pcCombo.modifierKey = entry.pcModifierKey;
                pcCombo.modifierActionType = entry.pcModifierKey != 0 || entry.pcModAction == 3 ?
                    static_cast<PluginLogic::ActionState>(entry.pcModAction) : PluginLogic::ActionState::kIgnored;
                pcCombo.modTapCount = entry.pcModTapCount;

                pcCombo.useCustomTimings = entry.useCustomTimings;
                pcCombo.holdDuration = entry.useCustomTimings ? entry.holdDuration : globalHoldDuration;
                pcCombo.tapWindow = entry.useCustomTimings ? entry.tapWindow : globalTapWindow;
                pcCombo.gestureIndex = entry.gestureIndex;

                keyManager->RegisterAction(std::to_string(i) + "_PC", pcCombo,
                    createCallback(static_cast<int>(i), entry.name, "PC"),
                    createReleaseCallback(static_cast<int>(i), entry.name, "PC"));
            }

            if (entry.gamepadMainKey != 0 && entry.gamepadMainAction != 0) {
                PluginLogic::ComboKey padCombo;
                padCombo.mainKey = entry.gamepadMainKey;
                padCombo.mainActionType = static_cast<PluginLogic::ActionState>(entry.gamepadMainAction);
                padCombo.mainTapCount = entry.gamepadMainTapCount;
                padCombo.needsDelay = entry.gamepadDelayTap;
                padCombo.modifierKey = entry.gamepadModifierKey;
                padCombo.modifierActionType = entry.gamepadModifierKey != 0 || entry.gamepadModAction == 3 ?
                    static_cast<PluginLogic::ActionState>(entry.gamepadModAction) : PluginLogic::ActionState::kIgnored;
                padCombo.modTapCount = entry.gamepadModTapCount;

                padCombo.useCustomTimings = entry.useCustomTimings;
                padCombo.holdDuration = entry.useCustomTimings ? entry.holdDuration : globalHoldDuration;
                padCombo.tapWindow = entry.useCustomTimings ? entry.tapWindow : globalTapWindow;
                padCombo.gestureIndex = entry.gestureIndex;
                padCombo.gamepadGestureStick = entry.gamepadGestureStick;

                keyManager->RegisterAction(std::to_string(i) + "_PAD", padCombo,
                    createCallback(static_cast<int>(i), entry.name, "Gamepad"),
                    createReleaseCallback(static_cast<int>(i), entry.name, "Gamepad"));
            }
        }
        keyManager->SortBindings();
    }

    inline std::vector<std::string> loadedInputFiles;
    inline std::vector<std::string> loadedGestureFiles;

    inline void LoadSettingsFromJson() {
        std::string targetPath = SETTINGS_PATH;
        try {
            std::ifstream ifs(targetPath);
            rapidjson::IStreamWrapper isw(ifs);
            rapidjson::Document doc;
            doc.ParseStream(isw);
            ifs.close();
            if (doc.HasParseError() || !doc.IsObject()) return;

            if (doc.HasMember("showGestureTrail")) showGestureTrail = doc["showGestureTrail"].GetBool();
            if (doc.HasMember("globalTapWindow")) globalTapWindow = doc["globalTapWindow"].GetFloat();
            if (doc.HasMember("globalHoldDuration")) globalHoldDuration = doc["globalHoldDuration"].GetFloat();
            if (doc.HasMember("advancedMode")) advancedMode = doc["advancedMode"].GetBool();
            if (doc.HasMember("defaultActionsGenerated")) defaultActionsGenerated = doc["defaultActionsGenerated"].GetBool();
            if (doc.HasMember("showDebugLogs")) showDebugLogs = doc["showDebugLogs"].GetBool();
        }
        catch (...) {}
    }

    inline void SaveSettingsToJson() {
        try {
            fs::create_directories(BASE_DIR);
            rapidjson::Document doc;
            doc.SetObject();
            auto& allocator = doc.GetAllocator();

            doc.AddMember("showGestureTrail", showGestureTrail, allocator);
            doc.AddMember("globalTapWindow", globalTapWindow, allocator);
            doc.AddMember("globalHoldDuration", globalHoldDuration, allocator);
            doc.AddMember("advancedMode", advancedMode, allocator);
            doc.AddMember("defaultActionsGenerated", defaultActionsGenerated, allocator);
            doc.AddMember("showDebugLogs", showDebugLogs, allocator);

            std::ofstream ofs(SETTINGS_PATH);
            rapidjson::OStreamWrapper osw(ofs);
            rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer(osw);
            doc.Accept(writer);
        }
        catch (...) {}
    }

    inline void LoadCacheFromJson() {
        actionCache.clear(); gestureCache.clear(); motionCache.clear();
        try {
            std::ifstream ifs(CACHE_PATH);
            if (!ifs.is_open()) return;
            rapidjson::IStreamWrapper isw(ifs);
            rapidjson::Document doc;
            doc.ParseStream(isw);
            ifs.close();

            if (doc.IsObject()) {
                if (doc.HasMember("Actions") && doc["Actions"].IsObject()) {
                    for (auto& m : doc["Actions"].GetObj()) actionCache[m.name.GetString()] = m.value.GetInt();
                }
                if (doc.HasMember("Gestures") && doc["Gestures"].IsObject()) {
                    for (auto& m : doc["Gestures"].GetObj()) gestureCache[m.name.GetString()] = m.value.GetInt();
                }
                if (doc.HasMember("Motions") && doc["Motions"].IsObject()) {
                    for (auto& m : doc["Motions"].GetObj()) motionCache[m.name.GetString()] = m.value.GetInt();
                }
            }
        }
        catch (...) {}
    }

    inline void SaveCacheToJson() {
        try {
            fs::create_directories(BASE_DIR);
            rapidjson::Document doc;
            doc.SetObject();
            auto& alloc = doc.GetAllocator();

            rapidjson::Value actionsObj(rapidjson::kObjectType);
            for (size_t i = 0; i < actionList.size(); ++i) {
                actionsObj.AddMember(rapidjson::StringRef(actionList[i].name), static_cast<int>(i), alloc);
            }
            doc.AddMember("Actions", actionsObj, alloc);

            rapidjson::Value gesturesObj(rapidjson::kObjectType);
            for (size_t i = 0; i < movementList.size(); ++i) {
                gesturesObj.AddMember(rapidjson::StringRef(movementList[i].name), static_cast<int>(i), alloc);
            }
            doc.AddMember("Gestures", gesturesObj, alloc);

            rapidjson::Value motionsObj(rapidjson::kObjectType);
            for (size_t i = 0; i < motionList.size(); ++i) {
                motionsObj.AddMember(rapidjson::StringRef(motionList[i].name), static_cast<int>(i), alloc);
            }
            doc.AddMember("Motions", motionsObj, alloc);

            std::ofstream ofs(CACHE_PATH);
            rapidjson::OStreamWrapper osw(ofs);
            rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer(osw);
            doc.Accept(writer);
        }
        catch (...) {}
    }

    inline void LoadActionsFromJson() {
        actionList.clear();
        loadedInputFiles.clear();
        fs::create_directories(INPUTS_DIR);
        std::vector<std::pair<int, ActionEntry>> tempLoaded;

        auto parseActionItem = [&](const rapidjson::Value& item) {
            ActionEntry entry;
            std::string actName = item.HasMember("name") ? item["name"].GetString() : "New Action";
            strncpy_s(entry.name, actName.c_str(), sizeof(entry.name) - 1);

            // PRIORIDADE: 1. Busca no Cache -> 2. Tenta do Json atual -> 3. Joga pro final
            int order = 999999;
            if (actionCache.find(actName) != actionCache.end()) {
                order = actionCache[actName];
            }
            else if (item.HasMember("order") && item["order"].IsInt()) {
                order = item["order"].GetInt();
            }

            if (item.HasMember("pcMainKey")) entry.pcMainKey = item["pcMainKey"].GetInt();
            if (item.HasMember("pcMainAction")) entry.pcMainAction = item["pcMainAction"].GetInt();
            if (item.HasMember("pcMainTapCount")) entry.pcMainTapCount = item["pcMainTapCount"].GetInt();
            if (item.HasMember("pcModifierKey")) entry.pcModifierKey = item["pcModifierKey"].GetInt();
            if (item.HasMember("pcModAction")) entry.pcModAction = item["pcModAction"].GetInt();
            if (item.HasMember("pcModTapCount")) entry.pcModTapCount = item["pcModTapCount"].GetInt();
            if (item.HasMember("pcDelayTap") && item["pcDelayTap"].IsBool()) entry.pcDelayTap = item["pcDelayTap"].GetBool();

            if (item.HasMember("gamepadMainKey")) entry.gamepadMainKey = item["gamepadMainKey"].GetInt();
            if (item.HasMember("gamepadMainAction")) entry.gamepadMainAction = item["gamepadMainAction"].GetInt();
            if (item.HasMember("gamepadMainTapCount")) entry.gamepadMainTapCount = item["gamepadMainTapCount"].GetInt();
            if (item.HasMember("gamepadModifierKey")) entry.gamepadModifierKey = item["gamepadModifierKey"].GetInt();
            if (item.HasMember("gamepadModAction")) entry.gamepadModAction = item["gamepadModAction"].GetInt();
            if (item.HasMember("gamepadModTapCount")) entry.gamepadModTapCount = item["gamepadModTapCount"].GetInt();
            if (item.HasMember("gamepadDelayTap") && item["gamepadDelayTap"].IsBool()) entry.gamepadDelayTap = item["gamepadDelayTap"].GetBool();

            if (item.HasMember("gestureIndex")) entry.gestureIndex = item["gestureIndex"].GetInt();
            if (item.HasMember("gamepadGestureStick")) entry.gamepadGestureStick = item["gamepadGestureStick"].GetInt();

            if (item.HasMember("useCustomTimings")) entry.useCustomTimings = item["useCustomTimings"].GetBool();
            if (item.HasMember("holdDuration")) entry.holdDuration = item["holdDuration"].GetFloat();
            if (item.HasMember("tapWindow")) entry.tapWindow = item["tapWindow"].GetFloat();

            tempLoaded.push_back({ order, entry });
            };

        for (const auto& entry : fs::directory_iterator(INPUTS_DIR)) {
            if (entry.path().extension() == ".json") {
                loadedInputFiles.push_back(entry.path().filename().string());
                std::ifstream ifs(entry.path());
                rapidjson::IStreamWrapper isw(ifs);
                rapidjson::Document doc;
                doc.ParseStream(isw);
                ifs.close();
                if (doc.IsArray()) {
                    for (rapidjson::SizeType i = 0; i < doc.Size(); i++) {
                        if (doc[i].IsObject()) parseActionItem(doc[i]);
                    }
                }
            }
        }

        std::sort(tempLoaded.begin(), tempLoaded.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        for (const auto& pair : tempLoaded) actionList.push_back(pair.second);
        SyncActionsWithEngine();
    }

    inline void SaveActionsToJson() {
        try {
            fs::create_directories(INPUTS_DIR);
            std::vector<std::string> uniqueFiles;
            for (const auto& action : actionList) {
                std::string file = SanitizeFileName(action.name);
                if (std::find(uniqueFiles.begin(), uniqueFiles.end(), file) == uniqueFiles.end()) {
                    uniqueFiles.push_back(file);
                }
            }


            for (const auto& targetFile : uniqueFiles) {
                rapidjson::Document doc;
                doc.SetArray();
                auto& allocator = doc.GetAllocator();

                for (size_t i = 0; i < actionList.size(); ++i) {
                    const auto& action = actionList[i];
                    std::string actFile = SanitizeFileName(action.name);

                    if (actFile == targetFile) {
                        rapidjson::Value actionObj(rapidjson::kObjectType);
                        actionObj.AddMember("order", static_cast<int>(i), allocator);

                        rapidjson::Value nameVal;
                        nameVal.SetString(action.name, allocator);
                        actionObj.AddMember("name", nameVal, allocator);

                        actionObj.AddMember("pcMainKey", action.pcMainKey, allocator);
                        actionObj.AddMember("pcMainAction", action.pcMainAction, allocator);
                        actionObj.AddMember("pcMainTapCount", action.pcMainTapCount, allocator);
                        actionObj.AddMember("pcModifierKey", action.pcModifierKey, allocator);
                        actionObj.AddMember("pcModAction", action.pcModAction, allocator);
                        actionObj.AddMember("pcModTapCount", action.pcModTapCount, allocator);
                        actionObj.AddMember("pcDelayTap", action.pcDelayTap, allocator);

                        actionObj.AddMember("gamepadMainKey", action.gamepadMainKey, allocator);
                        actionObj.AddMember("gamepadMainAction", action.gamepadMainAction, allocator);
                        actionObj.AddMember("gamepadMainTapCount", action.gamepadMainTapCount, allocator);
                        actionObj.AddMember("gamepadModifierKey", action.gamepadModifierKey, allocator);
                        actionObj.AddMember("gamepadModAction", action.gamepadModAction, allocator);
                        actionObj.AddMember("gamepadModTapCount", action.gamepadModTapCount, allocator);
                        actionObj.AddMember("gamepadDelayTap", action.gamepadDelayTap, allocator);

                        actionObj.AddMember("gestureIndex", action.gestureIndex, allocator);
                        actionObj.AddMember("gamepadGestureStick", action.gamepadGestureStick, allocator);

                        actionObj.AddMember("useCustomTimings", action.useCustomTimings, allocator);
                        actionObj.AddMember("holdDuration", action.holdDuration, allocator);
                        actionObj.AddMember("tapWindow", action.tapWindow, allocator);

                        doc.PushBack(actionObj, allocator);
                    }
                }

                std::ofstream ofs(INPUTS_DIR + targetFile);
                rapidjson::OStreamWrapper osw(ofs);
                rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer(osw);
                doc.Accept(writer);
                ofs.close();
            }

            // Cleanup: Deletes files that were emptied/renamed in the menu
            for (const auto& loadedFile : loadedInputFiles) {
                if (std::find(uniqueFiles.begin(), uniqueFiles.end(), loadedFile) == uniqueFiles.end()) {
                    std::string path = INPUTS_DIR + loadedFile;
                    if (fs::exists(path)) fs::remove(path);
                }
            }
            loadedInputFiles = uniqueFiles;
            SaveCacheToJson();
            SyncActionsWithEngine();
        }
        catch (...) {}
    }

    inline void LoadGesturesFromJson() {
        movementList.clear();
        loadedGestureFiles.clear();
        fs::create_directories(GESTURES_DIR);
        std::vector<std::pair<int, MovementEntry>> tempLoaded;

        for (const auto& entry : fs::directory_iterator(GESTURES_DIR)) {
            if (entry.path().extension() == ".json") {
                std::string fileName = entry.path().filename().string();
                loadedGestureFiles.push_back(fileName);

                std::ifstream ifs(entry.path());
                rapidjson::IStreamWrapper isw(ifs);
                rapidjson::Document doc;
                doc.ParseStream(isw);
                ifs.close();

                if (doc.IsArray()) {
                    for (const auto& item : doc.GetArray()) {
                        if (!item.IsObject()) continue;
                        MovementEntry g;
                        std::string gName = item.HasMember("name") ? item["name"].GetString() : "New Gesture";
                        strncpy_s(g.name, gName.c_str(), sizeof(g.name) - 1);

                        int order = 999999;
                        if (gestureCache.find(gName) != gestureCache.end()) {
                            order = gestureCache[gName];
                        }
                        else if (item.HasMember("order") && item["order"].IsInt()) {
                            order = item["order"].GetInt();
                        }
                        if (item.HasMember("requiredAccuracy")) g.requiredAccuracy = item["requiredAccuracy"].GetFloat();
                        if (item.HasMember("normalizedPoints") && item["normalizedPoints"].IsArray()) {
                            for (const auto& ptNode : item["normalizedPoints"].GetArray()) {
                                g.normalizedPoints.push_back({ ptNode["x"].GetFloat(), ptNode["y"].GetFloat() });
                            }
                        }
                        movementList.push_back(g);
                    }
                }
            }
        }
        std::stable_sort(tempLoaded.begin(), tempLoaded.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        for (const auto& pair : tempLoaded) movementList.push_back(pair.second);
    }

    inline void SaveGesturesToJson() {
        fs::create_directories(GESTURES_DIR);
        std::vector<std::string> uniqueFiles;
        for (const auto& g : movementList) {
            std::string file = SanitizeFileName(g.name);
            if (std::find(uniqueFiles.begin(), uniqueFiles.end(), file) == uniqueFiles.end()) {
                uniqueFiles.push_back(file);
            }
        }

        for (const auto& targetFile : uniqueFiles) {
            rapidjson::Document doc;
            doc.SetArray();
            auto& allocator = doc.GetAllocator();

            for (size_t i = 0; i < movementList.size(); ++i) {
                const auto& g = movementList[i];
                std::string gFile = SanitizeFileName(g.name);

                if (gFile == targetFile) {
                    rapidjson::Value obj(rapidjson::kObjectType);
                    obj.AddMember("order", static_cast<int>(i), allocator); 
                    obj.AddMember("name", rapidjson::StringRef(g.name), allocator);
                    obj.AddMember("requiredAccuracy", g.requiredAccuracy, allocator);

                    rapidjson::Value normArray(rapidjson::kArrayType);
                    for (const auto& p : g.normalizedPoints) {
                        rapidjson::Value pt(rapidjson::kObjectType);
                        pt.AddMember("x", p.x, allocator);
                        pt.AddMember("y", p.y, allocator);
                        normArray.PushBack(pt, allocator);
                    }
                    obj.AddMember("normalizedPoints", normArray, allocator);
                    doc.PushBack(obj, allocator);
                }
            }

            std::ofstream ofs(GESTURES_DIR + targetFile);
            rapidjson::OStreamWrapper osw(ofs);
            rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer(osw);
            doc.Accept(writer);
            ofs.close();
        }

        for (const auto& loadedFile : loadedGestureFiles) {
            if (std::find(uniqueFiles.begin(), uniqueFiles.end(), loadedFile) == uniqueFiles.end()) {
                std::string path = GESTURES_DIR + loadedFile;
                if (fs::exists(path)) fs::remove(path);
            }
        }
        loadedGestureFiles = uniqueFiles;
        SaveCacheToJson();
    }


    inline std::string FormatMotionKey(uint32_t keyID) {
        if (keyID == InputManagerAPI::VKEY_DIR_UP) return "UP";
        if (keyID == InputManagerAPI::VKEY_DIR_DOWN) return "DOWN";
        if (keyID == InputManagerAPI::VKEY_DIR_LEFT) return "LEFT";
        if (keyID == InputManagerAPI::VKEY_DIR_RIGHT) return "RIGHT";
        if (keyID == InputManagerAPI::VKEY_DIR_UPLEFT) return "UP-LEFT";
        if (keyID == InputManagerAPI::VKEY_DIR_UPRIGHT) return "UP-RIGHT";
        if (keyID == InputManagerAPI::VKEY_DIR_DOWNLEFT) return "DOWN-LEFT";
        if (keyID == InputManagerAPI::VKEY_DIR_DOWNRIGHT) return "DOWN-RIGHT";

        // Se for PC
        int pcIdx = GetIndexFromID(keyID, pcKeyIDs, std::size(pcKeyIDs));
        if (pcIdx != 0) return std::string(pcKeyNames[pcIdx]);
        // Se for Gamepad
        int padIdx = GetIndexFromID(keyID, gamepadKeyIDs, std::size(gamepadKeyIDs));
        if (padIdx != 0) return std::string(gamepadKeyNames[padIdx]);

        return "Unknown";
    }

    inline void LoadMotionsFromJson() {
        motionList.clear();
        loadedMotionFiles.clear();
        fs::create_directories(MOTIONS_DIR);
        std::vector<std::pair<int, MotionEntry>> tempLoaded;

        for (const auto& entry : fs::directory_iterator(MOTIONS_DIR)) {
            if (entry.path().extension() == ".json") {
                loadedMotionFiles.push_back(entry.path().filename().string());
                std::ifstream ifs(entry.path());
                rapidjson::IStreamWrapper isw(ifs);
                rapidjson::Document doc;
                doc.ParseStream(isw);
                ifs.close();

                if (doc.IsArray()) {
                    for (const auto& item : doc.GetArray()) {
                        if (!item.IsObject()) continue;
                        MotionEntry m;
                        std::string mName = item.HasMember("name") ? item["name"].GetString() : "New Motion";
                        strncpy_s(m.name, mName.c_str(), sizeof(m.name) - 1);

                        int order = 999999;
                        if (motionCache.find(mName) != motionCache.end()) {
                            order = motionCache[mName];
                        }
                        else if (item.HasMember("order") && item["order"].IsInt()) {
                            order = item["order"].GetInt();
                        }

                        if (item.HasMember("pcSequence") && item["pcSequence"].IsArray()) {
                            for (const auto& k : item["pcSequence"].GetArray()) m.pcSequence.push_back(k.GetUint());
                        }
                        if (item.HasMember("padSequence") && item["padSequence"].IsArray()) {
                            for (const auto& k : item["padSequence"].GetArray()) m.padSequence.push_back(k.GetUint());
                        }
                        if (item.HasMember("timeWindow")) m.timeWindow = item["timeWindow"].GetFloat();
                        tempLoaded.push_back({ order, m });
                    }
                }
            }
        }
        std::stable_sort(tempLoaded.begin(), tempLoaded.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        for (const auto& pair : tempLoaded) motionList.push_back(pair.second);
    }

    inline void SaveMotionsToJson() {
        fs::create_directories(MOTIONS_DIR);
        std::vector<std::string> uniqueFiles;

        for (const auto& m : motionList) {
            std::string file = SanitizeFileName(m.name);
            if (std::find(uniqueFiles.begin(), uniqueFiles.end(), file) == uniqueFiles.end()) {
                uniqueFiles.push_back(file);
            }
        }

        for (const auto& targetFile : uniqueFiles) {
            rapidjson::Document doc;
            doc.SetArray();
            auto& allocator = doc.GetAllocator();

            for (size_t i = 0; i < motionList.size(); ++i) {
                const auto& m = motionList[i];
                if (SanitizeFileName(m.name) == targetFile) {
                    rapidjson::Value obj(rapidjson::kObjectType);
                    obj.AddMember("order", static_cast<int>(i), allocator);
                    obj.AddMember("name", rapidjson::StringRef(m.name), allocator);

                    rapidjson::Value pcArr(rapidjson::kArrayType);
                    for (uint32_t k : m.pcSequence) pcArr.PushBack(k, allocator);
                    obj.AddMember("pcSequence", pcArr, allocator);

                    rapidjson::Value padArr(rapidjson::kArrayType);
                    for (uint32_t k : m.padSequence) padArr.PushBack(k, allocator);
                    obj.AddMember("padSequence", padArr, allocator);

                    obj.AddMember("timeWindow", m.timeWindow, allocator);
                    doc.PushBack(obj, allocator);
                }
            }
            std::ofstream ofs(MOTIONS_DIR + targetFile);
            rapidjson::OStreamWrapper osw(ofs);
            rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer(osw);
            doc.Accept(writer);
        }

        for (const auto& loadedFile : loadedMotionFiles) {
            if (std::find(uniqueFiles.begin(), uniqueFiles.end(), loadedFile) == uniqueFiles.end()) {
                std::string path = MOTIONS_DIR + loadedFile;
                if (fs::exists(path)) fs::remove(path);
            }
        }
        loadedMotionFiles = uniqueFiles;
        SaveCacheToJson(); 
    }



    inline void ShowDelayTooltip() {
        if (ImGuiMCP::IsItemHovered()) {
            ImGuiMCP::SetTooltip("%s", GetLoc("action.tooltip_delay",
                "What happens if unchecked?\n"
                "If there is another action requiring MORE taps (e.g., Tap 2) on this same key,\n"
                "the action with fewer taps (e.g., Tap 1) will trigger accidentally before it.\n\n"
                "Check this option so the system waits for the tap window to close,\n"
                "ensuring the exact number of taps is respected and preventing overlap."
            ));
        }
    }

    inline std::string ToLower(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
        return s;
    }

    inline std::string GetActionSummary(const ActionEntry& a) {
        std::string summary = "";

        if (a.pcMainKey != 0) {
            summary += GetLoc("common.pc", "PC") + std::string(": ");
            int mainIdx = GetIndexFromID(a.pcMainKey, pcKeyIDs, std::size(pcKeyIDs));
            summary += std::string(pcKeyNames[mainIdx]) + " (" + GetStateName(a.pcMainAction) + ")";

            if (a.pcModAction == 3) {
                std::string gName = (a.gestureIndex >= 0 && a.gestureIndex < movementList.size()) ? movementList[a.gestureIndex].name : GetLoc("common.unknown_gesture", "Unknown Gesture");
                summary += "  +  " + gName + " (" + GetLoc("state.gesture", "Gesture") + ")";
            }
            else if (a.pcModifierKey != 0) {
                int modIdx = GetIndexFromID(a.pcModifierKey, pcKeyIDs, std::size(pcKeyIDs));
                summary += "  +  " + std::string(pcKeyNames[modIdx]) + " (" + GetStateName(a.pcModAction) + ")";
            }
        }

        if (a.gamepadMainKey != 0) {
            if (!summary.empty()) summary += "  |  ";
            summary += GetLoc("common.pad", "PAD") + std::string(": ");
            int mainIdx = GetIndexFromID(a.gamepadMainKey, gamepadKeyIDs, std::size(gamepadKeyIDs));
            summary += std::string(gamepadKeyNames[mainIdx]) + " (" + GetStateName(a.gamepadMainAction) + ")";

            if (a.gamepadModAction == 3) {
                std::string gName = (a.gestureIndex >= 0 && a.gestureIndex < movementList.size()) ? movementList[a.gestureIndex].name : GetLoc("common.unknown_gesture", "Unknown Gesture");
                summary += "  +  " + gName + " (" + GetLoc("state.gesture", "Gesture") + ")";
            }
            else if (a.gamepadModifierKey != 0) {
                int modIdx = GetIndexFromID(a.gamepadModifierKey, gamepadKeyIDs, std::size(gamepadKeyIDs));
                summary += "  +  " + std::string(gamepadKeyNames[modIdx]) + " (" + GetStateName(a.gamepadModAction) + ")";
            }
        }

        if (summary.empty()) summary = GetLoc("dash.no_keys", "No keys configured");
        return summary;
    }

    inline bool HasKeyConfigured(const ActionEntry& action, const std::string& keyName) {
        if (keyName.empty()) return true;

        if (action.pcMainKey != 0) {
            int idx = GetIndexFromID(action.pcMainKey, pcKeyIDs, std::size(pcKeyIDs));
            if (std::string(pcKeyNames[idx]) == keyName) return true;
        }
        if (action.pcModifierKey != 0) {
            int idx = GetIndexFromID(action.pcModifierKey, pcKeyIDs, std::size(pcKeyIDs));
            if (std::string(pcKeyNames[idx]) == keyName) return true;
        }
        if (action.gamepadMainKey != 0) {
            int idx = GetIndexFromID(action.gamepadMainKey, gamepadKeyIDs, std::size(gamepadKeyIDs));
            if (std::string(gamepadKeyNames[idx]) == keyName) return true;
        }
        if (action.gamepadModifierKey != 0) {
            int idx = GetIndexFromID(action.gamepadModifierKey, gamepadKeyIDs, std::size(gamepadKeyIDs));
            if (std::string(gamepadKeyNames[idx]) == keyName) return true;
        }
        return false;
    }

    inline bool SearchableCombo(const char* label, int* current_item, const char* const items[], int items_count) {
        bool changed = false;
        const char* preview_value = (*current_item >= 0 && *current_item < items_count) ? items[*current_item] : GetLoc("common.none", "None");

        if (ImGuiMCP::BeginCombo(label, preview_value)) {
            static char searchBuf[128] = "";

            if (ImGuiMCP::IsWindowAppearing()) {
                searchBuf[0] = '\0';
                ImGuiMCP::SetKeyboardFocusHere();
            }

            std::string searchLabel = std::string(GetLoc("common.search", "Filter...")) + "##Search";
            ImGuiMCP::InputText(searchLabel.c_str(), searchBuf, sizeof(searchBuf));
            ImGuiMCP::Separator();

            std::string searchLower = ToLower(searchBuf);

            for (int i = 0; i < items_count; i++) {
                if (searchLower.empty() || ToLower(items[i]).find(searchLower) != std::string::npos) {
                    bool is_selected = (*current_item == i);
                    if (ImGuiMCP::Selectable(items[i], is_selected)) {
                        *current_item = i;
                        changed = true;
                    }
                    if (is_selected && ImGuiMCP::IsWindowAppearing()) {
                        ImGuiMCP::SetScrollHereY();
                    }
                }
            }
            ImGuiMCP::EndCombo();
        }
        return changed;
    }

    // =========================================================================
    // MENUS
    // =========================================================================

    inline void RenderMotionMenu() {
        ImGuiMCP::Text("%s", GetLoc("motion.title", "Input Manager - Motion Inputs (Fighting Game Style)"));
        ImGuiMCP::Separator(); ImGuiMCP::Spacing();

        bool hasMotionConflict = false;
        std::string motionErrorMsg;

        for (size_t i = 0; i < motionList.size(); ++i) {
            for (size_t j = i + 1; j < motionList.size(); ++j) {
                if (SanitizeFileName(motionList[i].name) == SanitizeFileName(motionList[j].name)) {
                    hasMotionConflict = true;
                    motionErrorMsg = GetLoc("error.dup_name", "DUPLICATE NAME: Motions conflict.");
                    break;
                }
                bool pcConflict = (!motionList[i].pcSequence.empty() && motionList[i].pcSequence == motionList[j].pcSequence);
                bool padConflict = (!motionList[i].padSequence.empty() && motionList[i].padSequence == motionList[j].padSequence);

                if (pcConflict || padConflict) {
                    hasMotionConflict = true;
                    motionErrorMsg = GetLoc("error.dup_seq", "DUPLICATE SEQUENCE: Identical inputs detected.");
                    break;
                }
            }
            if (hasMotionConflict) break;
        }

        if (hasMotionConflict) {
            ImGuiMCP::TextColored({ 1.0f, 0.2f, 0.2f, 1.0f }, "%s", GetLoc("error.conflict_title", "[CONFIGURATION ERROR]"));
            ImGuiMCP::TextColored({ 1.0f, 0.8f, 0.2f, 1.0f }, "%s", motionErrorMsg.c_str());
            ImGuiMCP::TextColored({ 0.7f, 0.7f, 0.7f, 1.0f }, "%s", GetLoc("error.resolve_motion", "Resolve the duplication by changing the name or recording a new sequence."));
            ImGuiMCP::Spacing(); ImGuiMCP::Separator(); ImGuiMCP::Spacing();
        }

        ImGuiMCP::BeginDisabled(hasMotionConflict);
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Button, { 0.1f, 0.5f, 0.1f, 1.0f });
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_ButtonHovered, { 0.2f, 0.6f, 0.2f, 1.0f });
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_ButtonActive, { 0.1f, 0.4f, 0.1f, 1.0f });

        if (ImGuiMCP::Button(GetLoc("motion.save_btn", "Save Motion Inputs"))) {
            SaveMotionsToJson();
        }

        ImGuiMCP::PopStyleColor(3);
        ImGuiMCP::EndDisabled();
        ImGuiMCP::SameLine();

        if (ImGuiMCP::Button(GetLoc("motion.add_btn", "Add New Motion"))) {
            MotionEntry newMotion;
            std::string baseName = GetLoc("motion.default_name", "New Motion");
            std::string finalName = baseName;
            int counter = 1;
            bool exists = true;
            while (exists) {
                exists = false;
                for (const auto& m : motionList) {
                    if (std::string(m.name) == finalName) {
                        exists = true;
                        break;
                    }
                }
                if (exists) {
                    finalName = baseName + " " + std::to_string(counter);
                    counter++;
                }
            }
            strncpy_s(newMotion.name, finalName.c_str(), sizeof(newMotion.name) - 1);
            motionList.push_back(newMotion);
        }
        ImGuiMCP::Spacing(); ImGuiMCP::Separator(); ImGuiMCP::Spacing();

        enum MotionMenuState { M_NONE, M_WAITING_REC, M_RECORDING, M_WAITING_TEST, M_TESTING, M_TEST_DONE };
        static MotionMenuState m_state = M_NONE;
        static int m_activeIndex = -1;
        static bool m_activePad = false;
        static std::chrono::steady_clock::time_point m_uiStartTime;

        auto keyMgr = PluginLogic::KeyManager::GetSingleton();
        bool isCapturing = (m_state != M_NONE && m_activeIndex >= 0 && m_activeIndex < motionList.size());

        if (isCapturing) {
            ImGuiMCP::TextColored({ 1.0f, 0.5f, 0.0f, 1.0f }, "%s", GetLoc("motion.capture_mode", ">>> MOTION CAPTURE/TEST MODE <<<"));
            auto& motion = motionList[m_activeIndex];
            float maxT = motion.timeWindow;

            if (m_state == M_WAITING_REC || m_state == M_WAITING_TEST) {
                ImGuiMCP::Text("%s", GetLoc("motion.press_enter", "Prepare yourself and press [ENTER] to START."));
                if (ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_Enter)) {
                    m_uiStartTime = std::chrono::steady_clock::now();
                    if (m_state == M_WAITING_REC) {
                        keyMgr->StartMotionRecording(m_activeIndex, m_activePad);
                        m_state = M_RECORDING;
                    }
                    else {
                        keyMgr->StartMotionTesting(m_activeIndex);
                        m_state = M_TESTING;
                    }
                }
                ImGuiMCP::Spacing();
                if (ImGuiMCP::Button(GetLoc("common.cancel", "Cancel"))) { m_state = M_NONE; m_activeIndex = -1; }
            }
            else if (m_state == M_RECORDING) {
                ImGuiMCP::TextColored({ 1.0f, 0.2f, 0.2f, 1.0f }, "%s", GetLoc("motion.recording", ">>> RECORDING IN PROGRESS <<<"));
                float elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - m_uiStartTime).count();
                float prog = std::min(elapsed / maxT, 1.0f);

                char buf[32]; snprintf(buf, sizeof(buf), "%.1f / %.1fs", elapsed, maxT);
                ImGuiMCP::ProgressBar(prog, { -1.0f, 0.0f }, buf);
                ImGuiMCP::Spacing();

                auto seq = keyMgr->GetRecordedMotion();
                if (seq.size() > 20) seq.resize(20);
                std::string s = GetLoc("motion.captured", "Captured: ");
                for (auto k : seq) s += "[" + FormatMotionKey(k) + "] ";
                ImGuiMCP::TextWrapped("%s", s.c_str());

                if (!keyMgr->IsRecordingMotion() || seq.size() >= 20) {
                    if (m_activePad) motion.padSequence = seq; else motion.pcSequence = seq;
                    if (keyMgr->IsRecordingMotion()) keyMgr->StopMotionRecording();
                    m_state = M_NONE; m_activeIndex = -1;
                }
            }
            else if (m_state == M_TESTING) {
                ImGuiMCP::TextColored({ 0.2f, 1.0f, 1.0f, 1.0f }, "%s", GetLoc("motion.testing", ">>> TESTING IN PROGRESS <<<"));
                ImGuiMCP::Text("%s", GetLoc("motion.execute_now", "Execute the sequence now!"));

                float elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - m_uiStartTime).count();
                float prog = std::min(elapsed / maxT, 1.0f);

                char buf[32]; snprintf(buf, sizeof(buf), "%.1f / %.1fs", elapsed, maxT);
                ImGuiMCP::ProgressBar(prog, { -1.0f, 0.0f }, buf);

                if (keyMgr->GetMotionTestSuccess() || !keyMgr->IsTestingMotion()) m_state = M_TEST_DONE;
            }
            else if (m_state == M_TEST_DONE) {
                if (keyMgr->GetMotionTestSuccess()) {
                    ImGuiMCP::TextColored({ 0.2f, 1.0f, 0.2f, 1.0f }, "%s", GetLoc("motion.success", "SUCCESS! Motion executed correctly!"));
                }
                else {
                    ImGuiMCP::TextColored({ 1.0f, 0.2f, 0.2f, 1.0f }, "%s", GetLoc("motion.failed", "FAILED! You ran out of time or pressed wrong buttons."));
                }
                ImGuiMCP::Spacing();
                if (ImGuiMCP::Button(GetLoc("common.close", "Close"))) {
                    keyMgr->ResetMotionTest();
                    m_state = M_NONE; m_activeIndex = -1;
                }
            }

            ImGuiMCP::Spacing(); ImGuiMCP::Separator(); ImGuiMCP::Spacing();
            ImGuiMCP::BeginDisabled();
        }

        auto motionListeners = PluginLogic::KeyManager::GetSingleton()->GetMotionListeners();
        for (size_t i = 0; i < motionList.size(); ++i) {
            auto& motion = motionList[i];
            ImGuiMCP::PushID(static_cast<int>(i));

            std::string headerLabel = "[" + std::to_string(i) + "] " + std::string(motion.name) + "###motionHeader_" + std::to_string(i);

            if (ImGuiMCP::CollapsingHeader(headerLabel.c_str())) {
                ImGuiMCP::Indent();
                ImGuiMCP::Spacing();
                ImGuiMCP::InputText(GetLoc("common.name", "Name"), motion.name, sizeof(motion.name));

                ImGuiMCP::Spacing();
                int currentId = static_cast<int>(i);
                ImGuiMCP::PushItemWidth(200.0f);
                if (ImGuiMCP::InputInt(GetLoc("common.id", "ID"), &currentId, 1, 10, ImGuiMCP::ImGuiInputTextFlags_EnterReturnsTrue)) {
                    if (currentId >= 0 && currentId < static_cast<int>(motionList.size()) && currentId != i) {
                        std::swap(motionList[i], motionList[currentId]);
                    }
                }
                ImGuiMCP::PopItemWidth();
                if (ImGuiMCP::IsItemHovered()) ImGuiMCP::SetTooltip("%s", GetLoc("common.swap_id_tt", "Type ID and press ENTER to swap."));
                ImGuiMCP::SameLine();

                ImGuiMCP::BeginDisabled(i == 0);
                if (ImGuiMCP::Button(GetLoc("common.move_up", "Move Up"))) std::swap(motionList[i], motionList[i - 1]);
                ImGuiMCP::EndDisabled();
                ImGuiMCP::SameLine();

                ImGuiMCP::BeginDisabled(i == motionList.size() - 1);
                if (ImGuiMCP::Button(GetLoc("common.move_down", "Move Down"))) std::swap(motionList[i], motionList[i + 1]);
                ImGuiMCP::EndDisabled();

                ImGuiMCP::Spacing();
                ImGuiMCP::PushItemWidth(200.0f);
                ImGuiMCP::SliderFloat(GetLoc("motion.time_window", "Time Window (s)"), &motion.timeWindow, 0.5f, 4.0f, "%.1f");
                ImGuiMCP::PopItemWidth();
                ImGuiMCP::Spacing();

                ImGuiMCP::TextColored({ 0.5f, 1.0f, 0.5f, 1.0f }, "%s", GetLoc("motion.pc_seq", "PC Sequence:"));
                std::string pcStr = "";
                for (uint32_t k : motion.pcSequence) pcStr += "[" + FormatMotionKey(k) + "] ";
                ImGuiMCP::TextWrapped("%s", pcStr.empty() ? GetLoc("common.none", "None") : pcStr.c_str());

                if (ImGuiMCP::Button(GetLoc("motion.rec_pc", "Record PC"))) {
                    m_activeIndex = static_cast<int>(i); m_activePad = false; m_state = M_WAITING_REC;
                }
                ImGuiMCP::SameLine();
                if (ImGuiMCP::Button(GetLoc("motion.test_pc", "Test PC"))) {
                    m_activeIndex = static_cast<int>(i); m_activePad = false; m_state = M_WAITING_TEST;
                }
                ImGuiMCP::Spacing();

                ImGuiMCP::TextColored({ 0.5f, 1.0f, 0.5f, 1.0f }, "%s", GetLoc("motion.pad_seq", "Gamepad Sequence:"));
                std::string padStr = "";
                for (uint32_t k : motion.padSequence) padStr += "[" + FormatMotionKey(k) + "] ";
                ImGuiMCP::TextWrapped("%s", padStr.empty() ? GetLoc("common.none", "None") : padStr.c_str());

                if (ImGuiMCP::Button(GetLoc("motion.rec_pad", "Record Gamepad"))) {
                    m_activeIndex = static_cast<int>(i); m_activePad = true; m_state = M_WAITING_REC;
                }
                ImGuiMCP::SameLine();
                if (ImGuiMCP::Button(GetLoc("motion.test_pad", "Test Gamepad"))) {
                    m_activeIndex = static_cast<int>(i); m_activePad = true; m_state = M_WAITING_TEST;
                }

                std::vector<PluginLogic::ModListener> activeListeners;
                for (const auto& l : motionListeners) if (l.actionID == static_cast<int>(i)) activeListeners.push_back(l);

                if (!activeListeners.empty()) {
                    ImGuiMCP::Spacing(); ImGuiMCP::Separator(); ImGuiMCP::Spacing();
                    ImGuiMCP::TextColored({ 0.9f, 0.6f, 1.0f, 1.0f }, "%s", GetLoc("common.connected_mods", "Mods Connected:"));
                    for (const auto& listener : activeListeners) {
                        ImGuiMCP::BulletText("Mod: '%s'  |  Purpose: '%s'", listener.modName.c_str(), listener.purpose.c_str());
                    }
                }

                ImGuiMCP::Spacing(); ImGuiMCP::Separator(); ImGuiMCP::Spacing();
                ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Button, { 0.6f, 0.2f, 0.2f, 1.0f });
                if (ImGuiMCP::Button(GetLoc("common.delete", "Delete"))) {
                    motionList.erase(motionList.begin() + i);
                    ImGuiMCP::PopStyleColor();
                    ImGuiMCP::Unindent();
                    ImGuiMCP::PopID();
                    break;
                }
                ImGuiMCP::PopStyleColor();

                ImGuiMCP::Unindent();
            }
            ImGuiMCP::PopID();
        }
        if (isCapturing) ImGuiMCP::EndDisabled();
    }

    inline void RenderSummaryDashboard(const std::vector<PluginLogic::ModListener>& listeners, const std::vector<PluginLogic::ModListener>& motionListeners) {
        ImGuiMCP::TextColored({ 0.5f, 1.0f, 0.5f, 1.0f }, "%s", GetLoc("dash.title", "Keys and Combos in Use"));
        ImGuiMCP::TextDisabled("%s", GetLoc("dash.desc", "Overview grouped by physical key for easy identification."));
        ImGuiMCP::Spacing();

        struct DashboardEntry { std::vector<int> actionIDs; std::vector<int> motionIDs; };
        std::vector<std::string> uniqueMods;
        auto addUniqueMod = [&](const std::string& modName) {
            if (std::find(uniqueMods.begin(), uniqueMods.end(), modName) == uniqueMods.end()) uniqueMods.push_back(modName);
            };

        for (const auto& l : listeners) addUniqueMod(l.modName);
        for (const auto& l : motionListeners) addUniqueMod(l.modName);

        static char dashFilterName[128] = "";
        static std::string dashSelectedKeyFilter = "";
        static char dashKeySearchBuf[128] = "";
        static std::string dashSelectedModFilter = "";
        static char dashModSearchBuf[128] = "";

        ImGuiMCP::TextColored({ 0.5f, 0.8f, 1.0f, 1.0f }, "%s", GetLoc("common.filters", "Search Filters:"));
        ImGuiMCP::PushItemWidth(180.0f);

        ImGuiMCP::InputText(GetLoc("common.name", "Name"), dashFilterName, sizeof(dashFilterName));
        ImGuiMCP::SameLine();

        if (ImGuiMCP::BeginCombo(GetLoc("common.key", "Key"), dashSelectedKeyFilter.empty() ? GetLoc("dash.all_keys", "All Keys") : dashSelectedKeyFilter.c_str())) {
            ImGuiMCP::InputText("Search##dashKeySearch", dashKeySearchBuf, sizeof(dashKeySearchBuf));
            std::string searchLower = ToLower(dashKeySearchBuf);
            ImGuiMCP::Separator();

            if (ImGuiMCP::Selectable(GetLoc("dash.all_keys", "All Keys"), dashSelectedKeyFilter.empty())) dashSelectedKeyFilter = "";

            for (size_t k = 1; k < std::size(pcKeyNames); ++k) {
                std::string kName = pcKeyNames[k];
                if (searchLower.empty() || ToLower(kName).find(searchLower) != std::string::npos) {
                    if (ImGuiMCP::Selectable((kName + " (PC)").c_str(), dashSelectedKeyFilter == kName + " (PC)")) dashSelectedKeyFilter = kName + " (PC)";
                }
            }
            for (size_t k = 1; k < std::size(gamepadKeyNames); ++k) {
                std::string kName = gamepadKeyNames[k];
                if (searchLower.empty() || ToLower(kName).find(searchLower) != std::string::npos) {
                    if (ImGuiMCP::Selectable((kName + " (PAD)").c_str(), dashSelectedKeyFilter == kName + " (PAD)")) dashSelectedKeyFilter = kName + " (PAD)";
                }
            }
            ImGuiMCP::EndCombo();
        }
        ImGuiMCP::SameLine();

        if (ImGuiMCP::BeginCombo(GetLoc("common.mod", "Mod"), dashSelectedModFilter.empty() ? GetLoc("dash.all_mods", "All Mods") : dashSelectedModFilter.c_str())) {
            ImGuiMCP::InputText("Search##dashModSearch", dashModSearchBuf, sizeof(dashModSearchBuf));
            std::string modSearchLower = ToLower(dashModSearchBuf);
            ImGuiMCP::Separator();

            if (ImGuiMCP::Selectable(GetLoc("dash.all_mods", "All Mods"), dashSelectedModFilter.empty())) dashSelectedModFilter = "";
            if (uniqueMods.empty()) {
                ImGuiMCP::TextDisabled("%s", GetLoc("dash.no_mods", "No mods connected at the moment"));
            }
            else {
                for (const auto& modName : uniqueMods) {
                    if (modSearchLower.empty() || ToLower(modName).find(modSearchLower) != std::string::npos) {
                        if (ImGuiMCP::Selectable(modName.c_str(), dashSelectedModFilter == modName)) dashSelectedModFilter = modName;
                    }
                }
            }
            ImGuiMCP::EndCombo();
        }
        ImGuiMCP::PopItemWidth();

        ImGuiMCP::Spacing(); ImGuiMCP::Separator(); ImGuiMCP::Spacing();

        std::map<std::string, DashboardEntry> keyUsageMap;
        std::string lowerDashFilterName = ToLower(dashFilterName);

        for (size_t i = 0; i < actionList.size(); ++i) {
            const auto& action = actionList[i];
            if (action.pcMainKey == 0 && action.gamepadMainKey == 0) continue;
            if (!lowerDashFilterName.empty() && ToLower(action.name).find(lowerDashFilterName) == std::string::npos) continue;

            bool modMatch = dashSelectedModFilter.empty();
            if (!dashSelectedModFilter.empty()) {
                for (const auto& l : listeners) {
                    if (l.actionID == static_cast<int>(i) && l.modName == dashSelectedModFilter) { modMatch = true; break; }
                }
            }
            if (!modMatch) continue;

            auto addKey = [&](const std::string& keyName) {
                if (!keyName.empty() && keyName != "None") {
                    if (!dashSelectedKeyFilter.empty() && keyName != dashSelectedKeyFilter) return;
                    auto& vec = keyUsageMap[keyName].actionIDs;
                    if (std::find(vec.begin(), vec.end(), static_cast<int>(i)) == vec.end()) vec.push_back(static_cast<int>(i));
                }
                };

            if (action.pcMainKey != 0) addKey(std::string(pcKeyNames[GetIndexFromID(action.pcMainKey, pcKeyIDs, std::size(pcKeyIDs))]) + " (PC)");
            if (action.pcModifierKey != 0) addKey(std::string(pcKeyNames[GetIndexFromID(action.pcModifierKey, pcKeyIDs, std::size(pcKeyIDs))]) + " (PC)");
            if (action.gamepadMainKey != 0) addKey(std::string(gamepadKeyNames[GetIndexFromID(action.gamepadMainKey, gamepadKeyIDs, std::size(gamepadKeyIDs))]) + " (PAD)");
            if (action.gamepadModifierKey != 0) addKey(std::string(gamepadKeyNames[GetIndexFromID(action.gamepadModifierKey, gamepadKeyIDs, std::size(gamepadKeyIDs))]) + " (PAD)");
        }

        for (size_t i = 0; i < motionList.size(); ++i) {
            const auto& motion = motionList[i];
            if (motion.pcSequence.empty() && motion.padSequence.empty()) continue;
            if (!lowerDashFilterName.empty() && ToLower(motion.name).find(lowerDashFilterName) == std::string::npos) continue;

            bool modMatch = dashSelectedModFilter.empty();
            if (!dashSelectedModFilter.empty()) {
                for (const auto& l : motionListeners) {
                    if (l.actionID == static_cast<int>(i) && l.modName == dashSelectedModFilter) { modMatch = true; break; }
                }
            }
            if (!modMatch) continue;

            auto addMotionKey = [&](uint32_t keyID, bool isPad) {
                std::string keyName = FormatMotionKey(keyID) + (isPad ? " (PAD)" : " (PC)");
                if (!dashSelectedKeyFilter.empty() && keyName != dashSelectedKeyFilter) return;
                auto& vec = keyUsageMap[keyName].motionIDs;
                if (std::find(vec.begin(), vec.end(), static_cast<int>(i)) == vec.end()) vec.push_back(static_cast<int>(i));
                };

            for (uint32_t k : motion.pcSequence) addMotionKey(k, false);
            for (uint32_t k : motion.padSequence) addMotionKey(k, true);
        }

        if (keyUsageMap.empty()) {
            ImGuiMCP::TextDisabled("%s", GetLoc("dash.no_results", "No results match the selected filters."));
            return;
        }

        if (ImGuiMCP::BeginTable("SummaryTable", 4, ImGuiMCP::ImGuiTableFlags_Borders | ImGuiMCP::ImGuiTableFlags_RowBg | ImGuiMCP::ImGuiTableFlags_Resizable)) {
            ImGuiMCP::TableSetupColumn(GetLoc("dash.col_action", "Action / ID"), ImGuiMCP::ImGuiTableColumnFlags_WidthFixed, 180.0f);
            ImGuiMCP::TableSetupColumn(GetLoc("dash.col_keys", "Keys and Combos"), ImGuiMCP::ImGuiTableColumnFlags_WidthStretch);
            ImGuiMCP::TableSetupColumn(GetLoc("dash.col_timings", "Timings"), ImGuiMCP::ImGuiTableColumnFlags_WidthFixed, 130.0f);
            ImGuiMCP::TableSetupColumn(GetLoc("common.connected_mods", "Connected Mods"), ImGuiMCP::ImGuiTableColumnFlags_WidthStretch);
            ImGuiMCP::TableHeadersRow();

            for (const auto& pair : keyUsageMap) {
                const std::string& keyName = pair.first;
                ImGuiMCP::TableNextRow();
                ImGuiMCP::TableSetColumnIndex(0);

                ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, { 0.8f, 0.8f, 1.0f, 1.0f });
                bool isNodeOpen = ImGuiMCP::TreeNodeEx(keyName.c_str(), ImGuiMCP::ImGuiTreeNodeFlags_SpanFullWidth | ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen);
                ImGuiMCP::PopStyleColor();

                if (isNodeOpen) {
                    for (int actionID : pair.second.actionIDs) {
                        const auto& action = actionList[actionID];
                        ImGuiMCP::TableNextRow();
                        ImGuiMCP::TableSetColumnIndex(0);
                        ImGuiMCP::Indent();
                        ImGuiMCP::TextColored({ 0.7f, 0.7f, 0.7f, 1.0f }, "ID: %d - %s", actionID, action.name);
                        ImGuiMCP::Unindent();

                        ImGuiMCP::TableSetColumnIndex(1);
                        ImGuiMCP::TextWrapped("%s", GetActionSummary(action).c_str());

                        ImGuiMCP::TableSetColumnIndex(2);
                        ImGuiMCP::Text("%s: %.2fs\n%s: %.2fs", GetLoc("action.tap_window", "Window"), action.tapWindow, GetLoc("action.hold_time", "Hold"), action.holdDuration);

                        ImGuiMCP::TableSetColumnIndex(3);
                        bool hasMod = false;
                        for (const auto& l : listeners) {
                            if (l.actionID == actionID) {
                                ImGuiMCP::BulletText("%s (%s)", l.modName.c_str(), l.purpose.c_str());
                                hasMod = true;
                            }
                        }
                        if (!hasMod) ImGuiMCP::TextDisabled("%s", GetLoc("common.none", "None"));
                    }

                    for (int motionID : pair.second.motionIDs) {
                        const auto& motion = motionList[motionID];
                        ImGuiMCP::TableNextRow();
                        ImGuiMCP::TableSetColumnIndex(0);
                        ImGuiMCP::Indent();
                        ImGuiMCP::TextColored({ 0.9f, 0.7f, 0.3f, 1.0f }, "%s: %d", GetLoc("common.motion_id", "Motion ID"), motionID);
                        ImGuiMCP::TextColored({ 0.7f, 0.7f, 0.7f, 1.0f }, "%s", motion.name);
                        ImGuiMCP::Unindent();

                        ImGuiMCP::TableSetColumnIndex(1);
                        std::string seqSummary = "";
                        if (!motion.pcSequence.empty()) {
                            seqSummary += GetLoc("common.pc", "PC") + std::string(": ");
                            for (uint32_t k : motion.pcSequence) seqSummary += "[" + FormatMotionKey(k) + "] ";
                        }
                        if (!motion.padSequence.empty()) {
                            if (!seqSummary.empty()) seqSummary += "  |  ";
                            seqSummary += GetLoc("common.pad", "PAD") + std::string(": ");
                            for (uint32_t k : motion.padSequence) seqSummary += "[" + FormatMotionKey(k) + "] ";
                        }
                        ImGuiMCP::TextWrapped("%s", seqSummary.empty() ? GetLoc("common.none", "None") : seqSummary.c_str());

                        ImGuiMCP::TableSetColumnIndex(2);
                        ImGuiMCP::Text("%s: %.2fs", GetLoc("action.tap_window", "Window"), motion.timeWindow);

                        ImGuiMCP::TableSetColumnIndex(3);
                        bool hasMod = false;
                        for (const auto& l : motionListeners) {
                            if (l.actionID == motionID) {
                                ImGuiMCP::BulletText("%s (%s)", l.modName.c_str(), l.purpose.c_str());
                                hasMod = true;
                            }
                        }
                        if (!hasMod) ImGuiMCP::TextDisabled("%s", GetLoc("common.none", "None"));
                    }
                    ImGuiMCP::TreePop();
                }
            }
            ImGuiMCP::EndTable();
        }
    }

    inline void RenderDashboardMenu() {
        ImGuiMCP::Text("%s", GetLoc("dash.header", "Input Manager - Dashboard"));
        ImGuiMCP::Separator(); ImGuiMCP::Spacing();
        auto listeners = PluginLogic::KeyManager::GetSingleton()->GetListeners();
        auto motionListeners = PluginLogic::KeyManager::GetSingleton()->GetMotionListeners();
        RenderSummaryDashboard(listeners, motionListeners);
    }

    inline void RenderGesturesMenu() {
        ImGuiMCP::Text("%s", GetLoc("gesture.title", "Input Manager - Gestures and Movements (Beta)"));
        ImGuiMCP::Separator(); ImGuiMCP::Spacing();

        bool hasGestureConflict = false;
        std::string gestureErrorMsg;
        for (size_t i = 0; i < movementList.size(); ++i) {
            for (size_t j = i + 1; j < movementList.size(); ++j) {
                if (SanitizeFileName(movementList[i].name) == SanitizeFileName(movementList[j].name)) {
                    hasGestureConflict = true;
                    gestureErrorMsg = GetLoc("error.dup_gesture", "DUPLICATE NAME: Gestures conflict. Please choose unique names.");
                    break;
                }
            }
            if (hasGestureConflict) break;
        }

        if (hasGestureConflict) {
            ImGuiMCP::TextColored({ 1.0f, 0.2f, 0.2f, 1.0f }, "%s", GetLoc("error.conflict_title", "[CONFIGURATION ERROR]"));
            ImGuiMCP::TextColored({ 1.0f, 0.8f, 0.2f, 1.0f }, "%s", gestureErrorMsg.c_str());
            ImGuiMCP::TextColored({ 0.7f, 0.7f, 0.7f, 1.0f }, "%s", GetLoc("error.resolve_gesture", "Resolve the duplication by changing the gesture's name."));
            ImGuiMCP::Spacing(); ImGuiMCP::Separator(); ImGuiMCP::Spacing();
        }

        ImGuiMCP::BeginDisabled(hasGestureConflict);
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Button, { 0.1f, 0.5f, 0.1f, 1.0f });
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_ButtonHovered, { 0.2f, 0.6f, 0.2f, 1.0f });
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_ButtonActive, { 0.1f, 0.4f, 0.1f, 1.0f });

        if (ImGuiMCP::Button(GetLoc("gesture.save_btn", "Save and Apply Gestures"))) {
            SaveSettingsToJson();
            SaveGesturesToJson();
        }

        ImGuiMCP::PopStyleColor(3);
        ImGuiMCP::EndDisabled();

        ImGuiMCP::SameLine();
        if (ImGuiMCP::Button(GetLoc("gesture.add_btn", "Add New Gesture"))) {
            MovementEntry newGesture;
            std::string baseName = GetLoc("gesture.default_name", "New Gesture");
            std::string finalName = baseName;
            int counter = 1;

            bool exists = true;
            while (exists) {
                exists = false;
                for (const auto& g : movementList) {
                    if (std::string(g.name) == finalName) { exists = true; break; }
                }
                if (exists) {
                    finalName = baseName + " " + std::to_string(counter);
                    counter++;
                }
            }
            strncpy_s(newGesture.name, finalName.c_str(), sizeof(newGesture.name) - 1);
            movementList.push_back(newGesture);
        }
        ImGuiMCP::Spacing(); ImGuiMCP::Separator(); ImGuiMCP::Spacing();

        ImGuiMCP::Checkbox(GetLoc("gesture.show_trail", "Show Gesture Trail in Game (Overlay)"), &showGestureTrail);

        ImGuiMCP::Spacing(); ImGuiMCP::Separator(); ImGuiMCP::Spacing();

        static int recordingIndex = -1;
        static int testingIndex = -1;
        static bool isWaitingForEnter = false;
        static std::vector<GestureMath::Point2D> tempPoints;
        static float lastScore = -1.0f;

        bool isCapturing = (recordingIndex != -1 || testingIndex != -1);

        if (isCapturing) {
            ImGuiMCP::TextColored({ 1.0f, 0.5f, 0.0f, 1.0f }, "%s", GetLoc("gesture.capture_mode", ">>> GESTURE CAPTURE MODE <<<"));

            if (isWaitingForEnter) {
                ImGuiMCP::Text("%s", GetLoc("gesture.press_enter", "Position the mouse where desired and press [ENTER] to START recording."));
                if (ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_Enter)) isWaitingForEnter = false;
            }
            else {
                ImGuiMCP::TextColored({ 1.0f, 0.2f, 0.2f, 1.0f }, "%s", GetLoc("gesture.recording", "[RECORDING] Move the mouse. Press [ENTER] again to FINISH."));

                auto io = ImGuiMCP::GetIO();
                float mouseX = io->MousePos.x;
                float mouseY = io->MousePos.y;

                if (tempPoints.empty()) tempPoints.push_back({ mouseX, mouseY });
                else {
                    float dx = mouseX - tempPoints.back().x;
                    float dy = mouseY - tempPoints.back().y;
                    if (dx * dx + dy * dy > 4.0f) tempPoints.push_back({ mouseX, mouseY });
                }

                if (tempPoints.size() > 1) {
                    ImGuiMCP::ImVec2 oldPos; ImGuiMCP::GetCursorScreenPos(&oldPos);
                    ImGuiMCP::ImDrawList* draw_list = ImGuiMCP::GetWindowDrawList();
                    size_t totalPoints = tempPoints.size();
                    for (size_t i = 1; i < totalPoints; ++i) {
                        ImGuiMCP::ImVec2 p1 = { tempPoints[i - 1].x, tempPoints[i - 1].y };
                        ImGuiMCP::ImVec2 p2 = { tempPoints[i].x, tempPoints[i].y };
                        float progress = (float)i / (totalPoints - 1);
                        ImGuiMCP::ImU32 lineColor = ImGuiMCP::GetColorU32({ progress, 1.0f - progress, 0.0f, 1.0f });
                        ImGuiMCP::ImDrawListManager::AddLine(draw_list, p1, p2, lineColor, 3.0f);
                    }
                    ImGuiMCP::SetCursorScreenPos(oldPos);
                }

                if (ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_Enter)) {
                    if (recordingIndex != -1) {
                        movementList[recordingIndex].rawPoints = tempPoints;
                        movementList[recordingIndex].normalizedPoints = GestureMath::NormalizeGesture(tempPoints);
                    }
                    else if (testingIndex != -1) {
                        std::vector<GestureMath::Point2D> testNorm = GestureMath::NormalizeGesture(tempPoints);
                        lastScore = GestureMath::GetMatchScore(movementList[testingIndex].normalizedPoints, testNorm);
                    }
                    recordingIndex = -1; testingIndex = -1; tempPoints.clear();
                }
            }
            ImGuiMCP::BeginDisabled();
        }

        for (size_t i = 0; i < movementList.size(); ++i) {
            auto& gesture = movementList[i];
            ImGuiMCP::PushID(static_cast<int>(i));
            std::string headerLabel = "[" + std::to_string(i) + "] " + std::string(gesture.name) + "###gestureHeader_" + std::to_string(i);

            ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Header, { 0.2f, 0.2f, 0.3f, 1.0f });

            if (ImGuiMCP::CollapsingHeader(headerLabel.c_str())) {
                ImGuiMCP::PopStyleColor();
                ImGuiMCP::Indent(); ImGuiMCP::Spacing();

                ImGuiMCP::InputText(GetLoc("common.name", "Name"), gesture.name, sizeof(gesture.name));
                ImGuiMCP::Spacing();

                int currentId = static_cast<int>(i);
                ImGuiMCP::PushItemWidth(200.0f);
                if (ImGuiMCP::InputInt(GetLoc("common.id", "ID"), &currentId, 1, 10, ImGuiMCP::ImGuiInputTextFlags_EnterReturnsTrue)) {
                    if (currentId >= 0 && currentId < static_cast<int>(movementList.size()) && currentId != i) std::swap(movementList[i], movementList[currentId]);
                }
                ImGuiMCP::PopItemWidth();
                ImGuiMCP::SameLine();

                ImGuiMCP::BeginDisabled(i == 0);
                if (ImGuiMCP::Button(GetLoc("common.move_up", "Move Up"))) std::swap(movementList[i], movementList[i - 1]);
                ImGuiMCP::EndDisabled();
                ImGuiMCP::SameLine();

                ImGuiMCP::BeginDisabled(i == movementList.size() - 1);
                if (ImGuiMCP::Button(GetLoc("common.move_down", "Move Down"))) std::swap(movementList[i], movementList[i + 1]);
                ImGuiMCP::EndDisabled();

                ImGuiMCP::Spacing();
                ImGuiMCP::SliderFloat(GetLoc("gesture.accuracy", "Required Accuracy"), &gesture.requiredAccuracy, 0.50f, 0.99f, "%.2f");
                ImGuiMCP::Spacing();

                ImGuiMCP::Text("%s", GetLoc("gesture.preview", "Recorded Drawing (Preview):"));
                if (!gesture.normalizedPoints.empty()) {
                    ImGuiMCP::ImVec2 p0; ImGuiMCP::GetCursorScreenPos(&p0);
                    ImGuiMCP::Dummy({ 240.0f, 240.0f });
                    ImGuiMCP::ImVec2 pEnd; ImGuiMCP::GetCursorScreenPos(&pEnd);
                    float minX = 99999.0f, maxX = -99999.0f, minY = 99999.0f, maxY = -99999.0f;

                    for (const auto& p : gesture.normalizedPoints) {
                        if (p.x < minX) minX = p.x; if (p.x > maxX) maxX = p.x;
                        if (p.y < minY) minY = p.y; if (p.y > maxY) maxY = p.y;
                    }

                    float rangeX = std::max(maxX - minX, 0.001f);
                    float rangeY = std::max(maxY - minY, 0.001f);
                    float maxRange = std::max(rangeX, rangeY);
                    float scale = 160.0f / maxRange;

                    float centerX = p0.x + 120.0f; float centerY = p0.y + 120.0f;
                    float midX = minX + (rangeX * 0.5f); float midY = minY + (rangeY * 0.5f);

                    size_t totalPoints = gesture.normalizedPoints.size();
                    if (totalPoints > 1) {
                        ImGuiMCP::ImDrawList* draw_list = ImGuiMCP::GetWindowDrawList();
                        for (size_t j = 1; j < totalPoints; ++j) {
                            auto p1 = gesture.normalizedPoints[j - 1]; auto p2 = gesture.normalizedPoints[j];
                            float x1 = centerX + ((p1.x - midX) * scale); float y1 = centerY + ((p1.y - midY) * scale);
                            float x2 = centerX + ((p2.x - midX) * scale); float y2 = centerY + ((p2.y - midY) * scale);

                            float progress = (float)j / (totalPoints - 1);
                            ImGuiMCP::ImU32 lineColor = ImGuiMCP::GetColorU32({ progress, 1.0f - progress, 0.0f, 1.0f });
                            ImGuiMCP::ImDrawListManager::AddLine(draw_list, { x1, y1 }, { x2, y2 }, lineColor, 3.0f);
                        }
                    }
                    ImGuiMCP::SetCursorScreenPos(pEnd);
                }
                else {
                    ImGuiMCP::TextColored({ 0.6f, 0.6f, 0.6f, 1.0f }, "%s", GetLoc("gesture.no_record", "[ NO GESTURE RECORDED ]"));
                }

                ImGuiMCP::Spacing(); ImGuiMCP::Separator(); ImGuiMCP::Spacing();

                if (ImGuiMCP::Button(GetLoc("gesture.rec_btn", "Record Movement"))) {
                    recordingIndex = static_cast<int>(i); testingIndex = -1; tempPoints.clear(); lastScore = -1.0f; isWaitingForEnter = true;
                }
                ImGuiMCP::SameLine();
                if (ImGuiMCP::Button(GetLoc("gesture.test_btn", "Validate Movement (Test)"))) {
                    testingIndex = static_cast<int>(i); recordingIndex = -1; tempPoints.clear(); lastScore = -1.0f; isWaitingForEnter = true;
                }

                if (lastScore >= 0.0f && testingIndex == -1 && recordingIndex == -1) {
                    ImGuiMCP::Spacing();
                    if (lastScore >= gesture.requiredAccuracy) {
                        ImGuiMCP::TextColored({ 0.2f, 1.0f, 0.2f, 1.0f }, "Last Test: %.1f%% Accuracy (Requires: %.1f%%)", lastScore * 100.0f, gesture.requiredAccuracy * 100.0f);
                    }
                    else {
                        ImGuiMCP::TextColored({ 1.0f, 0.2f, 0.2f, 1.0f }, "Last Test: %.1f%% Accuracy (Requires: %.1f%%)", lastScore * 100.0f, gesture.requiredAccuracy * 100.0f);
                    }
                }

                ImGuiMCP::Spacing(); ImGuiMCP::Separator(); ImGuiMCP::Spacing();
                ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Button, { 0.6f, 0.2f, 0.2f, 1.0f });
                if (ImGuiMCP::Button(GetLoc("common.delete", "Delete"))) {
                    movementList.erase(movementList.begin() + i);
                    ImGuiMCP::PopStyleColor();
                    ImGuiMCP::Unindent();
                    ImGuiMCP::PopID();
                    break;
                }
                ImGuiMCP::PopStyleColor();
                ImGuiMCP::Unindent(); ImGuiMCP::Spacing();
            }
            else { ImGuiMCP::PopStyleColor(); }
            ImGuiMCP::PopID();
        }
        if (isCapturing) ImGuiMCP::EndDisabled();
    }

    inline bool HasConflicts(std::string& outErrorMsg) {
        for (size_t i = 0; i < actionList.size(); ++i) {
            for (size_t j = i + 1; j < actionList.size(); ++j) {
                const auto& a = actionList[i];
                const auto& b = actionList[j];

                if (SanitizeFileName(a.name) == SanitizeFileName(b.name)) {
                    outErrorMsg = "DUPLICATE NAME: Actions '" + std::string(a.name) + "' and '" + std::string(b.name) + "' will generate the same file. Please choose unique names.";
                    return true;
                }
                auto CheckComboConflict = [](
                    int aMKey, int aMAct, int aMTap, int aModKey, int aModAct, int aModTap, int aGest,
                    int bMKey, int bMAct, int bMTap, int bModKey, int bModAct, int bModTap, int bGest) {

                        if (aMKey == 0 || bMKey == 0) return false;

                        struct K { int key; int act; int tap; int gest; };

                        auto make_key = [](int key, int act, int tap, int gest) -> K {
                            if (act == 0) return { 0, 0, 0, -1 }; // Ignore
                            int normAct = (act == 4) ? 2 : act; // Normaliza Press (4) para Hold (2)
                            if (normAct == 3) return { 0, 3, 0, gest }; // Gesto ignora a tecla real na checagem cruzada
                            return { key, normAct, (normAct == 1) ? tap : 1, -1 };
                            };

                        K a1 = make_key(aMKey, aMAct, aMTap, -1);
                        K a2 = make_key(aModKey, aModAct, aModTap, aGest);

                        K b1 = make_key(bMKey, bMAct, bMTap, -1);
                        K b2 = make_key(bModKey, bModAct, bModTap, bGest);

                        auto match = [](K x, K y) {
                            if (x.act == 0 && y.act == 0) return true;  // Ambos ignorados
                            if (x.act == 0 || y.act == 0) return false; // Apenas um ignorado

                            if (x.act == 3 && y.act == 3) return x.gest == y.gest; // Gestos

                            return (x.key == y.key) && (x.act == y.act) && (x.tap == y.tap);
                            };

                        // Avalia Combinação Direta ou Combinação Reversa
                        return (match(a1, b1) && match(a2, b2)) || (match(a1, b2) && match(a2, b1));
                    };

                // Check PC configuration
                if (CheckComboConflict(
                    a.pcMainKey, a.pcMainAction, a.pcMainTapCount, a.pcModifierKey, a.pcModAction, a.pcModTapCount, a.gestureIndex,
                    b.pcMainKey, b.pcMainAction, b.pcMainTapCount, b.pcModifierKey, b.pcModAction, b.pcModTapCount, b.gestureIndex)) {
                    outErrorMsg = "PC CONFLICT: '" + std::string(a.name) + "' and '" + std::string(b.name) + "' use the exact same keys, states, and tap counts.";
                    return true;
                }

                // Check Gamepad configuration
                if (CheckComboConflict(
                    a.gamepadMainKey, a.gamepadMainAction, a.gamepadMainTapCount, a.gamepadModifierKey, a.gamepadModAction, a.gamepadModTapCount, a.gestureIndex,
                    b.gamepadMainKey, b.gamepadMainAction, b.gamepadMainTapCount, b.gamepadModifierKey, b.gamepadModAction, b.gamepadModTapCount, b.gestureIndex)) {
                    outErrorMsg = "GAMEPAD CONFLICT: '" + std::string(a.name) + "' and '" + std::string(b.name) + "' use the exact same keys, states, and tap counts.";
                    return true;
                }
            }
        }
        return false;
    }

    inline void EnforceSafeGuards() {
        if (advancedMode) return;

        for (size_t i = 0; i < actionList.size(); ++i) {
            auto& a = actionList[i];

            if (a.useCustomTimings && a.tapWindow > globalTapWindow) {
                a.tapWindow = globalTapWindow;
            }

            for (size_t j = 0; j < actionList.size(); ++j) {
                if (i == j) continue;
                auto& b = actionList[j];

                // REGRA 1: HOLD/PRESS CONFLITANTE COMO ÂNCORA
                if (a.pcMainKey != 0 && (a.pcMainAction == 2 || a.pcMainAction == 4) && a.pcModifierKey == 0) {
                    if ((b.pcModifierKey == a.pcMainKey) || (b.pcMainKey == a.pcMainKey && b.pcModifierKey != 0)) {
                        if (a.holdDuration < 1.0f) a.holdDuration = 1.0f;
                    }
                }
                if (a.gamepadMainKey != 0 && (a.gamepadMainAction == 2 || a.gamepadMainAction == 4) && a.gamepadModifierKey == 0) {
                    if ((b.gamepadModifierKey == a.gamepadMainKey) || (b.gamepadMainKey == a.gamepadMainKey && b.gamepadModifierKey != 0)) {
                        if (a.holdDuration < 1.0f) a.holdDuration = 1.0f;
                    }
                }

                // REGRAS 2 e 3 (Tap Delay Dinâmico Inteligente)
                auto CheckDelay = [](int aMKey, int aMAct, int aMTap, int aModKey, int aModAct, int aModTap,
                    int bMKey, int bMAct, int bMTap, int bModKey, int bModAct, int bModTap) {

                        auto getTapInfo = [](int mK, int mAct, int mTap, int modK, int modAct, int modTap, int& outAnchor) {
                            if (mAct == 1) { outAnchor = (modAct == 0) ? 0 : modK; return std::make_pair(mK, mTap); }
                            if (modAct == 1) { outAnchor = mK; return std::make_pair(modK, modTap); }
                            return std::make_pair(0, 0);
                            };

                        int aAnchor = 0, bAnchor = 0;
                        auto aTap = getTapInfo(aMKey, aMAct, aMTap, aModKey, aModAct, aModTap, aAnchor);
                        auto bTap = getTapInfo(bMKey, bMAct, bMTap, bModKey, bModAct, bModTap, bAnchor);

                        if (aTap.first != 0 && bTap.first != 0) {
                            // Se compartilham o mesmo botão de Tap E a mesma âncora
                            if (aTap.first == bTap.first && aAnchor == bAnchor) {
                                // A ação que tem a quantidade MENOR de Taps é a que precisa esperar (Delay)
                                if (aTap.second < bTap.second) return true;
                            }
                        }
                        return false;
                    };

                if (CheckDelay(a.pcMainKey, a.pcMainAction, a.pcMainTapCount, a.pcModifierKey, a.pcModAction, a.pcModTapCount,
                    b.pcMainKey, b.pcMainAction, b.pcMainTapCount, b.pcModifierKey, b.pcModAction, b.pcModTapCount)) {
                    a.pcDelayTap = true;
                }

                if (CheckDelay(a.gamepadMainKey, a.gamepadMainAction, a.gamepadMainTapCount, a.gamepadModifierKey, a.gamepadModAction, a.gamepadModTapCount,
                    b.gamepadMainKey, b.gamepadMainAction, b.gamepadMainTapCount, b.gamepadModifierKey, b.gamepadModAction, b.gamepadModTapCount)) {
                    a.gamepadDelayTap = true;
                }
            }
        }
    }

    inline void RenderMenu() {
        ImGuiMCP::Text("%s", GetLoc("action.title", "Input Manager - Actions"));
        ImGuiMCP::Separator(); ImGuiMCP::Spacing();

        ImGuiMCP::Checkbox(GetLoc("action.adv_mode", "Advanced Mode (Disable Safe Guards)"), &advancedMode);
        if (!advancedMode) {
            EnforceSafeGuards();
            ImGuiMCP::SameLine();
            ImGuiMCP::TextColored({ 0.5f, 1.0f, 0.5f, 1.0f }, "%s", GetLoc("action.safe_on", " (Safe Guards Active: Preventing logic conflicts)"));
        }
        else {
            ImGuiMCP::SameLine();
            ImGuiMCP::TextColored({ 1.0f, 0.5f, 0.5f, 1.0f }, "%s", GetLoc("action.safe_off", " (WARNING: Safe Guards Disabled. Risk of breaking!)"));
        }

        ImGuiMCP::Spacing(); ImGuiMCP::Separator(); ImGuiMCP::Spacing();

        ImGuiMCP::TextColored({ 0.8f, 0.8f, 0.5f, 1.0f }, "%s", GetLoc("action.global_timings", "Global Timings Defaults"));
        ImGuiMCP::SliderFloat(GetLoc("action.global_tap", "Global Tap Window"), &globalTapWindow, 0.01f, 1.0f, "%.2f s");
        ImGuiMCP::SliderFloat(GetLoc("action.global_hold", "Global Hold Duration"), &globalHoldDuration, 0.01f, 2.0f, "%.2f s");
        ImGuiMCP::Spacing(); ImGuiMCP::Separator(); ImGuiMCP::Spacing();

        std::string errorMsg;
        bool hasConflict = HasConflicts(errorMsg);

        if (hasConflict) {
            ImGuiMCP::TextColored({ 1.0f, 0.2f, 0.2f, 1.0f }, "%s", GetLoc("error.conflict_title", "[CONFIGURATION ERROR]"));
            ImGuiMCP::TextColored({ 1.0f, 0.8f, 0.2f, 1.0f }, "%s", errorMsg.c_str());
            ImGuiMCP::TextColored({ 0.7f, 0.7f, 0.7f, 1.0f }, "%s", GetLoc("error.resolve_action", "Resolve the duplicate by changing the key or State."));
            ImGuiMCP::Spacing(); ImGuiMCP::Separator(); ImGuiMCP::Spacing();
        }

        ImGuiMCP::BeginDisabled(hasConflict);
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Button, { 0.1f, 0.5f, 0.1f, 1.0f });
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_ButtonHovered, { 0.2f, 0.6f, 0.2f, 1.0f });
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_ButtonActive, { 0.1f, 0.4f, 0.1f, 1.0f });

        if (ImGuiMCP::Button(GetLoc("action.save_btn", "Save and Apply Changes"))) {
            SaveSettingsToJson();
            SaveActionsToJson();
        }
        ImGuiMCP::PopStyleColor(3);
        ImGuiMCP::EndDisabled();

        ImGuiMCP::SameLine();
        if (ImGuiMCP::Button(GetLoc("action.add_btn", "Add New Input"))) {
            ActionEntry newAction;
            std::string baseName = GetLoc("action.default_name", "New Input");
            std::string finalName = baseName;
            int counter = 1;
            bool exists = true;
            while (exists) {
                exists = false;
                for (const auto& a : actionList) {
                    if (std::string(a.name) == finalName) { exists = true; break; }
                }
                if (exists) {
                    finalName = baseName + " " + std::to_string(counter);
                    counter++;
                }
            }
            strncpy_s(newAction.name, finalName.c_str(), sizeof(newAction.name) - 1);
            actionList.push_back(newAction);
        }
        ImGuiMCP::Spacing(); ImGuiMCP::Separator(); ImGuiMCP::Spacing();

        auto listeners = PluginLogic::KeyManager::GetSingleton()->GetListeners();
        std::vector<std::string> uniqueMods;
        for (const auto& l : listeners) {
            if (std::find(uniqueMods.begin(), uniqueMods.end(), l.modName) == uniqueMods.end()) uniqueMods.push_back(l.modName);
        }

        static char filterName[128] = "";
        static std::string selectedKeyFilter = "";
        static char keySearchBuf[128] = "";
        static std::string selectedModFilter = "";
        static char modSearchBuf[128] = "";

        ImGuiMCP::TextColored({ 0.5f, 0.8f, 1.0f, 1.0f }, "%s", GetLoc("common.filters", "Search Filters:"));
        ImGuiMCP::PushItemWidth(180.0f);
        ImGuiMCP::InputText(GetLoc("common.name", "Name"), filterName, sizeof(filterName));
        ImGuiMCP::SameLine();

        if (ImGuiMCP::BeginCombo(GetLoc("common.key", "Key"), selectedKeyFilter.empty() ? GetLoc("dash.all_keys", "All Keys") : selectedKeyFilter.c_str())) {
            ImGuiMCP::InputText("Search##keySearch", keySearchBuf, sizeof(keySearchBuf));
            std::string searchLower = ToLower(keySearchBuf);
            ImGuiMCP::Separator();

            if (ImGuiMCP::Selectable(GetLoc("dash.all_keys", "All Keys"), selectedKeyFilter.empty())) selectedKeyFilter = "";
            for (size_t k = 1; k < std::size(pcKeyNames); ++k) {
                if (searchLower.empty() || ToLower(pcKeyNames[k]).find(searchLower) != std::string::npos) {
                    if (ImGuiMCP::Selectable(pcKeyNames[k], selectedKeyFilter == pcKeyNames[k])) selectedKeyFilter = pcKeyNames[k];
                }
            }
            for (size_t k = 1; k < std::size(gamepadKeyNames); ++k) {
                if (searchLower.empty() || ToLower(gamepadKeyNames[k]).find(searchLower) != std::string::npos) {
                    if (ImGuiMCP::Selectable(gamepadKeyNames[k], selectedKeyFilter == gamepadKeyNames[k])) selectedKeyFilter = gamepadKeyNames[k];
                }
            }
            ImGuiMCP::EndCombo();
        }
        ImGuiMCP::SameLine();

        if (ImGuiMCP::BeginCombo(GetLoc("common.mod", "Mod"), selectedModFilter.empty() ? GetLoc("dash.all_mods", "All Mods") : selectedModFilter.c_str())) {
            ImGuiMCP::InputText("Search##modSearch", modSearchBuf, sizeof(modSearchBuf));
            std::string modSearchLower = ToLower(modSearchBuf);
            ImGuiMCP::Separator();

            if (ImGuiMCP::Selectable(GetLoc("dash.all_mods", "All Mods"), selectedModFilter.empty())) selectedModFilter = "";
            for (const auto& modName : uniqueMods) {
                if (modSearchLower.empty() || ToLower(modName).find(modSearchLower) != std::string::npos) {
                    if (ImGuiMCP::Selectable(modName.c_str(), selectedModFilter == modName)) selectedModFilter = modName;
                }
            }
            ImGuiMCP::EndCombo();
        }
        ImGuiMCP::PopItemWidth();
        ImGuiMCP::Spacing(); ImGuiMCP::Separator(); ImGuiMCP::Spacing();

        std::string lowerFilterName = ToLower(filterName);

        for (size_t i = 0; i < actionList.size(); ++i) {
            auto& action = actionList[i];
            if (!lowerFilterName.empty() && ToLower(action.name).find(lowerFilterName) == std::string::npos) continue;
            if (!HasKeyConfigured(action, selectedKeyFilter)) continue;

            std::vector<PluginLogic::ModListener> activeListeners;
            bool modMatch = selectedModFilter.empty();
            for (const auto& l : listeners) {
                if (l.actionID == static_cast<int>(i)) {
                    activeListeners.push_back(l);
                    if (!selectedModFilter.empty() && l.modName == selectedModFilter) modMatch = true;
                }
            }
            if (!selectedModFilter.empty() && !modMatch) continue;

            bool hasStateWarning = false;
            for (const auto& listener : activeListeners) {
                auto checkValidity = [&](int mAct, int modAct) {
                    bool mOk = listener.validMainActions.empty() || std::find(listener.validMainActions.begin(), listener.validMainActions.end(), mAct) != listener.validMainActions.end();
                    bool modOk = listener.validModActions.empty() || std::find(listener.validModActions.begin(), listener.validModActions.end(), modAct) != listener.validModActions.end();
                    return mOk && modOk;
                    };

                bool pcOk = (action.pcMainKey == 0) || checkValidity(action.pcMainAction, action.pcModAction);
                bool padOk = (action.gamepadMainKey == 0) || checkValidity(action.gamepadMainAction, action.gamepadModAction);

                if (!pcOk || !padOk) {
                    hasStateWarning = true;
                    break;
                }
            }

            ImGuiMCP::PushID(static_cast<int>(i));
            std::string actionSummary = GetActionSummary(action);
            std::string headerWarning = hasStateWarning ? " [!]" : "";
            std::string headerLabel = "[" + std::to_string(i) + "] " + std::string(action.name) + headerWarning + "   -   " + actionSummary + "###actionHeader_" + std::to_string(i);

            if (hasStateWarning) {
                ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Header, { 0.4f, 0.1f, 0.1f, 1.0f });
            }
            else {
                ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Header, { 0.2f, 0.2f, 0.2f, 1.0f });
            }

            if (ImGuiMCP::CollapsingHeader(headerLabel.c_str())) {
                ImGuiMCP::PopStyleColor();
                ImGuiMCP::Indent(); ImGuiMCP::Spacing();

                if (hasStateWarning) {
                    ImGuiMCP::TextColored({ 1.0f, 0.4f, 0.4f, 1.0f }, "%s", GetLoc("action.warning_state", "[WARNING] Selected state is NOT supported by a connected Mod!"));
                    ImGuiMCP::Spacing();
                }

                ImGuiMCP::InputText(GetLoc("common.name", "Name"), action.name, sizeof(action.name));
                ImGuiMCP::Spacing();

                int currentId = static_cast<int>(i);
                ImGuiMCP::PushItemWidth(200.0f);
                if (ImGuiMCP::InputInt(GetLoc("common.id", "ID"), &currentId, 1, 10, ImGuiMCP::ImGuiInputTextFlags_EnterReturnsTrue)) {
                    if (currentId >= 0 && currentId < static_cast<int>(actionList.size()) && currentId != i) std::swap(actionList[i], actionList[currentId]);
                }
                ImGuiMCP::PopItemWidth();
                ImGuiMCP::SameLine();

                ImGuiMCP::BeginDisabled(i == 0);
                if (ImGuiMCP::Button(GetLoc("common.move_up", "Move Up"))) std::swap(actionList[i], actionList[i - 1]);
                ImGuiMCP::EndDisabled();
                ImGuiMCP::SameLine();

                ImGuiMCP::BeginDisabled(i == actionList.size() - 1);
                if (ImGuiMCP::Button(GetLoc("common.move_down", "Move Down"))) std::swap(actionList[i], actionList[i + 1]);
                ImGuiMCP::EndDisabled();
                ImGuiMCP::SameLine();

                ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Button, { 0.6f, 0.2f, 0.2f, 1.0f });
                if (ImGuiMCP::Button(GetLoc("common.delete", "Delete"))) {
                    actionList.erase(actionList.begin() + i);
                    ImGuiMCP::PopStyleColor();
                    ImGuiMCP::Unindent(); ImGuiMCP::PopID();
                    break;
                }
                ImGuiMCP::PopStyleColor();
                ImGuiMCP::Spacing(); ImGuiMCP::Separator(); ImGuiMCP::Spacing();

                // PC Block
                ImGuiMCP::TextColored({ 0.5f, 0.8f, 1.0f, 1.0f }, "%s", GetLoc("action.pc_header", "Keyboard and Mouse"));
                bool canGesturePC = (action.pcMainAction == 2 || action.pcMainAction == 4);
                if (!canGesturePC && action.pcModAction == 3) action.pcModAction = 0;

                if (ImGuiMCP::BeginTable("PCTable", 2, ImGuiMCP::ImGuiTableFlags_BordersInnerH)) {
                    ImGuiMCP::TableSetupColumn(GetLoc("action.col_key", "Key / Trigger"), ImGuiMCP::ImGuiTableColumnFlags_WidthFixed, 200.0f);
                    ImGuiMCP::TableSetupColumn(GetLoc("action.col_state", "State"), ImGuiMCP::ImGuiTableColumnFlags_WidthStretch);

                    ImGuiMCP::TableNextRow(); ImGuiMCP::TableSetColumnIndex(0);
                    int pcMainIdx = GetIndexFromID(action.pcMainKey, pcKeyIDs, std::size(pcKeyIDs));
                    ImGuiMCP::SetNextItemWidth(180.0f);
                    if (SearchableCombo("##pcMKey", &pcMainIdx, pcKeyNames, std::size(pcKeyNames))) {
                        int selectedKey = pcKeyIDs[pcMainIdx];
                        if (selectedKey != 0 && selectedKey == action.pcModifierKey) action.pcModifierKey = 0;
                        action.pcMainKey = selectedKey;
                    }

                    ImGuiMCP::TableSetColumnIndex(1); ImGuiMCP::SetNextItemWidth(150.0f);
                    if (action.pcMainKey == 0) action.pcMainAction = 0;
                    ImGuiMCP::BeginDisabled(action.pcMainKey == 0);
                    if (ImGuiMCP::BeginCombo("##pcMAct", GetStateName(action.pcMainAction))) {
                        for (int s = 0; s < 5; ++s) {
                            if (s == 3) continue; // Pula o gesto no Main
                            if (ImGuiMCP::Selectable(GetStateName(s), action.pcMainAction == s)) action.pcMainAction = s;
                        }
                        ImGuiMCP::EndCombo();
                    }
                    if (action.pcMainAction == 1) {
                        ImGuiMCP::SameLine(); ImGuiMCP::SetNextItemWidth(140.0f);
                        ImGuiMCP::InputInt(GetLoc("action.taps", "Taps##pcMainTap"), &action.pcMainTapCount);
                        if (action.pcMainTapCount < 1) action.pcMainTapCount = 1;
                    }
                    ImGuiMCP::EndDisabled();

                    ImGuiMCP::TableNextRow(); ImGuiMCP::TableSetColumnIndex(0);
                    ImGuiMCP::TextDisabled("%s", GetLoc("action.mod_optional", "   + Modifier (Optional)"));

                    if (action.pcModAction == 3) {
                        std::string gesturePreview = (action.gestureIndex >= 0 && action.gestureIndex < movementList.size())
                            ? std::string(movementList[action.gestureIndex].name) : GetLoc("input.no_gesture", "[ No Gesture ]");
                        ImGuiMCP::SetNextItemWidth(180.0f);
                        if (ImGuiMCP::BeginCombo("##pcModKey", gesturePreview.c_str())) {
                            for (size_t gIdx = 0; gIdx < movementList.size(); ++gIdx) {
                                if (ImGuiMCP::Selectable(movementList[gIdx].name, action.gestureIndex == (int)gIdx)) action.gestureIndex = (int)gIdx;
                            }
                            ImGuiMCP::EndCombo();
                        }
                    }
                    else {
                        int pcModIdx = GetIndexFromID(action.pcModifierKey, pcKeyIDs, std::size(pcKeyIDs));
                        ImGuiMCP::SetNextItemWidth(180.0f);
                        if (SearchableCombo("##pcModKey", &pcModIdx, pcKeyNames, std::size(pcKeyNames))) {
                            int selectedKey = pcKeyIDs[pcModIdx];
                            if (selectedKey != 0 && selectedKey == action.pcMainKey) action.pcMainKey = 0;
                            action.pcModifierKey = selectedKey;
                            if (action.pcModifierKey == 0) action.pcModAction = 0;
                        }
                    }

                    ImGuiMCP::TableSetColumnIndex(1); ImGuiMCP::SetNextItemWidth(150.0f);
                    if (ImGuiMCP::BeginCombo("##pcModAct", GetStateName(action.pcModAction))) {
                        for (int s = 0; s < 5; ++s) {
                            if (!canGesturePC && s == 3) continue;
                            if (ImGuiMCP::Selectable(GetStateName(s), action.pcModAction == s)) {
                                action.pcModAction = s;
                                if (s == 0) action.pcModifierKey = 0;
                                else if (s == 3 && action.gestureIndex < 0 && !movementList.empty()) action.gestureIndex = 0;
                                else if (action.pcModifierKey == 0) {
                                    for (size_t k = 1; k < std::size(pcKeyIDs); ++k) {
                                        if (pcKeyIDs[k] != action.pcMainKey) { action.pcModifierKey = pcKeyIDs[k]; break; }
                                    }
                                }
                            }
                        }
                        ImGuiMCP::EndCombo();
                    }
                    if (action.pcModAction == 1) {
                        ImGuiMCP::SameLine(); ImGuiMCP::SetNextItemWidth(140.0f);
                        ImGuiMCP::InputInt(GetLoc("action.taps", "Taps##pcModTap"), &action.pcModTapCount);
                        if (action.pcModTapCount < 1) action.pcModTapCount = 1;
                    }
                    ImGuiMCP::EndTable();
                }
                ImGuiMCP::Checkbox(GetLoc("action.delay", "Delay##pcDelay"), &action.pcDelayTap);
                ShowDelayTooltip();
                ImGuiMCP::Spacing(); ImGuiMCP::Separator(); ImGuiMCP::Spacing();

                // Pad Block
                ImGuiMCP::TextColored({ 0.5f, 1.0f, 0.5f, 1.0f }, "%s", GetLoc("action.pad_header", "Gamepad"));
                bool canGesturePad = (action.gamepadMainAction == 2 || action.gamepadMainAction == 4);
                if (!canGesturePad && action.gamepadModAction == 3) action.gamepadModAction = 0;

                if (ImGuiMCP::BeginTable("PadTable", 2, ImGuiMCP::ImGuiTableFlags_BordersInnerH)) {
                    ImGuiMCP::TableSetupColumn(GetLoc("action.col_key", "Key / Trigger"), ImGuiMCP::ImGuiTableColumnFlags_WidthFixed, 200.0f);
                    ImGuiMCP::TableSetupColumn(GetLoc("action.col_state", "State"), ImGuiMCP::ImGuiTableColumnFlags_WidthStretch);

                    ImGuiMCP::TableNextRow(); ImGuiMCP::TableSetColumnIndex(0);
                    int padMainIdx = GetIndexFromID(action.gamepadMainKey, gamepadKeyIDs, std::size(gamepadKeyIDs));
                    ImGuiMCP::SetNextItemWidth(180.0f);
                    if (SearchableCombo("##padMKey", &padMainIdx, gamepadKeyNames, std::size(gamepadKeyNames))) {
                        int selectedKey = gamepadKeyIDs[padMainIdx];
                        if (selectedKey != 0 && selectedKey == action.gamepadModifierKey) action.gamepadModifierKey = 0;
                        action.gamepadMainKey = selectedKey;
                    }

                    ImGuiMCP::TableSetColumnIndex(1); ImGuiMCP::SetNextItemWidth(150.0f);
                    if (action.gamepadMainKey == 0) action.gamepadMainAction = 0;
                    ImGuiMCP::BeginDisabled(action.gamepadMainKey == 0);
                    if (ImGuiMCP::BeginCombo("##padMAct", GetStateName(action.gamepadMainAction))) {
                        for (int s = 0; s < 5; ++s) {
                            if (s == 3) continue; 
                            if (ImGuiMCP::Selectable(GetStateName(s), action.gamepadMainAction == s)) action.gamepadMainAction = s;
                        }
                        ImGuiMCP::EndCombo();
                    }
                    if (action.gamepadMainAction == 1) {
                        ImGuiMCP::SameLine(); ImGuiMCP::SetNextItemWidth(140.0f);
                        ImGuiMCP::InputInt(GetLoc("action.taps", "Taps##padMainTap"), &action.gamepadMainTapCount);
                        if (action.gamepadMainTapCount < 1) action.gamepadMainTapCount = 1;
                    }
                    ImGuiMCP::EndDisabled();

                    ImGuiMCP::TableNextRow(); ImGuiMCP::TableSetColumnIndex(0);
                    ImGuiMCP::TextDisabled("%s", GetLoc("action.mod_optional", "   + Modifier (Optional)"));

                    if (action.gamepadModAction == 3) {
                        std::string gesturePreview = (action.gestureIndex >= 0 && action.gestureIndex < movementList.size())
                            ? std::string(movementList[action.gestureIndex].name) : GetLoc("input.no_gesture", "[ No Gesture ]");
                        ImGuiMCP::SetNextItemWidth(180.0f);
                        if (ImGuiMCP::BeginCombo("##padModKey", gesturePreview.c_str())) {
                            for (size_t gIdx = 0; gIdx < movementList.size(); ++gIdx) {
                                if (ImGuiMCP::Selectable(movementList[gIdx].name, action.gestureIndex == (int)gIdx)) action.gestureIndex = (int)gIdx;
                            }
                            ImGuiMCP::EndCombo();
                        }
                    }
                    else {
                        int padModIdx = GetIndexFromID(action.gamepadModifierKey, gamepadKeyIDs, std::size(gamepadKeyIDs));
                        ImGuiMCP::SetNextItemWidth(180.0f);
                        if (SearchableCombo("##padModKey", &padModIdx, gamepadKeyNames, std::size(gamepadKeyNames))) {
                            int selectedKey = gamepadKeyIDs[padModIdx];
                            if (selectedKey != 0 && selectedKey == action.gamepadMainKey) action.gamepadMainKey = 0;
                            action.gamepadModifierKey = selectedKey;
                            if (action.gamepadModifierKey == 0) action.gamepadModAction = 0;
                        }
                    }

                    ImGuiMCP::TableSetColumnIndex(1); ImGuiMCP::SetNextItemWidth(150.0f);
                    if (ImGuiMCP::BeginCombo("##padModAct", GetStateName(action.gamepadModAction))) {
                        for (int s = 0; s < 5; ++s) {
                            if (!canGesturePad && s == 3) continue;
                            if (ImGuiMCP::Selectable(GetStateName(s), action.gamepadModAction == s)) {
                                action.gamepadModAction = s;
                                if (s == 0) action.gamepadModifierKey = 0;
                                else if (s == 3 && action.gestureIndex < 0 && !movementList.empty()) action.gestureIndex = 0;
                                else if (action.gamepadModifierKey == 0) {
                                    for (size_t k = 1; k < std::size(gamepadKeyIDs); ++k) {
                                        if (gamepadKeyIDs[k] != action.gamepadMainKey) { action.gamepadModifierKey = gamepadKeyIDs[k]; break; }
                                    }
                                }
                            }
                        }
                        ImGuiMCP::EndCombo();
                    }
                    if (action.gamepadModAction == 1) {
                        ImGuiMCP::SameLine(); ImGuiMCP::SetNextItemWidth(140.0f);
                        ImGuiMCP::InputInt(GetLoc("action.taps", "Taps##padModTap"), &action.gamepadModTapCount);
                        if (action.gamepadModTapCount < 1) action.gamepadModTapCount = 1;
                    }
                    if (action.gamepadModAction == 3) {
                        ImGuiMCP::TableNextRow(); ImGuiMCP::TableSetColumnIndex(0);
                        ImGuiMCP::TextColored({ 0.8f, 0.8f, 0.4f, 1.0f }, "%s", GetLoc("action.thumbstick", "   Thumbstick for Gesture:"));
                        ImGuiMCP::TableSetColumnIndex(1);
                        ImGuiMCP::RadioButton(GetLoc("action.left", "Left##padStick"), &action.gamepadGestureStick, 0);
                        ImGuiMCP::SameLine();
                        ImGuiMCP::RadioButton(GetLoc("action.right", "Right##padStick"), &action.gamepadGestureStick, 1);
                    }
                    ImGuiMCP::EndTable();
                }
                ImGuiMCP::Checkbox(GetLoc("action.delay", "Delay##padDelay"), &action.gamepadDelayTap);
                ShowDelayTooltip();
                ImGuiMCP::Spacing(); ImGuiMCP::Separator(); ImGuiMCP::Spacing();

                ImGuiMCP::TextColored({ 0.8f, 0.8f, 0.5f, 1.0f }, "%s", GetLoc("action.response_timings", "Response Timings"));
                ImGuiMCP::Checkbox(GetLoc("action.use_custom", "Use Custom Timings for this Action"), &action.useCustomTimings);

                float tempHold = action.useCustomTimings ? action.holdDuration : globalHoldDuration;
                float tempTap = action.useCustomTimings ? action.tapWindow : globalTapWindow;

                ImGuiMCP::BeginDisabled(!action.useCustomTimings);
                ImGuiMCP::PushItemWidth(150.0f);
                ImGuiMCP::SliderFloat(GetLoc("action.tap_window", "Tap Window##act"), &tempTap, 0.01f, 1.0f, "%.2f");
                ImGuiMCP::PopItemWidth(); ImGuiMCP::SameLine();
                ImGuiMCP::PushItemWidth(300.0f);
                ImGuiMCP::InputFloat("##TapInput", &tempTap, 0.01f, 0.0f, "%.2f");
                ImGuiMCP::PopItemWidth();

                ImGuiMCP::PushItemWidth(150.0f);
                ImGuiMCP::SliderFloat(GetLoc("action.hold_time", "Hold Time##act"), &tempHold, 0.01f, 2.0f, "%.2f");
                ImGuiMCP::PopItemWidth(); ImGuiMCP::SameLine();
                ImGuiMCP::PushItemWidth(300.0f);
                ImGuiMCP::InputFloat("##HoldInput", &tempHold, 0.01f, 0.0f, "%.2f");
                ImGuiMCP::PopItemWidth();
                ImGuiMCP::EndDisabled();

                if (action.useCustomTimings) { action.holdDuration = tempHold; action.tapWindow = tempTap; }
                ImGuiMCP::Spacing(); ImGuiMCP::Separator(); ImGuiMCP::Spacing();

                if (!activeListeners.empty()) {
                    ImGuiMCP::TextColored({ 0.9f, 0.6f, 1.0f, 1.0f }, "%s", GetLoc("common.connected_mods", "Mods Connected to this Action:"));
                    for (const auto& listener : activeListeners) {

                        std::string modLabel = GetLoc("common.mod", "Mod");
                        std::string purposeLabel = GetLoc("common.purpose", "Purpose");
                        std::string actionLabel = GetLoc("common.action", "Action");
                        std::string modActionLabel = GetLoc("common.mod_action", "Mod Action");
                        std::string anyLabel = GetLoc("common.any", "Any");

                        std::string line = modLabel + ": '" + listener.modName + "'  |  " + purposeLabel + ": '" + listener.purpose + "'";

                        auto formatExpected = [&](const std::vector<int>& validList) {
                            if (validList.empty()) return anyLabel;
                            std::string res = "";
                            for (size_t k = 0; k < validList.size(); ++k) {
                                if (validList[k] >= 0 && validList[k] <= 4) {
                                    res += GetStateName(validList[k]); 
                                    if (k < validList.size() - 1) res += ", ";
                                }
                            }
                            return res;
                            };

                        if (!listener.validMainActions.empty() || !listener.validModActions.empty()) {
                            line += "  |  " + actionLabel + ": " + formatExpected(listener.validMainActions) +
                                "  |  " + modActionLabel + ": " + formatExpected(listener.validModActions);
                        }

                        ImGuiMCP::BulletText("%s", line.c_str());
                    }
                    ImGuiMCP::Spacing(); ImGuiMCP::Separator(); ImGuiMCP::Spacing();
                }

                ImGuiMCP::Unindent(); ImGuiMCP::Spacing();
            }
            else ImGuiMCP::PopStyleColor();
            ImGuiMCP::PopID();
        }
    }

    inline void GenerateDefaultActions() {
        // Se já foi gerado no passado (salvo no json), não cria novamente.
        if (defaultActionsGenerated) return;

        std::vector<std::string> defaultNames = {
            "Forward", "Back", "Left", "Right", "Left hand", "Right hand", "Power Attack", "Block",
            "Jump", "Sprint", "Sneak", "Shout", "Ready Weapon", "Interact", "Dodge", "Target Lock",
            "Menu", "Journal", "Wait", "Auto Move", "Toggle POV", "Quick Save", "Quick Load", "Quick Map",
            "Quick Magic", "Quick Inventory", "Quick Stats", "Tween Menu", "Favorites", "Console",
            "Use Potion", "Use Poison", "Bash", "Heavy Attack", "Light Attack", "Camera Lock", 
            "Zoom In", "Zoom Out", "Next Target", "Previous Target", "Combo 1", "Combo 2", "Combo 3", 
            "Special Move 1", "Special Move 2", "Special Move 3", "Special Move 4", "Ultimate", "Heal Companion", 
            "Command Pet", "Whistle", "Quick Slot 1", "Quick Slot 2"
        };

        for (size_t i = 0; i < defaultNames.size(); ++i) {
            ActionEntry act;
            strncpy_s(act.name, defaultNames[i].c_str(), sizeof(act.name) - 1);
            actionList.push_back(act);
        }

        defaultActionsGenerated = true;

        SaveSettingsToJson();
        SaveActionsToJson(); 
    }

    inline void RenderDebugMenu() {
        ImGuiMCP::Text("%s", GetLoc("debug.title", "Input Manager - Debug"));
        ImGuiMCP::Separator(); ImGuiMCP::Spacing();

        if (ImGuiMCP::Checkbox(GetLoc("debug.show_log", "Show Log in Hud and on log"), &showDebugLogs)) {
            SaveSettingsToJson();
        }
        ImGuiMCP::Spacing();
        ImGuiMCP::TextDisabled("%s", GetLoc("debug.desc", "If enabled, triggered actions and motions will be displayed on the HUD and printed to the SKSE log."));
    }

    inline void Register() {
        if (!SKSEMenuFramework::IsInstalled()) return;
        LoadLanguage();
        LoadCacheFromJson();
        LoadSettingsFromJson();
        LoadActionsFromJson();
        LoadGesturesFromJson();
        LoadMotionsFromJson();
        GenerateDefaultActions();
        SKSEMenuFramework::SetSection("Input Manager");
        SKSEMenuFramework::AddSectionItem(GetLoc("menu.tab_inputs", "Inputs"), RenderMenu);
        SKSEMenuFramework::AddSectionItem(GetLoc("menu.tab_motions", "Motion Inputs"), RenderMotionMenu);
        SKSEMenuFramework::AddSectionItem(GetLoc("menu.tab_gestures", "Gestures"), RenderGesturesMenu);
        SKSEMenuFramework::AddSectionItem(GetLoc("menu.tab_dashboard", "Inputs in use"), RenderDashboardMenu);
        SKSEMenuFramework::AddSectionItem(GetLoc("menu.tab_debug", "Debug"), RenderDebugMenu);
    }
}