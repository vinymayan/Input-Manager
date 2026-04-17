#include "Events.h"
#include "logger.h" 
#include <thread>
#include "Settings.h"

namespace PluginLogic {

    void KeyManager::RegisterAction(const std::string& name, ComboKey combo, std::function<void()> callback, std::function<void()> releaseCallback) {
        //logger::info("[KeyManager] Registrando acao: '{}' | MainKey: {} | ModKey: {}", name, combo.mainKey, combo.modifierKey);
        _bindings.push_back({ name, combo, callback, releaseCallback });
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

    void KeyManager::UpdateModListener(int actionID, const std::string& modName, const std::string& purpose, bool isRegistering) {
        if (isRegistering) {
            // LÓGICA DE REGISTRO / ATUALIZAÇÃO
            for (auto& listener : _listeners) {
                if (listener.modName == modName && listener.purpose == purpose) {
                    logger::info("[Input Manager] Atualizado! Mod: '{}' | Proposito: '{}' mudou para Acao ID: {}", modName, purpose, actionID);
                    listener.actionID = actionID;
                    return;
                }
            }
            logger::info("[Input Manager] Novo Mod Registrado! ID da Acao: {} | Mod: '{}' | Utilizado para: '{}'", actionID, modName, purpose);
            _listeners.push_back({ actionID, modName, purpose });
        }
        else {
            // LÓGICA DE DESREGISTRO (Remoção)
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



    void KeyManager::CheckMotionMatches(std::chrono::steady_clock::time_point now) {
        if (_inputHistory.empty()) return;

        for (size_t m = 0; m < ActionMenuUI::motionList.size(); ++m) {
            const auto& motionEntry = ActionMenuUI::motionList[m];

            for (int pass = 0; pass < 2; ++pass) {
                const auto& requiredSeq = (pass == 0) ? motionEntry.pcSequence : motionEntry.padSequence;

                if (requiredSeq.empty()) continue;
                if (_inputHistory.back().keyID != requiredSeq.back()) continue;

                int reqIdx = static_cast<int>(requiredSeq.size()) - 1;
                int firstMatchHistoryIdx = -1;

                // Busca a sequência de trás pra frente
                for (int i = static_cast<int>(_inputHistory.size()) - 1; i >= 0 && reqIdx >= 0; --i) {
                    if (_inputHistory[i].keyID == requiredSeq[reqIdx]) {
                        if (reqIdx == 0) firstMatchHistoryIdx = i; // Encontrou o primeiro input da sequência!
                        reqIdx--;
                    }
                }

                if (reqIdx < 0 && firstMatchHistoryIdx != -1) {
                    // Calcula o tempo levado entre o PRIMEIRO e o ÚLTIMO botão da sequência
                    float timeTaken = std::chrono::duration<float>(_inputHistory.back().timestamp - _inputHistory[firstMatchHistoryIdx].timestamp).count();

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
                            auto player = RE::PlayerCharacter::GetSingleton();
                            if (player) {
                                player->SetGraphVariableInt("MotionInputCMF", static_cast<int>(m));
                                player->SetGraphVariableBool("BFCO_Is0GravityAttck", true);
                                if (auto charController = player->GetCharController()) {
                                    // Zera a gravidade do jogador
                                    charController->gravity = 0.0f;

                                    // Zera o tempo de queda para evitar dano de queda acumulado quando a gravidade voltar
                                    charController->fallTime = 0.0f;

                                    charController->outVelocity.quad.m128_f32[2] = 0.0f;
                                    charController->initialVelocity.quad.m128_f32[2] = 0.0f;
                                    charController->velocityMod.quad.m128_f32[2] = 0.0f;
                                    charController->direction.quad.m128_f32[2] = 0.0f;

                                    // Recupera a velocidade linear atual do corpo rígido (RigidBody) na engine e anula o eixo Z
                                    RE::hkVector4 linearVel;
                                    charController->GetLinearVelocityImpl(linearVel);
                                    linearVel.quad.m128_f32[2] = 0.0f;
                                    charController->SetLinearVelocityImpl(linearVel);
                                }
                                player->NotifyAnimationGraph("BFCO_0GravityStart");
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
    }


    // ADICIONE ESTE NOVO MÉTODO
    void KeyManager::StartMotionTesting(int motionIndex) {
        _testingMotionIndex = motionIndex;
        _motionTestSuccess = false;
        _inputHistory.clear();
        _recordingStartTime = std::chrono::steady_clock::now();
    }


    bool KeyManager::ProcessInput(RE::InputEvent* a_event) {
        if (!a_event) return false;
        bool consumed = false;
        auto now = std::chrono::steady_clock::now();

        // --- ADIÇÃO: LIMPEZA DO BUFFER (Manter no máximo 4.0s globalmente) ---
        while (!_inputHistory.empty() && std::chrono::duration<float>(now - _inputHistory.front().timestamp).count() > 4.0f) {
            _inputHistory.pop_front();
        }

        // --- ADIÇÃO: TIMER DE GRAVAÇÃO (Para após 2s) ---
        if (_isRecordingMotion && _recordingMotionIndex >= 0) {
            float maxTime = ActionMenuUI::motionList[_recordingMotionIndex].timeWindow;
            if (std::chrono::duration<float>(now - _recordingStartTime).count() > maxTime) {
                _isRecordingMotion = false; // Acabou o tempo de gravação
            }
        }
        if (_testingMotionIndex >= 0) {
            float maxTime = ActionMenuUI::motionList[_testingMotionIndex].timeWindow;
            if (std::chrono::duration<float>(now - _recordingStartTime).count() > maxTime) {
                _testingMotionIndex = -1; // Acabou o tempo de teste (Falhou)
            }
        }

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
                isKeyDown = btn->IsDown();

                // D-PAD Gamepad
                if (rawKeyID == RE::BSWin32GamepadDevice::Keys::kUp + 266) newUp = btn->IsDown() || btn->IsHeld();
                else if (rawKeyID == RE::BSWin32GamepadDevice::Keys::kDown + 266) newDown = btn->IsDown() || btn->IsHeld();
                else if (rawKeyID == RE::BSWin32GamepadDevice::Keys::kLeft + 266) newLeft = btn->IsDown() || btn->IsHeld();
                else if (rawKeyID == RE::BSWin32GamepadDevice::Keys::kRight + 266) newRight = btn->IsDown() || btn->IsHeld();
                // WASD PC
                else if (rawKeyID == DefaultkeyUp) newUp = btn->IsDown() || btn->IsHeld();
                else if (rawKeyID == DefaultkeyDown) newDown = btn->IsDown() || btn->IsHeld();
                else if (rawKeyID == DefaultkeyLeft) newLeft = btn->IsDown() || btn->IsHeld();
                else if (rawKeyID == DefaultkeyRight) newRight = btn->IsDown() || btn->IsHeld();
            }

            // Verifica se a direção alterou
            if (newUp != _dirUp || newDown != _dirDown || newLeft != _dirLeft || newRight != _dirRight) {
                _dirUp = newUp; _dirDown = newDown; _dirLeft = newLeft; _dirRight = newRight;
                dirChanged = true;
            }

            // --- ADIÇÃO: REGISTRO NO BUFFER ---
            uint32_t eventToLog = 0;

            if (dirChanged) {
                eventToLog = GetDirectionVKey(_dirUp, _dirDown, _dirLeft, _dirRight);
            }
            else if (isKeyDown && rawKeyID != 0) {
                // Não registra de novo as teclas direcionais brutas no buffer de motion, elas já viraram VKEY
                if (rawKeyID != DefaultkeyUp && rawKeyID != DefaultkeyLeft &&
                    rawKeyID != DefaultkeyDown && rawKeyID != DefaultkeyRight &&
                    rawKeyID != (RE::BSWin32GamepadDevice::Keys::kUp + 266) && rawKeyID != (RE::BSWin32GamepadDevice::Keys::kDown + 266) &&
                    rawKeyID != (RE::BSWin32GamepadDevice::Keys::kLeft + 266) && rawKeyID != (RE::BSWin32GamepadDevice::Keys::kRight + 266)) {

                    eventToLog = rawKeyID;
                }
            }

            if (eventToLog != 0) {
                _inputHistory.push_back({ eventToLog, now });

                if (_isRecordingMotion && (isGamepadEvent == _isRecordingGamepad)) {
                    // Evita Spam da mesma tecla repetida consecutivamente
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

                std::lock_guard<std::mutex> lock(_gestureMutex); // Protege a gravação dos pontos
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

                        std::lock_guard<std::mutex> lock(_gestureMutex); // Protege a gravação dos pontos
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
                                // Verifica se a tecla solta era a tecla principal do Hold ou a modificadora do Hold
                                bool mainReleased = (binding.combo.mainKey == id && (binding.combo.mainActionType == ActionState::kHold || binding.combo.mainActionType == ActionState::kPress));
                                bool modReleased = (binding.combo.modifierKey == id && (binding.combo.modifierActionType == ActionState::kHold || binding.combo.modifierActionType == ActionState::kPress));
                                
                                if (mainReleased || modReleased) {
                                    ExecuteReleaseCallback(binding.name);
                                    binding.activeHold = false; // Desliga a flag, o hold acabou
                                }
                            }
                        }
                        state.isDown = false;
                        state.lastUpTime = now;
                        state.isHeldFired = false;
                        state.usedAsModifier = false;
                        state.tapHistory.push_back(now);

                        // Limpa lixo do histórico (Remove taps mais velhos que 2 segundos para não dar leak de memoria)
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

                        // Define quem atua como Gatilho (Trava ativada) e quem atua como Âncora (Trava ignorada)
                        bool mainIsAnchor = false;
                        bool modIsAnchor = false;

                        if (binding.combo.modifierKey != 0) {
                            if (binding.combo.modifierActionType == ActionState::kTap) {
                                // Se o Modificador é Tap, ele é o Gatilho. A Tecla Principal é a Âncora.
                                mainIsAnchor = true;
                                modIsAnchor = false;
                            }
                            else if (binding.combo.mainActionType == ActionState::kTap) {
                                // Se a Tecla Principal é Tap, ela é o Gatilho. O Modificador é a Âncora.
                                mainIsAnchor = false;
                                modIsAnchor = true;
                            }
                            else {
                                // Se nenhum for Tap (ex: Hold+Press), o Modificador atua como âncora tradicional.
                                mainIsAnchor = false;
                                modIsAnchor = true;
                            }
                        }

                        // Verifica as condições bases de Tap, Hold, etc, respeitando as âncoras reais
                        if (IsConditionMet(binding.combo.mainKey, binding.combo.mainActionType, binding.combo.mainTapCount, now, binding.combo.tapWindow, binding.combo.holdDuration, mainIsAnchor) &&
                            IsConditionMet(binding.combo.modifierKey, binding.combo.modifierActionType, binding.combo.modTapCount, now, binding.combo.tapWindow, binding.combo.holdDuration, modIsAnchor)) {
                            // LÓGICA DE DELAY PARA TAP CONFLITANTE
                            bool isMainTap = (binding.combo.mainActionType == ActionState::kTap);
                            bool isModTap = (binding.combo.modifierActionType == ActionState::kTap);

                            // 1. Trava os disparos repetidos instantaneamente para o frame atual não reavaliar
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

                                    // Dispara APENAS se os Taps exatos baterem. 
                                    if (validTaps == requiredTaps && !_keyStates[tapKey].isDown) {
                                        SKSE::GetTaskInterface()->AddTask([this, actionName, tapKey]() {
                                            ExecuteCallback(actionName);

                                            //Liga o ActionReleased SOMENTE se a ação realmente disparou no delay
                                            for (auto& b : _bindings) {
                                                if (b.name == actionName) {
                                                    bool needsRelease = (b.combo.mainActionType == ActionState::kHold || b.combo.modifierActionType == ActionState::kHold ||
                                                        b.combo.mainActionType == ActionState::kPress || b.combo.modifierActionType == ActionState::kPress);
                                                    if (needsRelease) {
                                                        // Verifica se a tecla âncora (Hold/Press) ainda está sendo segurada
                                                        bool anchorIsDown = false;
                                                        if (b.combo.mainActionType == ActionState::kHold || b.combo.mainActionType == ActionState::kPress) {
                                                            anchorIsDown = _keyStates[b.combo.mainKey].isDown;
                                                        }
                                                        else {
                                                            anchorIsDown = _keyStates[b.combo.modifierKey].isDown;
                                                        }

                                                        if (anchorIsDown) {
                                                            b.activeHold = true; // Ainda segura, aguarda o botão subir
                                                        }
                                                        else {
                                                            ExecuteReleaseCallback(actionName); // Já soltou durante a espera do delay, libera agora
                                                        }
                                                    }
                                                    break;
                                                }
                                            }
                                            // Limpa para não disparar múltiplas vezes em sequências rápidas
                                            _keyStates[tapKey].tapHistory.clear();
                                            });
                                    }
                                    }).detach();
                            }
                            else {
                                // Se for Tap sem conflitos, Hold, ou Press, DISPARA INSTANTANEAMENTE
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
        return consumed;
    }
    void GetMovementKeys()
    {
        auto controlMap = RE::ControlMap::GetSingleton();
        auto userEvents = RE::UserEvents::GetSingleton();
        if (controlMap && userEvents) {
            DefaultkeyUp = controlMap->GetMappedKey(userEvents->forward, RE::INPUT_DEVICE::kKeyboard);
            DefaultkeyDown = controlMap->GetMappedKey(userEvents->back, RE::INPUT_DEVICE::kKeyboard);
            DefaultkeyLeft = controlMap->GetMappedKey(userEvents->strafeLeft, RE::INPUT_DEVICE::kKeyboard);
            DefaultkeyRight = controlMap->GetMappedKey(userEvents->strafeRight, RE::INPUT_DEVICE::kKeyboard);
        }
    }
}
