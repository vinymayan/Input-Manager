#include <Windows.h>
#include <CommCtrl.h> 
#pragma comment(lib, "Comctl32.lib")
#include "Events.h"

#define FOCUS_SUBCLASS_ID 1337 

// O novo formato da funcao usando Subclass
LRESULT CALLBACK FocusSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {

    // 1. Alt+Tab ou minimizou
    if (uMsg == WM_ACTIVATEAPP && wParam == FALSE) {
        logger::info("[Input Manager] WM_ACTIVATEAPP: Jogo minimizado/perdeu prioridade (Alt+Tab). Limpando inputs...");
        PluginLogic::KeyManager::GetSingleton()->ResetAllInputs();
    }
    // 2. Clicou fora da janela (em outro monitor, por exemplo)
    /*else if (uMsg == WM_ACTIVATE && LOWORD(wParam) == WA_INACTIVE) {
        logger::info("[Input Manager] WM_ACTIVATE: Janela inativa. Limpando inputs...");
        PluginLogic::KeyManager::GetSingleton()->ResetAllInputs();
    }*/
    // 3. Foco perdido genérico
    /*else if (uMsg == WM_KILLFOCUS) {
        logger::info("[Input Manager] WM_KILLFOCUS: Foco perdido. Limpando inputs...");
        PluginLogic::KeyManager::GetSingleton()->ResetAllInputs();
    }*/

    // Passa a mensagem adiante com segurança (substitui o antigo CallWindowProc)
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

void InstallWindowFocusHook() {
    auto taskInterface = SKSE::GetTaskInterface();
    if (taskInterface) {
        taskInterface->AddTask([]() {
            HWND g_hWindow = nullptr;
            auto mainSingleton = RE::Main::GetSingleton();

            if (mainSingleton) {
                g_hWindow = reinterpret_cast<HWND>(mainSingleton->wnd);
                if (!IsWindow(g_hWindow)) g_hWindow = nullptr; 
            }

            if (!g_hWindow) {
                g_hWindow = FindWindowA(nullptr, "Skyrim Special Edition");
            }

            if (g_hWindow && IsWindow(g_hWindow)) {
                DWORD windowPid = 0;
                GetWindowThreadProcessId(g_hWindow, &windowPid);
                
                if (windowPid == GetCurrentProcessId()) {
                    if (SetWindowSubclass(g_hWindow, FocusSubclassProc, FOCUS_SUBCLASS_ID, 0)) {
                        logger::info("[Input Manager] Hook de foco de janela (Subclass) instalado com sucesso!");
                    }
                }
            } else {
                logger::error("[Input Manager] Falha critica: Nao foi possivel encontrar a janela do jogo.");
            }
        });
    }
}