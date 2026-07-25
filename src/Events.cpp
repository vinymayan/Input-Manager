#include "Events.h"
#include "logger.h" 
#include <thread>
#include "Settings.h"

namespace PluginLogic {

    void KeyManager::RegisterAction(const std::string& name, ComboKey combo, std::function<void()> callback, std::function<void()> releaseCallback) {
        //logger::info("[KeyManager] Registrando acao: '{}' | MainKey: {} | ModKey: {}", name, combo.mainKey, combo.modifierKey);
        _bindings.push_back({ name, combo, callback, releaseCallback });
    }

    bool KeyManager::ProcessCoreLogic(RE::InputEvent* a_event) {
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
        if (_testingMotionIndex >= 0) {
            float maxTime = ActionMenuUI::motionList[_testingMotionIndex].timeWindow;
            if (std::chrono::duration<float>(now - _recordingStartTime).count() > maxTime) {
                _testingMotionIndex = -1;
            }
        }

        // Percorre a Linked List de eventos de entrada desse exato frame
        for (auto* e = a_event; e != nullptr; e = e->next) {

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
                    if (_tempMotionSequence.empty() || _tempMotionSequence.back() != eventToLog) {
                        _tempMotionSequence.push_back(eventToLog);
                    }
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

                // -------------------------------------------------------------------
                // AVALIAÇÃO DINÂMICA DE TODAS AS AÇÕES REGISTADAS
                // -------------------------------------------------------------------
                for (auto& binding : _bindings) {

                    if (binding.combo.modifierActionType == ActionState::kGesture) continue;

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

                            bool isMainTap = (binding.combo.mainActionType == ActionState::kTap);
                            bool isModTap = (binding.combo.modifierActionType == ActionState::kTap);

                            if (binding.combo.mainActionType == ActionState::kHold) _keyStates[binding.combo.mainKey].isHeldFired = true;
                            if (binding.combo.modifierActionType == ActionState::kHold) _keyStates[binding.combo.modifierKey].isHeldFired = true;
                            if (binding.combo.mainActionType == ActionState::kPress) _keyStates[binding.combo.mainKey].isPressFired = true;
                            if (binding.combo.modifierActionType == ActionState::kPress) _keyStates[binding.combo.modifierKey].isPressFired = true;

                            _keyStates[binding.combo.mainKey].usedAsModifier = true;
                            if (binding.combo.modifierKey != 0) _keyStates[binding.combo.modifierKey].usedAsModifier = true;

                            consumed = true;

                            if ((isMainTap || isModTap) && binding.combo.needsDelay) {

                                std::string actionName = binding.name;
                                uint32_t tapKey = isMainTap ? binding.combo.mainKey : binding.combo.modifierKey;
                                int requiredTaps = isMainTap ? binding.combo.mainTapCount : binding.combo.modTapCount;
                                float waitTime = binding.combo.tapWindow;

                                std::thread([this, actionName, tapKey, requiredTaps, waitTime]() {
                                    std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(waitTime * 1000)));

                                    auto nowWake = std::chrono::steady_clock::now();
                                    int validTaps = 0;

                                    for (auto it = _keyStates[tapKey].tapHistory.rbegin(); it != _keyStates[tapKey].tapHistory.rend(); ++it) {
                                        if (std::chrono::duration<float>(nowWake - *it).count() <= (waitTime * 1.5f)) {
                                            validTaps++;
                                        }
                                        else {
                                            break;
                                        }
                                    }

                                    if (validTaps == requiredTaps && !_keyStates[tapKey].isDown) {
                                        SKSE::GetTaskInterface()->AddTask([this, actionName, tapKey]() {
                                            ExecuteCallback(actionName);

                                            for (auto& b : _bindings) {
                                                if (b.name == actionName) {
                                                    bool needsRelease = (b.combo.mainActionType == ActionState::kHold || b.combo.modifierActionType == ActionState::kHold ||
                                                        b.combo.mainActionType == ActionState::kPress || b.combo.modifierActionType == ActionState::kPress);
                                                    if (needsRelease) {
                                                        bool anchorIsDown = false;
                                                        if (b.combo.mainActionType == ActionState::kHold || b.combo.mainActionType == ActionState::kPress) {
                                                            anchorIsDown = _keyStates[b.combo.mainKey].isDown;
                                                        }
                                                        else {
                                                            anchorIsDown = _keyStates[b.combo.modifierKey].isDown;
                                                        }

                                                        if (anchorIsDown) {
                                                            b.activeHold = true;
                                                        }
                                                        else {
                                                            ExecuteReleaseCallback(actionName);
                                                        }
                                                    }
                                                    break;
                                                }
                                            }
                                            _keyStates[tapKey].tapHistory.clear();
                                            });
                                    }
                                    }).detach();
                            }
                            else {
                                ExecuteCallback(binding.name);

                                if (binding.combo.mainActionType == ActionState::kHold || binding.combo.modifierActionType == ActionState::kHold ||
                                    binding.combo.mainActionType == ActionState::kPress || binding.combo.modifierActionType == ActionState::kPress) {
                                    binding.activeHold = true;
                                }

                                if (binding.combo.mainActionType == ActionState::kTap) _keyStates[binding.combo.mainKey].tapHistory.clear();
                                if (binding.combo.modifierActionType == ActionState::kTap) _keyStates[binding.combo.modifierKey].tapHistory.clear();
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

        ProcessCoreLogic(a_event);

        if (forceHook) return true;

        return false;
    }


    void KeyManager::ClearStates() {
        //logger::info("[KeyManager] Limpando todos os estados de teclas.");
        _keyStates.clear();
    }

    void KeyManager::ClearBindings() {
        //logger::info("[KeyManager] Limpando todos os bindings e estados registrados.");
        _bindings.clear();
        _keyStates.clear();
    }

    void KeyManager::SortBindings() {
        std::stable_sort(_bindings.begin(), _bindings.end(), [](const KeyBinding& a, const KeyBinding& b) {
            bool aHasMod = (a.combo.modifierKey != 0);
            bool bHasMod = (b.combo.modifierKey != 0);
            return aHasMod > bHasMod; // true (1) vem antes de false (0)
            });
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
                if (_testingMotionIndex >= 0 && ((pass == 1) != _isRecordingGamepad)) {
                    continue;
                }

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

                    if (_testingMotionIndex == static_cast<int>(m)) {
                        // Sucesso no Teste!
                        _motionTestSuccess = true;
                        _testingMotionIndex = -1;
                    }
                    else if (_testingMotionIndex == -1 && !_isRecordingMotion) {
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
