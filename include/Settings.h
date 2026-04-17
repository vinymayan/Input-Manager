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
    inline const char* actionStateNames[] = { "Ignore", "Tap", "Hold", "Gesture","Press"};

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
            ImGuiMCP::SetTooltip(
                "What happens if unchecked?\n"
                "If there is another Double Tap action using this same key,\n"
                "the Single Tap will trigger accidentally before the Double Tap.\n\n"
                "Check this option for the system to wait a short time,\n"
                "ensuring isolated actions do not overlap."
            );
        }
    }

    inline std::string ToLower(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
        return s;
    }

    inline std::string GetActionSummary(const ActionEntry& a) {
        std::string summary = "";

        if (a.pcMainKey != 0) {
            summary += "PC: ";

            int mainIdx = GetIndexFromID(a.pcMainKey, pcKeyIDs, std::size(pcKeyIDs));
            summary += std::string(pcKeyNames[mainIdx]) + " (" + actionStateNames[a.pcMainAction] + ")";


            if (a.pcModAction == 3) {
                std::string gName = (a.gestureIndex >= 0 && a.gestureIndex < movementList.size()) ? movementList[a.gestureIndex].name : "Unknown Gesture";
                summary += "  +  " + gName + " (Gesture)";
            }
            else if (a.pcModifierKey != 0) {
                int modIdx = GetIndexFromID(a.pcModifierKey, pcKeyIDs, std::size(pcKeyIDs));
                summary += "  +  " + std::string(pcKeyNames[modIdx]) + " (" + actionStateNames[a.pcModAction] + ")";
            }
        }

        if (a.gamepadMainKey != 0) {
            if (!summary.empty()) summary += "  |  ";
            summary += "PAD: ";

            int mainIdx = GetIndexFromID(a.gamepadMainKey, gamepadKeyIDs, std::size(gamepadKeyIDs));
            summary += std::string(gamepadKeyNames[mainIdx]) + " (" + actionStateNames[a.gamepadMainAction] + ")";

            if (a.gamepadModAction == 3) {
                std::string gName = (a.gestureIndex >= 0 && a.gestureIndex < movementList.size()) ? movementList[a.gestureIndex].name : "Unknown Gesture";
                summary += "  +  " + gName + " (Gesture)";
            }
            else if (a.gamepadModifierKey != 0) {
                int modIdx = GetIndexFromID(a.gamepadModifierKey, gamepadKeyIDs, std::size(gamepadKeyIDs));
                summary += "  +  " + std::string(gamepadKeyNames[modIdx]) + " (" + actionStateNames[a.gamepadModAction] + ")";
            }
        }

        if (summary.empty()) summary = "No keys configured";
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
        const char* preview_value = (*current_item >= 0 && *current_item < items_count) ? items[*current_item] : "None";

        if (ImGuiMCP::BeginCombo(label, preview_value)) {
            static char searchBuf[128] = "";

            // Quando a lista abre, limpa a caixa de texto e foca nela
            if (ImGuiMCP::IsWindowAppearing()) {
                searchBuf[0] = '\0';
                ImGuiMCP::SetKeyboardFocusHere();
            }

            ImGuiMCP::InputText("Filter...##Search", searchBuf, sizeof(searchBuf));
            ImGuiMCP::Separator();

            std::string searchLower = ToLower(searchBuf);

            // Popula os Selectables filtrando a array base
            for (int i = 0; i < items_count; i++) {
                if (searchLower.empty() || ToLower(items[i]).find(searchLower) != std::string::npos) {
                    bool is_selected = (*current_item == i);
                    if (ImGuiMCP::Selectable(items[i], is_selected)) {
                        *current_item = i;
                        changed = true;
                    }
                    // Desce a barra de rolagem até o item selecionado ao abrir
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
        ImGuiMCP::Text("Input Manager - Motion Inputs (Fighting Game Style)");
        ImGuiMCP::Separator(); ImGuiMCP::Spacing();

        bool hasMotionConflict = false;
        std::string motionErrorMsg;

        for (size_t i = 0; i < motionList.size(); ++i) {
            for (size_t j = i + 1; j < motionList.size(); ++j) {
                // Checa Nomes Iguais
                if (SanitizeFileName(motionList[i].name) == SanitizeFileName(motionList[j].name)) {
                    hasMotionConflict = true;
                    motionErrorMsg = "DUPLICATE NAME: Motions '" + std::string(motionList[i].name) + "' and '" + std::string(motionList[j].name) + "' conflict.";
                    break;
                }
                // Checa Sequências Iguais (PC ou Pad)
                bool pcConflict = (!motionList[i].pcSequence.empty() && motionList[i].pcSequence == motionList[j].pcSequence);
                bool padConflict = (!motionList[i].padSequence.empty() && motionList[i].padSequence == motionList[j].padSequence);

                if (pcConflict || padConflict) {
                    hasMotionConflict = true;
                    motionErrorMsg = "DUPLICATE SEQUENCE: '" + std::string(motionList[i].name) + "' and '" + std::string(motionList[j].name) + "' have identical " + (pcConflict ? "PC" : "Gamepad") + " inputs.";
                    break;
                }
            }
            if (hasMotionConflict) break;
        }

        if (hasMotionConflict) {
            ImGuiMCP::TextColored({ 1.0f, 0.2f, 0.2f, 1.0f }, "[CONFIGURATION ERROR]");
            ImGuiMCP::TextColored({ 1.0f, 0.8f, 0.2f, 1.0f }, "%s", motionErrorMsg.c_str());
            ImGuiMCP::TextColored({ 0.7f, 0.7f, 0.7f, 1.0f }, "Resolve the duplication by changing the name or recording a new sequence.");
            ImGuiMCP::Spacing(); ImGuiMCP::Separator(); ImGuiMCP::Spacing();
        }

        // Trava o botão Save se houver conflito
        ImGuiMCP::BeginDisabled(hasMotionConflict);
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Button, { 0.1f, 0.5f, 0.1f, 1.0f });
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_ButtonHovered, { 0.2f, 0.6f, 0.2f, 1.0f });
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_ButtonActive, { 0.1f, 0.4f, 0.1f, 1.0f });

        if (ImGuiMCP::Button("Save Motion Inputs")) {
            SaveMotionsToJson();
        }

        ImGuiMCP::PopStyleColor(3);
        ImGuiMCP::EndDisabled();
        ImGuiMCP::SameLine();
        if (ImGuiMCP::Button("Add New Motion")) {
            motionList.push_back(MotionEntry());
        }
        ImGuiMCP::Spacing(); ImGuiMCP::Separator(); ImGuiMCP::Spacing();

        // Máquina de Estados Global para a Janela
        enum MotionMenuState { M_NONE, M_WAITING_REC, M_RECORDING, M_WAITING_TEST, M_TESTING, M_TEST_DONE };
        static MotionMenuState m_state = M_NONE;
        static int m_activeIndex = -1;
        static bool m_activePad = false;
        static std::chrono::steady_clock::time_point m_uiStartTime;

        auto keyMgr = PluginLogic::KeyManager::GetSingleton();
        bool isCapturing = (m_state != M_NONE && m_activeIndex >= 0 && m_activeIndex < motionList.size());

        // ====================================================================
        // INTERFACE DO TOPO: GRAVAÇÃO E TESTE
        // ====================================================================
        if (isCapturing) {
            ImGuiMCP::TextColored({ 1.0f, 0.5f, 0.0f, 1.0f }, ">>> MOTION CAPTURE/TEST MODE <<<");
            auto& motion = motionList[m_activeIndex];
            float maxT = motion.timeWindow;

            // ETAPA 1: AGUARDAR ENTER
            if (m_state == M_WAITING_REC || m_state == M_WAITING_TEST) {
                ImGuiMCP::Text("Prepare yourself and press [ENTER] to START %s.", m_state == M_WAITING_REC ? "RECORDING" : "TESTING");
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
                if (ImGuiMCP::Button("Cancel")) { m_state = M_NONE; m_activeIndex = -1; }
            }
            // ETAPA 2: GRAVANDO
            else if (m_state == M_RECORDING) {
                ImGuiMCP::TextColored({ 1.0f, 0.2f, 0.2f, 1.0f }, ">>> RECORDING IN PROGRESS <<<");
                float elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - m_uiStartTime).count();
                float prog = std::min(elapsed / maxT, 1.0f);

                char buf[32]; snprintf(buf, sizeof(buf), "%.1f / %.1fs", elapsed, maxT);
                ImGuiMCP::ProgressBar(prog, { -1.0f, 0.0f }, buf);
                ImGuiMCP::Spacing();

                auto seq = keyMgr->GetRecordedMotion();
                if (seq.size() > 20) {
                    seq.resize(20);
                }
                std::string s = "Captured: ";
                for (auto k : seq) s += "[" + FormatMotionKey(k) + "] ";
                ImGuiMCP::TextWrapped("%s", s.c_str());

                // Sai sozinho quando o timer expira no backend
                if (!keyMgr->IsRecordingMotion() || seq.size() >= 20) {
                    if (m_activePad) motion.padSequence = seq; else motion.pcSequence = seq;

                    if (keyMgr->IsRecordingMotion()) {
                        keyMgr->StopMotionRecording();
                    }

                    m_state = M_NONE; m_activeIndex = -1;
                }
            }
            // ETAPA 3: TESTANDO
            else if (m_state == M_TESTING) {
                ImGuiMCP::TextColored({ 0.2f, 1.0f, 1.0f, 1.0f }, ">>> TESTING IN PROGRESS <<<");
                ImGuiMCP::Text("Execute the sequence now!");

                float elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - m_uiStartTime).count();
                float prog = std::min(elapsed / maxT, 1.0f);

                char buf[32]; snprintf(buf, sizeof(buf), "%.1f / %.1fs", elapsed, maxT);
                ImGuiMCP::ProgressBar(prog, { -1.0f, 0.0f }, buf);

                if (keyMgr->GetMotionTestSuccess()) {
                    m_state = M_TEST_DONE;
                }
                else if (!keyMgr->IsTestingMotion()) {
                    m_state = M_TEST_DONE; // Falhou no timer
                }
            }
            // ETAPA 4: RESULTADO DO TESTE
            else if (m_state == M_TEST_DONE) {
                if (keyMgr->GetMotionTestSuccess()) {
                    ImGuiMCP::TextColored({ 0.2f, 1.0f, 0.2f, 1.0f }, "SUCCESS! Motion executed correctly in time!");
                }
                else {
                    ImGuiMCP::TextColored({ 1.0f, 0.2f, 0.2f, 1.0f }, "FAILED! You ran out of time or pressed wrong buttons.");
                }
                ImGuiMCP::Spacing();
                if (ImGuiMCP::Button("Close")) {
                    keyMgr->ResetMotionTest();
                    m_state = M_NONE; m_activeIndex = -1;
                }
            }

            ImGuiMCP::Spacing(); ImGuiMCP::Separator(); ImGuiMCP::Spacing();
            ImGuiMCP::BeginDisabled();
        }

        // ====================================================================
        // LISTA DE MOTION INPUTS
        // ====================================================================
        auto motionListeners = PluginLogic::KeyManager::GetSingleton()->GetMotionListeners();
        for (size_t i = 0; i < motionList.size(); ++i) {
            auto& motion = motionList[i];
            ImGuiMCP::PushID(static_cast<int>(i));

            std::string headerLabel = "[" + std::to_string(i) + "] " + std::string(motion.name) + "###motionHeader_" + std::to_string(i);

            if (ImGuiMCP::CollapsingHeader(headerLabel.c_str())) {
                ImGuiMCP::Indent();
                ImGuiMCP::Spacing();
                ImGuiMCP::InputText("Motion Name", motion.name, sizeof(motion.name));

                // --- SISTEMA DE REORDENAMENTO ---
                ImGuiMCP::Spacing();
                int currentId = static_cast<int>(i);
                ImGuiMCP::PushItemWidth(200.0f);
                if (ImGuiMCP::InputInt("Motion ID", &currentId, 1, 10, ImGuiMCP::ImGuiInputTextFlags_EnterReturnsTrue)) {
                    if (currentId >= 0 && currentId < static_cast<int>(motionList.size()) && currentId != i) {
                        std::swap(motionList[i], motionList[currentId]);
                    }
                }
                ImGuiMCP::PopItemWidth();
                if (ImGuiMCP::IsItemHovered()) ImGuiMCP::SetTooltip("Type the ID and press ENTER to swap motions.");
                ImGuiMCP::SameLine();

                ImGuiMCP::BeginDisabled(i == 0);
                if (ImGuiMCP::Button("Move Up##mot")) std::swap(motionList[i], motionList[i - 1]);
                ImGuiMCP::EndDisabled();
                ImGuiMCP::SameLine();

                ImGuiMCP::BeginDisabled(i == motionList.size() - 1);
                if (ImGuiMCP::Button("Move Down##mot")) std::swap(motionList[i], motionList[i + 1]);
                ImGuiMCP::EndDisabled();

                ImGuiMCP::Spacing();
                ImGuiMCP::PushItemWidth(200.0f);
                ImGuiMCP::SliderFloat("Time Window (s)##timeW", &motion.timeWindow, 0.5f, 4.0f, "%.1f");
                ImGuiMCP::PopItemWidth();
                if (ImGuiMCP::IsItemHovered()) ImGuiMCP::SetTooltip("Max time allowed to complete the full sequence.");
                ImGuiMCP::Spacing();

                // PC BLOCK
                ImGuiMCP::TextColored({ 0.5f, 1.0f, 0.5f, 1.0f }, "PC Sequence:");
                std::string pcStr = "";
                for (uint32_t k : motion.pcSequence) pcStr += "[" + FormatMotionKey(k) + "] ";
                ImGuiMCP::TextWrapped("%s", pcStr.empty() ? "None" : pcStr.c_str());

                if (ImGuiMCP::Button("Record PC")) {
                    m_activeIndex = static_cast<int>(i); m_activePad = false; m_state = M_WAITING_REC;
                }
                ImGuiMCP::SameLine();
                if (ImGuiMCP::Button("Test PC")) {
                    m_activeIndex = static_cast<int>(i); m_activePad = false; m_state = M_WAITING_TEST;
                }
                ImGuiMCP::Spacing();

                // GAMEPAD BLOCK
                ImGuiMCP::TextColored({ 0.5f, 1.0f, 0.5f, 1.0f }, "Gamepad Sequence:");
                std::string padStr = "";
                for (uint32_t k : motion.padSequence) padStr += "[" + FormatMotionKey(k) + "] ";
                ImGuiMCP::TextWrapped("%s", padStr.empty() ? "None" : padStr.c_str());

                if (ImGuiMCP::Button("Record Gamepad")) {
                    m_activeIndex = static_cast<int>(i); m_activePad = true; m_state = M_WAITING_REC;
                }
                ImGuiMCP::SameLine();
                if (ImGuiMCP::Button("Test Gamepad")) {
                    m_activeIndex = static_cast<int>(i); m_activePad = true; m_state = M_WAITING_TEST;
                }

                std::vector<PluginLogic::ModListener> activeListeners;
                for (const auto& l : motionListeners) {
                    if (l.actionID == static_cast<int>(i)) activeListeners.push_back(l);
                }

                if (!activeListeners.empty()) {
                    ImGuiMCP::Spacing(); ImGuiMCP::Separator(); ImGuiMCP::Spacing();
                    ImGuiMCP::TextColored({ 0.9f, 0.6f, 1.0f, 1.0f }, "Mods Connected to this Motion:");
                    for (const auto& listener : activeListeners) {
                        ImGuiMCP::BulletText("Mod: '%s'  |  Purpose: '%s'", listener.modName.c_str(), listener.purpose.c_str());
                    }
                }

                ImGuiMCP::Spacing(); ImGuiMCP::Separator(); ImGuiMCP::Spacing();
                ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Button, { 0.6f, 0.2f, 0.2f, 1.0f });
                if (ImGuiMCP::Button("Delete Motion")) {
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
        if (isCapturing) {
            ImGuiMCP::EndDisabled();
        }
    }

    inline void RenderSummaryDashboard(const std::vector<PluginLogic::ModListener>& listeners, const std::vector<PluginLogic::ModListener>& motionListeners) {
        ImGuiMCP::TextColored({ 0.5f, 1.0f, 0.5f, 1.0f }, "Keys and Combos in Use");
        ImGuiMCP::TextDisabled("Overview grouped by physical key for easy identification.");
        ImGuiMCP::Spacing();

        struct DashboardEntry {
            std::vector<int> actionIDs;
            std::vector<int> motionIDs;
        };

        std::vector<std::string> uniqueMods;
        auto addUniqueMod = [&](const std::string& modName) {
            if (std::find(uniqueMods.begin(), uniqueMods.end(), modName) == uniqueMods.end()) {
                uniqueMods.push_back(modName);
            }
            };

        for (const auto& l : listeners) addUniqueMod(l.modName);
        for (const auto& l : motionListeners) addUniqueMod(l.modName);

        static char dashFilterName[128] = "";
        static std::string dashSelectedKeyFilter = "";
        static char dashKeySearchBuf[128] = "";
        static std::string dashSelectedModFilter = "";
        static char dashModSearchBuf[128] = "";

        if (!dashSelectedModFilter.empty() && std::find(uniqueMods.begin(), uniqueMods.end(), dashSelectedModFilter) == uniqueMods.end()) {
            dashSelectedModFilter = "";
        }

        ImGuiMCP::TextColored({ 0.5f, 0.8f, 1.0f, 1.0f }, "Search Filters:");
        ImGuiMCP::PushItemWidth(180.0f);

        ImGuiMCP::InputText("Name##dash", dashFilterName, sizeof(dashFilterName));
        ImGuiMCP::SameLine();

        if (ImGuiMCP::BeginCombo("Key##dash", dashSelectedKeyFilter.empty() ? "All Keys" : dashSelectedKeyFilter.c_str())) {
            ImGuiMCP::InputText("Search##dashKeySearch", dashKeySearchBuf, sizeof(dashKeySearchBuf));
            std::string searchLower = ToLower(dashKeySearchBuf);
            ImGuiMCP::Separator();

            if (ImGuiMCP::Selectable("All Keys", dashSelectedKeyFilter.empty())) {
                dashSelectedKeyFilter = "";
            }

            for (size_t k = 1; k < std::size(pcKeyNames); ++k) {
                std::string kName = pcKeyNames[k];
                if (searchLower.empty() || ToLower(kName).find(searchLower) != std::string::npos) {
                    if (ImGuiMCP::Selectable((kName + " (PC)").c_str(), dashSelectedKeyFilter == kName + " (PC)")) {
                        dashSelectedKeyFilter = kName + " (PC)";
                    }
                }
            }
            for (size_t k = 1; k < std::size(gamepadKeyNames); ++k) {
                std::string kName = gamepadKeyNames[k];
                if (searchLower.empty() || ToLower(kName).find(searchLower) != std::string::npos) {
                    if (ImGuiMCP::Selectable((kName + " (PAD)").c_str(), dashSelectedKeyFilter == kName + " (PAD)")) {
                        dashSelectedKeyFilter = kName + " (PAD)";
                    }
                }
            }
            ImGuiMCP::EndCombo();
        }
        ImGuiMCP::SameLine();

        if (ImGuiMCP::BeginCombo("Mod##dash", dashSelectedModFilter.empty() ? "All Mods" : dashSelectedModFilter.c_str())) {
            ImGuiMCP::InputText("Search##dashModSearch", dashModSearchBuf, sizeof(dashModSearchBuf));
            std::string modSearchLower = ToLower(dashModSearchBuf);
            ImGuiMCP::Separator();

            if (ImGuiMCP::Selectable("All Mods", dashSelectedModFilter.empty())) {
                dashSelectedModFilter = "";
            }
            if (uniqueMods.empty()) {
                ImGuiMCP::TextDisabled("No mods connected at the moment");
            }
            else {
                for (const auto& modName : uniqueMods) {
                    if (modSearchLower.empty() || ToLower(modName).find(modSearchLower) != std::string::npos) {
                        if (ImGuiMCP::Selectable(modName.c_str(), dashSelectedModFilter == modName)) {
                            dashSelectedModFilter = modName;
                        }
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

            // Aplica filtro de Nome
            if (!lowerDashFilterName.empty() && ToLower(action.name).find(lowerDashFilterName) == std::string::npos) {
                continue;
            }

            // Aplica filtro de Mod
            bool modMatch = dashSelectedModFilter.empty();
            if (!dashSelectedModFilter.empty()) {
                for (const auto& l : listeners) {
                    if (l.actionID == static_cast<int>(i) && l.modName == dashSelectedModFilter) {
                        modMatch = true;
                        break;
                    }
                }
            }
            if (!modMatch) continue;

            auto addKey = [&](const std::string& keyName) {
                if (!keyName.empty() && keyName != "None") {
                    // Aplica filtro de Key
                    if (!dashSelectedKeyFilter.empty() && keyName != dashSelectedKeyFilter) return;

                    auto& vec = keyUsageMap[keyName].actionIDs;
                    if (std::find(vec.begin(), vec.end(), static_cast<int>(i)) == vec.end()) {
                        vec.push_back(static_cast<int>(i));
                    }
                }
                };

            if (action.pcMainKey != 0) {
                int idx = GetIndexFromID(action.pcMainKey, pcKeyIDs, std::size(pcKeyIDs));
                addKey(std::string(pcKeyNames[idx]) + " (PC)");
            }
            if (action.pcModifierKey != 0) {
                int idx = GetIndexFromID(action.pcModifierKey, pcKeyIDs, std::size(pcKeyIDs));
                addKey(std::string(pcKeyNames[idx]) + " (PC)");
            }
            if (action.gamepadMainKey != 0) {
                int idx = GetIndexFromID(action.gamepadMainKey, gamepadKeyIDs, std::size(gamepadKeyIDs));
                addKey(std::string(gamepadKeyNames[idx]) + " (PAD)");
            }
            if (action.gamepadModifierKey != 0) {
                int idx = GetIndexFromID(action.gamepadModifierKey, gamepadKeyIDs, std::size(gamepadKeyIDs));
                addKey(std::string(gamepadKeyNames[idx]) + " (PAD)");
            }
        }

        for (size_t i = 0; i < motionList.size(); ++i) {
            const auto& motion = motionList[i];
            if (motion.pcSequence.empty() && motion.padSequence.empty()) continue;

            // Aplica filtro de Nome
            if (!lowerDashFilterName.empty() && ToLower(motion.name).find(lowerDashFilterName) == std::string::npos) {
                continue;
            }

            // Aplica filtro de Mod
            bool modMatch = dashSelectedModFilter.empty();
            if (!dashSelectedModFilter.empty()) {
                for (const auto& l : motionListeners) {
                    if (l.actionID == static_cast<int>(i) && l.modName == dashSelectedModFilter) {
                        modMatch = true;
                        break;
                    }
                }
            }
            if (!modMatch) continue;

            auto addMotionKey = [&](uint32_t keyID, bool isPad) {
                std::string keyName = FormatMotionKey(keyID) + (isPad ? " (PAD)" : " (PC)");

                // Aplica filtro de Key
                if (!dashSelectedKeyFilter.empty() && keyName != dashSelectedKeyFilter) return;

                auto& vec = keyUsageMap[keyName].motionIDs;
                if (std::find(vec.begin(), vec.end(), static_cast<int>(i)) == vec.end()) {
                    vec.push_back(static_cast<int>(i));
                }
                };

            for (uint32_t k : motion.pcSequence) addMotionKey(k, false);
            for (uint32_t k : motion.padSequence) addMotionKey(k, true);
        }

        if (keyUsageMap.empty()) {
            ImGuiMCP::TextDisabled("No results match the selected filters.");
            return;
        }

        if (ImGuiMCP::BeginTable("SummaryTable", 4, ImGuiMCP::ImGuiTableFlags_Borders | ImGuiMCP::ImGuiTableFlags_RowBg | ImGuiMCP::ImGuiTableFlags_Resizable)) {
            ImGuiMCP::TableSetupColumn("Action / ID", ImGuiMCP::ImGuiTableColumnFlags_WidthFixed, 180.0f);
            ImGuiMCP::TableSetupColumn("Keys and Combos", ImGuiMCP::ImGuiTableColumnFlags_WidthStretch);
            ImGuiMCP::TableSetupColumn("Timings", ImGuiMCP::ImGuiTableColumnFlags_WidthFixed, 130.0f);
            ImGuiMCP::TableSetupColumn("Connected Mods", ImGuiMCP::ImGuiTableColumnFlags_WidthStretch);
            ImGuiMCP::TableHeadersRow();

            for (const auto& pair : keyUsageMap) {
                const std::string& keyName = pair.first;
                const std::vector<int>& actionIDs = pair.second.actionIDs;
                const std::vector<int>& motionIDs = pair.second.motionIDs;

                ImGuiMCP::TableNextRow();
                ImGuiMCP::TableSetColumnIndex(0);

                ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, { 0.8f, 0.8f, 1.0f, 1.0f });
                bool isNodeOpen = ImGuiMCP::TreeNodeEx(keyName.c_str(), ImGuiMCP::ImGuiTreeNodeFlags_SpanFullWidth | ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen);
                ImGuiMCP::PopStyleColor();

                if (isNodeOpen) {

                    for (int actionID : actionIDs) {
                        const auto& action = actionList[actionID];

                        ImGuiMCP::TableNextRow();
                        ImGuiMCP::TableSetColumnIndex(0);
                        ImGuiMCP::Indent();
                        ImGuiMCP::TextColored({ 0.7f, 0.7f, 0.7f, 1.0f }, "ID: %d - %s", actionID, action.name);
                        ImGuiMCP::Unindent();

                        ImGuiMCP::TableSetColumnIndex(1);
                        ImGuiMCP::TextWrapped("%s", GetActionSummary(action).c_str());

                        ImGuiMCP::TableSetColumnIndex(2);
                        ImGuiMCP::Text("Window: %.2fs\nHold: %.2fs", action.tapWindow, action.holdDuration);

                        ImGuiMCP::TableSetColumnIndex(3);
                        bool hasMod = false;
                        for (const auto& l : listeners) {
                            if (l.actionID == actionID) {
                                ImGuiMCP::BulletText("%s (%s)", l.modName.c_str(), l.purpose.c_str());
                                hasMod = true;
                            }
                        }
                        if (!hasMod) ImGuiMCP::TextDisabled("None");
                    }

                    for (int motionID : motionIDs) {
                        const auto& motion = motionList[motionID];

                        ImGuiMCP::TableNextRow();
                        ImGuiMCP::TableSetColumnIndex(0);
                        ImGuiMCP::Indent();
                        ImGuiMCP::TextColored({ 0.9f, 0.7f, 0.3f, 1.0f }, "Motion ID: %d", motionID);
                        ImGuiMCP::TextColored({ 0.7f, 0.7f, 0.7f, 1.0f }, "%s", motion.name);
                        ImGuiMCP::Unindent();

                        ImGuiMCP::TableSetColumnIndex(1);
                        std::string seqSummary = "";
                        if (!motion.pcSequence.empty()) {
                            seqSummary += "PC: ";
                            for (uint32_t k : motion.pcSequence) seqSummary += "[" + FormatMotionKey(k) + "] ";
                        }
                        if (!motion.padSequence.empty()) {
                            if (!seqSummary.empty()) seqSummary += "  |  ";
                            seqSummary += "PAD: ";
                            for (uint32_t k : motion.padSequence) seqSummary += "[" + FormatMotionKey(k) + "] ";
                        }
                        ImGuiMCP::TextWrapped("%s", seqSummary.empty() ? "None" : seqSummary.c_str());

                        ImGuiMCP::TableSetColumnIndex(2);
                        ImGuiMCP::Text("Time Window: %.2fs", motion.timeWindow);

                        ImGuiMCP::TableSetColumnIndex(3);
                        bool hasMod = false;
                        for (const auto& l : motionListeners) {
                            if (l.actionID == motionID) {
                                ImGuiMCP::BulletText("%s (%s)", l.modName.c_str(), l.purpose.c_str());
                                hasMod = true;
                            }
                        }
                        if (!hasMod) ImGuiMCP::TextDisabled("None");
                    }

                    ImGuiMCP::TreePop();
                }
            }
            ImGuiMCP::EndTable();
        }
    }

    inline void RenderDashboardMenu() {
        ImGuiMCP::Text("Input Manager - Dashboard");
        ImGuiMCP::Separator(); ImGuiMCP::Spacing();

        auto listeners = PluginLogic::KeyManager::GetSingleton()->GetListeners();
        auto motionListeners = PluginLogic::KeyManager::GetSingleton()->GetMotionListeners(); 

        RenderSummaryDashboard(listeners, motionListeners); 
    }

    inline void RenderGesturesMenu() {
        ImGuiMCP::Text("Input Manager - Gestures and Movements (Beta)");
        ImGuiMCP::Separator(); ImGuiMCP::Spacing();

        bool hasGestureConflict = false;
        std::string gestureErrorMsg;
        for (size_t i = 0; i < movementList.size(); ++i) {
            for (size_t j = i + 1; j < movementList.size(); ++j) {
                if (SanitizeFileName(movementList[i].name) == SanitizeFileName(movementList[j].name)) {
                    hasGestureConflict = true;
                    gestureErrorMsg = "DUPLICATE NAME: Gestures '" + std::string(movementList[i].name) + "' and '" + std::string(movementList[j].name) + "' conflict. Please choose unique names.";
                    break;
                }
            }
            if (hasGestureConflict) break;
        }

        if (hasGestureConflict) {
            ImGuiMCP::TextColored({ 1.0f, 0.2f, 0.2f, 1.0f }, "[CONFIGURATION ERROR]");
            ImGuiMCP::TextColored({ 1.0f, 0.8f, 0.2f, 1.0f }, "%s", gestureErrorMsg.c_str());
            ImGuiMCP::TextColored({ 0.7f, 0.7f, 0.7f, 1.0f }, "Resolve the duplication by changing the gesture's name.");
            ImGuiMCP::Spacing(); ImGuiMCP::Separator(); ImGuiMCP::Spacing();
        }


        ImGuiMCP::BeginDisabled(hasGestureConflict);

        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Button, { 0.1f, 0.5f, 0.1f, 1.0f });
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_ButtonHovered, { 0.2f, 0.6f, 0.2f, 1.0f });
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_ButtonActive, { 0.1f, 0.4f, 0.1f, 1.0f });

        if (ImGuiMCP::Button("Save and Apply Gestures")) {
            SaveSettingsToJson();
            SaveGesturesToJson();
        }

        ImGuiMCP::PopStyleColor(3);

        ImGuiMCP::EndDisabled();

        ImGuiMCP::SameLine();
        if (ImGuiMCP::Button("Add New Gesture")) {
            MovementEntry newGesture;
            std::string baseName = "New Gesture";
            std::string finalName = baseName;
            int counter = 1;

            bool exists = true;
            while (exists) {
                exists = false;
                for (const auto& g : movementList) {
                    if (std::string(g.name) == finalName) {
                        exists = true;
                        break;
                    }
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

        ImGuiMCP::Checkbox("Show Gesture Trail in Game (Overlay)", &showGestureTrail);
        if (ImGuiMCP::IsItemHovered()) {
            ImGuiMCP::SetTooltip("Draws a blue line on the screen while the gesture shortcut is pressed.\nFor this to work, remember to call ActionMenuUI::RenderGestureTrailOverlay()\nin your main ImGui hook (OnPresent).");
        }

        ImGuiMCP::Spacing(); ImGuiMCP::Separator(); ImGuiMCP::Spacing();

        static int recordingIndex = -1;
        static int testingIndex = -1;
        static bool isWaitingForEnter = false;
        static std::vector<GestureMath::Point2D> tempPoints;
        static float lastScore = -1.0f;

        bool isCapturing = (recordingIndex != -1 || testingIndex != -1);

        if (isCapturing) {
            ImGuiMCP::TextColored({ 1.0f, 0.5f, 0.0f, 1.0f }, ">>> GESTURE CAPTURE MODE <<<");

            if (isWaitingForEnter) {
                ImGuiMCP::Text("Position the mouse where desired and press [ENTER] to START recording.");
                if (ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_Enter)) {
                    isWaitingForEnter = false; 
                }
            }
            else {
                ImGuiMCP::TextColored({ 1.0f, 0.2f, 0.2f, 1.0f }, "[RECORDING] Move the mouse. Press [ENTER] again to FINISH.");

                auto io = ImGuiMCP::GetIO();
                float mouseX = io->MousePos.x;
                float mouseY = io->MousePos.y;

                if (tempPoints.empty()) {
                    tempPoints.push_back({ mouseX, mouseY });
                }
                else {
                    float dx = mouseX - tempPoints.back().x;
                    float dy = mouseY - tempPoints.back().y;
                    if (dx * dx + dy * dy > 4.0f) {
                        tempPoints.push_back({ mouseX, mouseY });
                    }
                }

                if (tempPoints.size() > 1) {
                    ImGuiMCP::ImVec2 oldPos;
                    ImGuiMCP::GetCursorScreenPos(&oldPos); // Salva a posição exata da UI antes de desenhar
                    ImGuiMCP::ImDrawList* draw_list = ImGuiMCP::GetWindowDrawList();
                    size_t totalPoints = tempPoints.size();
                    for (size_t i = 1; i < totalPoints; ++i) {
                        ImGuiMCP::ImVec2 p1 = { tempPoints[i - 1].x, tempPoints[i - 1].y };
                        ImGuiMCP::ImVec2 p2 = { tempPoints[i].x, tempPoints[i].y };

                        // 2. Calcula a cor (Gradiente dinâmico como no seu original)
                        float progress = (float)i / (totalPoints - 1);
                        // Assumindo que você tem acesso ao namespace ImGui nativo para pegar a cor:
                        ImGuiMCP::ImU32 lineColor = ImGuiMCP::GetColorU32({ progress, 1.0f - progress, 0.0f, 1.0f });

                        float thickness = 3.0f; // Ajuste a grossura da linha aqui

                        // 3. PASSE O DRAW_LIST COMO O PRIMEIRO ARGUMENTO
                        ImGuiMCP::ImDrawListManager::AddLine(draw_list, p1, p2, lineColor, thickness);
                    }
                    ImGuiMCP::SetCursorScreenPos(oldPos); // Restaura o cursor!
                }

                // When Enter is pressed for the SECOND TIME, it saves and finishes
                if (ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_Enter)) {
                    if (recordingIndex != -1) {
                        movementList[recordingIndex].rawPoints = tempPoints;
                        movementList[recordingIndex].normalizedPoints = GestureMath::NormalizeGesture(tempPoints);
                    }
                    else if (testingIndex != -1) {
                        std::vector<GestureMath::Point2D> testNorm = GestureMath::NormalizeGesture(tempPoints);
                        lastScore = GestureMath::GetMatchScore(movementList[testingIndex].normalizedPoints, testNorm);
                    }

                    recordingIndex = -1;
                    testingIndex = -1;
                    tempPoints.clear();
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
                ImGuiMCP::Indent();
                ImGuiMCP::Spacing();

                ImGuiMCP::InputText("Gesture Name", gesture.name, sizeof(gesture.name));

                // --- INÍCIO: SISTEMA DE REORDENAMENTO (MUDAR ID) ---
                ImGuiMCP::Spacing();
                int currentId = static_cast<int>(i);
                ImGuiMCP::PushItemWidth(200.0f);
                if (ImGuiMCP::InputInt("Gesture ID", &currentId, 1, 10, ImGuiMCP::ImGuiInputTextFlags_EnterReturnsTrue)) {
                    if (currentId >= 0 && currentId < static_cast<int>(movementList.size()) && currentId != i) {
                        std::swap(movementList[i], movementList[currentId]);
                    }
                }
                ImGuiMCP::PopItemWidth();
                if (ImGuiMCP::IsItemHovered()) ImGuiMCP::SetTooltip("Type the ID and press ENTER to swap gestures.");
                ImGuiMCP::SameLine();

                ImGuiMCP::BeginDisabled(i == 0);
                if (ImGuiMCP::Button("Move Up##gest")) std::swap(movementList[i], movementList[i - 1]);
                ImGuiMCP::EndDisabled();
                ImGuiMCP::SameLine();

                ImGuiMCP::BeginDisabled(i == movementList.size() - 1);
                if (ImGuiMCP::Button("Move Down##gest")) std::swap(movementList[i], movementList[i + 1]);
                ImGuiMCP::EndDisabled();

                ImGuiMCP::Spacing();
                ImGuiMCP::SliderFloat("Required Accuracy", &gesture.requiredAccuracy, 0.50f, 0.99f, "%.2f");
                ImGuiMCP::Spacing();

                ImGuiMCP::Text("Recorded Drawing (Preview):");
                if (!gesture.normalizedPoints.empty()) {
                    ImGuiMCP::ImVec2 p0;
                    ImGuiMCP::GetCursorScreenPos(&p0);
                    ImGuiMCP::Dummy({ 240.0f, 240.0f });
                    ImGuiMCP::ImVec2 pEnd;
                    ImGuiMCP::GetCursorScreenPos(&pEnd);
                    float minX = 99999.0f, maxX = -99999.0f;
                    float minY = 99999.0f, maxY = -99999.0f;

                    // Encontra os limites extremos do desenho
                    for (const auto& p : gesture.normalizedPoints) {
                        if (p.x < minX) minX = p.x;
                        if (p.x > maxX) maxX = p.x;
                        if (p.y < minY) minY = p.y;
                        if (p.y > maxY) maxY = p.y;
                    }

                    float rangeX = maxX - minX;
                    float rangeY = maxY - minY;
                    // Previne divisão por zero caso seja um ponto único
                    if (rangeX < 0.001f) rangeX = 0.001f;
                    if (rangeY < 0.001f) rangeY = 0.001f;

                    // Pega na maior dimensão para não distorcer a proporção
                    float maxRange = std::max(rangeX, rangeY);

                    // Escala personalizada: O desenho vai caber sempre em 160px (com margem dentro dos 240px)
                    float scale = 160.0f / maxRange;

                    // Centro geométrico do Canvas de 240x240
                    float centerX = p0.x + 120.0f;
                    float centerY = p0.y + 120.0f;

                    // Centro matemático do desenho
                    float midX = minX + (rangeX * 0.5f);
                    float midY = minY + (rangeY * 0.5f);

                    // 4. Desenha as linhas perfeitamente centralizadas e escaladas
                    size_t totalPoints = gesture.normalizedPoints.size();

                    if (totalPoints > 1) {
                        // Obtém a lista de desenho (caso não tenha pego no início do escopo)
                        ImGuiMCP::ImDrawList* draw_list = ImGuiMCP::GetWindowDrawList();

                        for (size_t j = 1; j < totalPoints; ++j) {
                            auto p1 = gesture.normalizedPoints[j - 1];
                            auto p2 = gesture.normalizedPoints[j];

                            // Aplica a escala dinâmica centralizando a partir do midX e midY
                            float x1 = centerX + ((p1.x - midX) * scale);
                            float y1 = centerY + ((p1.y - midY) * scale);
                            float x2 = centerX + ((p2.x - midX) * scale);
                            float y2 = centerY + ((p2.y - midY) * scale);

                            // Converte as coordenadas escaladas para ImVec2
                            ImGuiMCP::ImVec2 vec1 = { x1, y1 };
                            ImGuiMCP::ImVec2 vec2 = { x2, y2 };

                            // Calcula o progresso para o gradiente de cor (Verde para Vermelho, ou vice-versa)
                            float progress = (float)j / (totalPoints - 1);

                            // Cria a cor. Note que usamos o ImGui nativo para converter o ImVec4 para ImU32
                            ImGuiMCP::ImU32 lineColor = ImGuiMCP::GetColorU32({ progress, 1.0f - progress, 0.0f, 1.0f });

                            // Define a espessura da linha
                            float thickness = 3.0f;

                            // Desenha a linha contínua
                            ImGuiMCP::ImDrawListManager::AddLine(draw_list, vec1, vec2, lineColor, thickness);
                        }
                    }
                    ImGuiMCP::SetCursorScreenPos(pEnd);
                }
                else {
                    ImGuiMCP::TextColored({ 0.6f, 0.6f, 0.6f, 1.0f }, "[ NO GESTURE RECORDED ]");
                }

                ImGuiMCP::Spacing(); ImGuiMCP::Separator(); ImGuiMCP::Spacing();

                if (ImGuiMCP::Button("Record Movement")) {
                    recordingIndex = static_cast<int>(i);
                    testingIndex = -1;
                    tempPoints.clear();
                    lastScore = -1.0f;
                    isWaitingForEnter = true;
                }
                ImGuiMCP::SameLine();
                if (ImGuiMCP::Button("Validate Movement (Test)")) {
                    testingIndex = static_cast<int>(i);
                    recordingIndex = -1;
                    tempPoints.clear();
                    lastScore = -1.0f;
                    isWaitingForEnter = true;
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

                std::vector<std::string> connectedActions;
                for (const auto& act : actionList) {
                    if ((act.pcMainKey != 0 && act.pcModAction == 3 && act.gestureIndex == static_cast<int>(i)) ||
                        (act.gamepadMainKey != 0 && act.gamepadModAction == 3 && act.gestureIndex == static_cast<int>(i))) {
                        connectedActions.push_back(act.name);
                    }
                }
                if (!connectedActions.empty()) {
                    ImGuiMCP::Spacing(); ImGuiMCP::Separator(); ImGuiMCP::Spacing();
                    ImGuiMCP::TextColored({ 0.9f, 0.6f, 1.0f, 1.0f }, "Actions using this Gesture:");
                    for (const auto& actName : connectedActions) {
                        ImGuiMCP::BulletText("%s", actName.c_str());
                    }
                }

                ImGuiMCP::Spacing(); ImGuiMCP::Separator(); ImGuiMCP::Spacing();
                ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Button, { 0.6f, 0.2f, 0.2f, 1.0f });
                if (ImGuiMCP::Button("Delete Gesture")) {
                    movementList.erase(movementList.begin() + i);
                    ImGuiMCP::PopStyleColor();
                    ImGuiMCP::Unindent();
                    ImGuiMCP::PopID();
                    break;
                }
                ImGuiMCP::PopStyleColor();

                ImGuiMCP::Unindent();
                ImGuiMCP::Spacing();
            }
            else {
                ImGuiMCP::PopStyleColor();
            }
            ImGuiMCP::PopID();
        }

        if (isCapturing) {
            ImGuiMCP::EndDisabled();
        }
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

                        if (aModAct == 0 || aModAct == 3) aModKey = 0;
                        if (bModAct == 0 || bModAct == 3) bModKey = 0;

                        if (aMKey == bMKey && aMAct == bMAct) {
                            bool mainTapMatch = (aMAct == 1) ? (aMTap == bMTap) : true;

                            if (mainTapMatch) {
                                if (aModAct == 3 && bModAct == 3) {
                                    return aGest == bGest;
                                }
                                else if (aModAct != 3 && bModAct != 3) {
                                    bool modTapMatch = (aModAct == 1) ? (aModTap == bModTap) : true;
                                    return aModKey == bModKey && aModAct == bModAct && modTapMatch;
                                }
                            }
                        }

                        if (aModAct != 3 && bModAct != 3 && aModKey != 0 && bModKey != 0) {
                            bool crossTapMatch1 = (aMAct == 1) ? (aMTap == bModTap) : true;
                            bool crossTapMatch2 = (aModAct == 1) ? (aModTap == bMTap) : true;

                            if (aMKey == bModKey && aMAct == bModAct && crossTapMatch1 &&
                                aModKey == bMKey && aModAct == bMAct && crossTapMatch2) {
                                return true;
                            }
                        }
                        return false;
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

            // Limita o valor customizado ao máximo global
            if (a.useCustomTimings && a.tapWindow > globalTapWindow) {
                a.tapWindow = globalTapWindow;
            }

            for (size_t j = 0; j < actionList.size(); ++j) {
                if (i == j) continue;
                auto& b = actionList[j];

                // REGRA 1: HOLD CONFLITANTE COM OUTRA AÇÃO NA MESMA TECLA
                // (Se houver um Hold na mesma tecla de um Tap, o tempo de Hold precisa ser alto o suficiente)
                if (a.pcMainKey != 0 && a.pcMainAction == 2 && a.pcModifierKey == 0) { // 2 = Hold
                    if ((b.pcModifierKey == a.pcMainKey) || (b.pcMainKey == a.pcMainKey && b.pcModifierKey != 0)) {
                        if (a.holdDuration < 1.0f) a.holdDuration = 1.0f;
                    }
                }
                if (a.gamepadMainKey != 0 && a.gamepadMainAction == 2 && a.gamepadModifierKey == 0) {
                    if ((b.gamepadModifierKey == a.gamepadMainKey) || (b.gamepadMainKey == a.gamepadMainKey && b.gamepadModifierKey != 0)) {
                        if (a.holdDuration < 1.0f) a.holdDuration = 1.0f;
                    }
                }

                // REGRA 2: TAP ÚNICO CONFLITANTE COM MÚLTIPLOS TAPS
                // (Se a Ação A exige 1 Tap, mas a Ação B exige 2 ou mais na MESMA tecla, a Ação A PRECISA de Delay)
                if (a.pcMainKey != 0 && a.pcMainAction == 1 && a.pcMainTapCount == 1 && a.pcModifierKey == 0) {
                    if (b.pcMainKey == a.pcMainKey && b.pcMainAction == 1 && b.pcMainTapCount > 1 && b.pcModifierKey == 0) {
                        a.pcDelayTap = true;
                    }
                }
                if (a.gamepadMainKey != 0 && a.gamepadMainAction == 1 && a.gamepadMainTapCount == 1 && a.gamepadModifierKey == 0) {
                    if (b.gamepadMainKey == a.gamepadMainKey && b.gamepadMainAction == 1 && b.gamepadMainTapCount > 1 && b.gamepadModifierKey == 0) {
                        a.gamepadDelayTap = true;
                    }
                }

                // REGRA 3: LIMPEZA DE MODIFICADORES DUPLOS (Garante que botões parceiros exijam Delay correto)
                if (b.pcMainKey != 0 && b.pcModifierKey != 0 && b.pcMainAction == 1 && b.pcModAction == 1) {
                    b.pcDelayTap = false; // Combos de duas teclas não precisam de delay
                    if (a.pcModifierKey == 0 && a.pcMainAction == 1 && a.pcMainTapCount == 1) {
                        if (a.pcMainKey == b.pcMainKey || a.pcMainKey == b.pcModifierKey) {
                            a.pcDelayTap = true;
                        }
                    }
                }
                if (b.gamepadMainKey != 0 && b.gamepadModifierKey != 0 && b.gamepadMainAction == 1 && b.gamepadModAction == 1) {
                    b.gamepadDelayTap = false;
                    if (a.gamepadModifierKey == 0 && a.gamepadMainAction == 1 && a.gamepadMainTapCount == 1) {
                        if (a.gamepadMainKey == b.gamepadMainKey || a.gamepadMainKey == b.gamepadModifierKey) {
                            a.gamepadDelayTap = true;
                        }
                    }
                }
            }
        }
    }

    inline void RenderMenu() {
        ImGuiMCP::Text("Input Manager");
        ImGuiMCP::Separator(); ImGuiMCP::Spacing();

        ImGuiMCP::Checkbox("Advanced Mode (Disable Safe Guards)", &advancedMode);
        if (!advancedMode) {
            EnforceSafeGuards();
            ImGuiMCP::SameLine();
            ImGuiMCP::TextColored({ 0.5f, 1.0f, 0.5f, 1.0f }, " (Safe Guards Active: Preventing logic conflicts)");
        }
        else {
            ImGuiMCP::SameLine();
            ImGuiMCP::TextColored({ 1.0f, 0.5f, 0.5f, 1.0f }, " (WARNING: Safe Guards Disabled. Risk of breaking!)");
        }

        ImGuiMCP::Spacing(); ImGuiMCP::Separator(); ImGuiMCP::Spacing();

        ImGuiMCP::TextColored({ 0.8f, 0.8f, 0.5f, 1.0f }, "Global Timings Defaults");
        ImGuiMCP::SliderFloat("Global Tap Window", &globalTapWindow, 0.01f, 1.0f, "%.2f s");
        ImGuiMCP::SliderFloat("Global Hold Duration", &globalHoldDuration, 0.01f, 2.0f, "%.2f s");
        ImGuiMCP::Spacing(); ImGuiMCP::Separator(); ImGuiMCP::Spacing();

        std::string errorMsg;
        bool hasConflict = HasConflicts(errorMsg);

        if (hasConflict) {
            ImGuiMCP::TextColored({ 1.0f, 0.2f, 0.2f, 1.0f }, "[CONFIGURATION ERROR]");
            ImGuiMCP::TextColored({ 1.0f, 0.8f, 0.2f, 1.0f }, "%s", errorMsg.c_str());
            ImGuiMCP::TextColored({ 0.7f, 0.7f, 0.7f, 1.0f }, "Resolve the duplicate by changing the key or State.");
            ImGuiMCP::Spacing(); ImGuiMCP::Separator(); ImGuiMCP::Spacing();
        }

        ImGuiMCP::BeginDisabled(hasConflict);
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Button, { 0.1f, 0.5f, 0.1f, 1.0f });
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_ButtonHovered, { 0.2f, 0.6f, 0.2f, 1.0f });
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_ButtonActive, { 0.1f, 0.4f, 0.1f, 1.0f });

        if (ImGuiMCP::Button("Save and Apply Changes")) {
            SaveSettingsToJson();
            SaveActionsToJson();
        }
        ImGuiMCP::PopStyleColor(3);
        ImGuiMCP::EndDisabled();

        ImGuiMCP::SameLine();
        if (ImGuiMCP::Button("Add New Action")) {
            ActionEntry newAction;
            std::string baseName = "New Action";
            std::string finalName = baseName;
            int counter = 1;

            // Loop to ensure a unique name
            bool exists = true;
            while (exists) {
                exists = false;
                for (const auto& a : actionList) {
                    if (std::string(a.name) == finalName) {
                        exists = true;
                        break;
                    }
                }
                if (exists) {
                    finalName = baseName + " " + std::to_string(counter);
                    counter++;
                }
            }

            strncpy_s(newAction.name, finalName.c_str(), sizeof(newAction.name) - 1);
            actionList.push_back(newAction);
        }
        ImGuiMCP::Spacing();
        ImGuiMCP::Separator();
        ImGuiMCP::Spacing();

        auto listeners = PluginLogic::KeyManager::GetSingleton()->GetListeners();
        std::vector<std::string> uniqueMods;
        for (const auto& l : listeners) {
            if (std::find(uniqueMods.begin(), uniqueMods.end(), l.modName) == uniqueMods.end()) {
                uniqueMods.push_back(l.modName);
            }
        }

        static char filterName[128] = "";
        static std::string selectedKeyFilter = "";
        static char keySearchBuf[128] = "";
        static std::string selectedModFilter = "";
        static char modSearchBuf[128] = "";

        if (!selectedModFilter.empty() && std::find(uniqueMods.begin(), uniqueMods.end(), selectedModFilter) == uniqueMods.end()) {
            selectedModFilter = "";
        }

        ImGuiMCP::TextColored({ 0.5f, 0.8f, 1.0f, 1.0f }, "Search Filters:");

        ImGuiMCP::PushItemWidth(180.0f);

        ImGuiMCP::InputText("Name", filterName, sizeof(filterName));
        ImGuiMCP::SameLine();

        if (ImGuiMCP::BeginCombo("Key", selectedKeyFilter.empty() ? "All Keys" : selectedKeyFilter.c_str())) {
            ImGuiMCP::InputText("Search##keySearch", keySearchBuf, sizeof(keySearchBuf));
            std::string searchLower = ToLower(keySearchBuf);
            ImGuiMCP::Separator();

            if (ImGuiMCP::Selectable("All Keys", selectedKeyFilter.empty())) {
                selectedKeyFilter = "";
            }

            for (size_t k = 1; k < std::size(pcKeyNames); ++k) {
                std::string kName = pcKeyNames[k];
                if (searchLower.empty() || ToLower(kName).find(searchLower) != std::string::npos) {
                    if (ImGuiMCP::Selectable((kName + " (PC)").c_str(), selectedKeyFilter == kName)) {
                        selectedKeyFilter = kName;
                    }
                }
            }
            for (size_t k = 1; k < std::size(gamepadKeyNames); ++k) {
                std::string kName = gamepadKeyNames[k];
                if (searchLower.empty() || ToLower(kName).find(searchLower) != std::string::npos) {
                    if (ImGuiMCP::Selectable((kName + " (PAD)").c_str(), selectedKeyFilter == kName)) {
                        selectedKeyFilter = kName;
                    }
                }
            }
            ImGuiMCP::EndCombo();
        }
        ImGuiMCP::SameLine();

        if (ImGuiMCP::BeginCombo("Mod", selectedModFilter.empty() ? "All Mods" : selectedModFilter.c_str())) {
            ImGuiMCP::InputText("Search##modSearch", modSearchBuf, sizeof(modSearchBuf));
            std::string modSearchLower = ToLower(modSearchBuf);
            ImGuiMCP::Separator();

            if (ImGuiMCP::Selectable("All Mods", selectedModFilter.empty())) {
                selectedModFilter = "";
            }
            if (uniqueMods.empty()) {
                ImGuiMCP::TextDisabled("No mods connected at the moment");
            }
            else {
                for (const auto& modName : uniqueMods) {
                    if (modSearchLower.empty() || ToLower(modName).find(modSearchLower) != std::string::npos) {
                        if (ImGuiMCP::Selectable(modName.c_str(), selectedModFilter == modName)) {
                            selectedModFilter = modName;
                        }
                    }
                }
            }
            ImGuiMCP::EndCombo();
        }
        ImGuiMCP::PopItemWidth();

        ImGuiMCP::Spacing(); ImGuiMCP::Separator(); ImGuiMCP::Spacing();

        std::string lowerFilterName = ToLower(filterName);

        for (size_t i = 0; i < actionList.size(); ++i) {
            auto& action = actionList[i];

            if (!lowerFilterName.empty() && ToLower(action.name).find(lowerFilterName) == std::string::npos) {
                continue;
            }

            if (!HasKeyConfigured(action, selectedKeyFilter)) {
                continue;
            }

            std::vector<PluginLogic::ModListener> activeListeners;
            bool modMatch = selectedModFilter.empty();

            for (const auto& l : listeners) {
                if (l.actionID == static_cast<int>(i)) {
                    activeListeners.push_back(l);
                    if (!selectedModFilter.empty() && l.modName == selectedModFilter) {
                        modMatch = true;
                    }
                }
            }
            if (!selectedModFilter.empty() && !modMatch) {
                continue;
            }

            ImGuiMCP::PushID(static_cast<int>(i));

            std::string actionSummary = GetActionSummary(action);
            std::string headerLabel = "[" + std::to_string(i) + "] " + std::string(action.name) + "   -   " + actionSummary + "###actionHeader_" + std::to_string(i);
            ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Header, { 0.2f, 0.2f, 0.2f, 1.0f });

            if (ImGuiMCP::CollapsingHeader(headerLabel.c_str())) {
                ImGuiMCP::PopStyleColor();
                ImGuiMCP::Indent();
                ImGuiMCP::Spacing();

                ImGuiMCP::InputText("Action Name", action.name, sizeof(action.name));

                ImGuiMCP::Spacing();

                int currentId = static_cast<int>(i);
                ImGuiMCP::PushItemWidth(200.0f);
                if (ImGuiMCP::InputInt("Action ID", &currentId, 1, 10, ImGuiMCP::ImGuiInputTextFlags_EnterReturnsTrue)) {
                    if (currentId >= 0 && currentId < static_cast<int>(actionList.size()) && currentId != i) {
                        std::swap(actionList[i], actionList[currentId]);
                    }
                }
                ImGuiMCP::PopItemWidth();
                if (ImGuiMCP::IsItemHovered()) ImGuiMCP::SetTooltip("Type the ID and press ENTER to swap actions.");
                ImGuiMCP::SameLine();

                ImGuiMCP::BeginDisabled(i == 0);
                if (ImGuiMCP::Button("Move Up")) std::swap(actionList[i], actionList[i - 1]);
                ImGuiMCP::EndDisabled();
                ImGuiMCP::SameLine();

                ImGuiMCP::BeginDisabled(i == actionList.size() - 1);
                if (ImGuiMCP::Button("Move Down")) std::swap(actionList[i], actionList[i + 1]);
                ImGuiMCP::EndDisabled();
                ImGuiMCP::SameLine();

                ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Button, { 0.6f, 0.2f, 0.2f, 1.0f });
                ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_ButtonHovered, { 0.8f, 0.3f, 0.3f, 1.0f });
                if (ImGuiMCP::Button("Delete Action")) {
                    actionList.erase(actionList.begin() + i);
                    ImGuiMCP::PopStyleColor(2);
                    ImGuiMCP::Unindent();
                    ImGuiMCP::PopID();
                    break;
                }
                ImGuiMCP::PopStyleColor(2);
                ImGuiMCP::Spacing(); ImGuiMCP::Separator(); ImGuiMCP::Spacing();

                ImGuiMCP::TextColored({ 0.5f, 0.8f, 1.0f, 1.0f }, "Keyboard and Mouse");
                bool canGesturePC = (action.pcMainAction == 2 || action.pcMainAction == 4);
                if (!canGesturePC && action.pcModAction == 3) action.pcModAction = 0;

                if (ImGuiMCP::BeginTable("PCTable", 2, ImGuiMCP::ImGuiTableFlags_BordersInnerH)) {
                    ImGuiMCP::TableSetupColumn("Key / Trigger", ImGuiMCP::ImGuiTableColumnFlags_WidthFixed, 200.0f);
                    ImGuiMCP::TableSetupColumn("State", ImGuiMCP::ImGuiTableColumnFlags_WidthStretch);

                    ImGuiMCP::TableNextRow();
                    ImGuiMCP::TableSetColumnIndex(0);
                    int pcMainIdx = GetIndexFromID(action.pcMainKey, pcKeyIDs, std::size(pcKeyIDs));
                    ImGuiMCP::SetNextItemWidth(180.0f);
                    if (SearchableCombo("##pcMKey", &pcMainIdx, pcKeyNames, std::size(pcKeyNames))) {
                        int selectedKey = pcKeyIDs[pcMainIdx];
                        if (selectedKey != 0 && selectedKey == action.pcModifierKey) action.pcModifierKey = 0;
                        action.pcMainKey = selectedKey;
                    }

                    ImGuiMCP::TableSetColumnIndex(1);
                    ImGuiMCP::SetNextItemWidth(150.0f);
                    if (action.pcMainKey == 0) action.pcMainAction = 0;
                    ImGuiMCP::BeginDisabled(action.pcMainKey == 0);
                    if (ImGuiMCP::Combo("##pcMAct", &action.pcMainAction, actionStateNames, 5)) {
                        if (action.pcMainAction == 3) action.pcMainAction = 0; 
                    }
                    if (action.pcMainAction == 1) {
                        ImGuiMCP::SameLine();
                        ImGuiMCP::SetNextItemWidth(140.0f);
                        ImGuiMCP::InputInt("Taps##pcMainTap", &action.pcMainTapCount);
                        if (action.pcMainTapCount < 1) action.pcMainTapCount = 1;
                    }
                    ImGuiMCP::EndDisabled();

                    ImGuiMCP::TableNextRow();
                    ImGuiMCP::TableSetColumnIndex(0); ImGuiMCP::TextDisabled("   + Modifier (Optional)");

                    if (action.pcModAction == 3) {
                        std::string gesturePreview = (action.gestureIndex >= 0 && action.gestureIndex < movementList.size())
                            ? std::string(movementList[action.gestureIndex].name) : "[ No Gesture ]";

                        ImGuiMCP::SetNextItemWidth(180.0f);
                        if (ImGuiMCP::BeginCombo("##pcModKey", gesturePreview.c_str())) {
                            for (size_t gIdx = 0; gIdx < movementList.size(); ++gIdx) {
                                if (ImGuiMCP::Selectable(movementList[gIdx].name, action.gestureIndex == (int)gIdx)) {
                                    action.gestureIndex = (int)gIdx;
                                }
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

                            if (action.pcModifierKey == 0) {
                                action.pcModAction = 0;
                            }
                        }
                    }

                        ImGuiMCP::TableSetColumnIndex(1);
                        ImGuiMCP::SetNextItemWidth(150.0f);

                        if (ImGuiMCP::Combo("##pcModAct", &action.pcModAction, actionStateNames, 5)) {
                            if (!canGesturePC && action.pcModAction == 3) action.pcModAction = 0;
                            if (action.pcModAction == 0) {
                                // Regra 1: Ação é Ignore -> Tecla DEVE ser None
                                action.pcModifierKey = 0;
                            }
                            else if (action.pcModAction == 3) {
                                // Regra 2 (Gestos): Ação não é Ignore. Se não houver gesto selecionado, pega o primeiro.
                                if (action.gestureIndex < 0 && !movementList.empty()) {
                                    action.gestureIndex = 0;
                                }
                            }
                            else {
                                // Regra 2 (Tap/Hold): Ação não é Ignore. Se a tecla for None, pega a primeira válida.
                                if (action.pcModifierKey == 0) {
                                    for (size_t k = 1; k < std::size(pcKeyIDs); ++k) {
                                        if (pcKeyIDs[k] != action.pcMainKey) {
                                            action.pcModifierKey = pcKeyIDs[k];
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                        if (action.pcModAction == 1) { 
                            ImGuiMCP::SameLine();
                            ImGuiMCP::SetNextItemWidth(140.0f);
                            ImGuiMCP::InputInt("Taps##pcModTap", &action.pcModTapCount);
                            if (action.pcModTapCount < 1) action.pcModTapCount = 1;
                        }
                    
                    ImGuiMCP::EndTable();
                }
                ImGuiMCP::Checkbox("Delay##pcDelay", &action.pcDelayTap);
                ShowDelayTooltip();

                ImGuiMCP::Spacing(); ImGuiMCP::Separator(); ImGuiMCP::Spacing();

                ImGuiMCP::TextColored({ 0.5f, 1.0f, 0.5f, 1.0f }, "Gamepad");
                bool canGesturePad = (action.gamepadMainAction == 2 || action.gamepadMainAction == 4);
                if (!canGesturePad && action.gamepadModAction == 3) action.gamepadModAction = 0;

                if (ImGuiMCP::BeginTable("PadTable", 2, ImGuiMCP::ImGuiTableFlags_BordersInnerH)) {
                    ImGuiMCP::TableSetupColumn("Key / Trigger", ImGuiMCP::ImGuiTableColumnFlags_WidthFixed, 200.0f);
                    ImGuiMCP::TableSetupColumn("State", ImGuiMCP::ImGuiTableColumnFlags_WidthStretch);

                    ImGuiMCP::TableNextRow();
                    ImGuiMCP::TableSetColumnIndex(0);
                    int padMainIdx = GetIndexFromID(action.gamepadMainKey, gamepadKeyIDs, std::size(gamepadKeyIDs));
                    ImGuiMCP::SetNextItemWidth(180.0f);
                    if (SearchableCombo("##padMKey", &padMainIdx, gamepadKeyNames, std::size(gamepadKeyNames))) {
                        int selectedKey = gamepadKeyIDs[padMainIdx];
                        if (selectedKey != 0 && selectedKey == action.gamepadModifierKey) action.gamepadModifierKey = 0;
                        action.gamepadMainKey = selectedKey;
                    }

                    ImGuiMCP::TableSetColumnIndex(1);
                    ImGuiMCP::SetNextItemWidth(150.0f);
                    if (action.gamepadMainKey == 0) action.gamepadMainAction = 0;
                    ImGuiMCP::BeginDisabled(action.gamepadMainKey == 0);
                    if (ImGuiMCP::Combo("##padMAct", &action.gamepadMainAction, actionStateNames, 5)) {
                        if (action.gamepadMainAction == 3) action.gamepadMainAction = 0;
                    }
                    if (action.gamepadMainAction == 1) {
                        ImGuiMCP::SameLine();
                        ImGuiMCP::SetNextItemWidth(140.0f);
                        ImGuiMCP::InputInt("Taps##padMainTap", &action.gamepadMainTapCount);
                        if (action.gamepadMainTapCount < 1) action.gamepadMainTapCount = 1;
                    }
                    ImGuiMCP::EndDisabled();

                    ImGuiMCP::TableNextRow();
                    ImGuiMCP::TableSetColumnIndex(0); ImGuiMCP::TextDisabled("   + Modifier (Optional)");

                    if (action.gamepadModAction == 3) {
                        std::string gesturePreview = (action.gestureIndex >= 0 && action.gestureIndex < movementList.size())
                            ? std::string(movementList[action.gestureIndex].name) : "[ No Gesture ]";

                        ImGuiMCP::SetNextItemWidth(180.0f);
                        if (ImGuiMCP::BeginCombo("##padModKey", gesturePreview.c_str())) {
                            for (size_t gIdx = 0; gIdx < movementList.size(); ++gIdx) {
                                if (ImGuiMCP::Selectable(movementList[gIdx].name, action.gestureIndex == (int)gIdx)) {
                                    action.gestureIndex = (int)gIdx;
                                }
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
                            if (action.gamepadModifierKey == 0) {
                                action.gamepadModAction = 0;
                            }
                        }
                    }

                    ImGuiMCP::TableSetColumnIndex(1);
                    ImGuiMCP::SetNextItemWidth(150.0f);


                    if (ImGuiMCP::Combo("##padModAct", &action.gamepadModAction, actionStateNames, 5)) {
                        if (!canGesturePad && action.gamepadModAction == 3) action.gamepadModAction = 0;

                        if (action.gamepadModAction == 0) {
                            // Regra 1: Ação é Ignore -> Tecla DEVE ser None
                            action.gamepadModifierKey = 0;
                        }
                        else if (action.gamepadModAction == 3) {
                            // Regra 2 (Gestos): Ação não é Ignore. Se não houver gesto selecionado, pega o primeiro.
                            if (action.gestureIndex < 0 && !movementList.empty()) {
                                action.gestureIndex = 0;
                            }
                        }
                        else {
                            // Regra 2 (Tap/Hold): Ação não é Ignore. Se o botão for None, pega o primeiro válido.
                            if (action.gamepadModifierKey == 0) {
                                for (size_t k = 1; k < std::size(gamepadKeyIDs); ++k) {
                                    if (gamepadKeyIDs[k] != action.gamepadMainKey) {
                                        action.gamepadModifierKey = gamepadKeyIDs[k];
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    if (action.gamepadModAction == 1) { 
                        ImGuiMCP::SameLine();
                        ImGuiMCP::SetNextItemWidth(140.0f);
                        ImGuiMCP::InputInt("Taps##padModTap", &action.gamepadModTapCount);
                        if (action.gamepadModTapCount < 1) action.gamepadModTapCount = 1;
                    }
                    if (action.gamepadModAction == 3) {
                        ImGuiMCP::TableNextRow();
                        ImGuiMCP::TableSetColumnIndex(0);
                        ImGuiMCP::TextColored({ 0.8f, 0.8f, 0.4f, 1.0f }, "   Thumbstick for Gesture:");
                        ImGuiMCP::TableSetColumnIndex(1);
                        ImGuiMCP::RadioButton("Left##padStick", &action.gamepadGestureStick, 0);
                        ImGuiMCP::SameLine();
                        ImGuiMCP::RadioButton("Right##padStick", &action.gamepadGestureStick, 1);
                    }

                    ImGuiMCP::EndTable();
                }
                ImGuiMCP::Checkbox("Delay##padDelay", &action.gamepadDelayTap);
                ShowDelayTooltip();

                ImGuiMCP::Spacing(); ImGuiMCP::Separator(); ImGuiMCP::Spacing();

                ImGuiMCP::TextColored({ 0.8f, 0.8f, 0.5f, 1.0f }, "Response Timings");
                ImGuiMCP::Checkbox("Use Custom Timings for this Action", &action.useCustomTimings);

                float tempHold = action.useCustomTimings ? action.holdDuration : globalHoldDuration;
                float tempTap = action.useCustomTimings ? action.tapWindow : globalTapWindow;

                ImGuiMCP::BeginDisabled(!action.useCustomTimings);

                ImGuiMCP::PushItemWidth(150.0f);
                ImGuiMCP::SliderFloat("Tap Window##act", &tempTap, 0.01f, 1.0f, "%.2f");
                ImGuiMCP::PopItemWidth();

                ImGuiMCP::SameLine();

                ImGuiMCP::PushItemWidth(300.0f);
                ImGuiMCP::InputFloat("##TapInput", &tempTap, 0.01f, 0.0f, "%.2f");
                ImGuiMCP::PopItemWidth();

                ImGuiMCP::PushItemWidth(150.0f);
                ImGuiMCP::SliderFloat("Hold Time##act", &tempHold, 0.01f, 2.0f, "%.2f");
                ImGuiMCP::PopItemWidth();

                ImGuiMCP::SameLine();

                ImGuiMCP::PushItemWidth(300.0f);
                ImGuiMCP::InputFloat("##HoldInput", &tempHold, 0.01f, 0.0f, "%.2f");
                ImGuiMCP::PopItemWidth();

                ImGuiMCP::EndDisabled();

                if (action.useCustomTimings) {
                    action.holdDuration = tempHold;
                    action.tapWindow = tempTap;
                }

                ImGuiMCP::Spacing(); ImGuiMCP::Separator(); ImGuiMCP::Spacing();

                if (!activeListeners.empty()) {
                    ImGuiMCP::TextColored({ 0.9f, 0.6f, 1.0f, 1.0f }, "Mods Connected to this Action:");
                    for (const auto& listener : activeListeners) {
                        ImGuiMCP::BulletText("Mod: '%s'  |  Purpose: '%s'", listener.modName.c_str(), listener.purpose.c_str());
                    }
                    ImGuiMCP::Spacing(); ImGuiMCP::Separator(); ImGuiMCP::Spacing();
                }

                ImGuiMCP::Unindent();
                ImGuiMCP::Spacing();
            }
            else {
                ImGuiMCP::PopStyleColor();
            }
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
        ImGuiMCP::Text("Input Manager - Debug");
        ImGuiMCP::Separator(); ImGuiMCP::Spacing();

        if (ImGuiMCP::Checkbox("Show Log in Hud and on log", &showDebugLogs)) {
            SaveSettingsToJson();
        }
        ImGuiMCP::Spacing();
        ImGuiMCP::TextDisabled("If enabled, triggered actions and motions will be displayed on the HUD and printed to the SKSE log.");
    }

    inline void Register() {
        if (!SKSEMenuFramework::IsInstalled()) return;
        LoadCacheFromJson();
        LoadSettingsFromJson();
        LoadActionsFromJson();
        LoadGesturesFromJson();
        LoadMotionsFromJson();
        GenerateDefaultActions();
        SKSEMenuFramework::SetSection("Input Manager");
        SKSEMenuFramework::AddSectionItem("Inputs", RenderMenu);
        SKSEMenuFramework::AddSectionItem("Motion Inputs", RenderMotionMenu);
        SKSEMenuFramework::AddSectionItem("Gestures", RenderGesturesMenu);
        SKSEMenuFramework::AddSectionItem("Inputs in use", RenderDashboardMenu);
        SKSEMenuFramework::AddSectionItem("Debug", RenderDebugMenu);
    }
}