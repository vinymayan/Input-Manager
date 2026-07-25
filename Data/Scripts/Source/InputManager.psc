Scriptname InputManager

; ===============================================================================================
; FUNÇÕES NATIVAS UNIFICADAS (inputType: 0 = Action, 1 = Motion Input, 2 = Gestures/Movements)
; ===============================================================================================

; Retorna a quantidade total de inputs registrados no tipo especificado.
Int Function GetInputCount(Int inputType) Global Native

; Retorna o Nome customizado do input pelo jogador.
String Function GetInputName(Int inputType, Int inputID) Global Native

; Cria um novo Input (Action, Motion ou Gesto) com o nome especificado. Retorna o ID (Index) ou -1 se houver conflito.
Int Function CreateInput(Int inputType, String inputName) Global Native

; Deleta um Input existente. Retorna True se sucesso (impede a deleção se mais de 1 mod depender dele).
Bool Function DeleteInput(Int inputType, Int inputID) Global Native

; Registra ou remove o seu mod da lista de ouvintes de um Action ou Motion.
; validMainActions e validModActions servem para restringir quais Estados (0=Ignore, 1=Tap, 2=Hold, 3=Gesture, 4=Press) são suportados pelo seu mod.
; Para não aplicar restrições (permitir qualquer estado), passe um array vazio: new Int[0]
Function UpdateListener(Int inputType, Int inputID, String modName, String purpose, Bool isRegistering, Int[] validMainActions, Int[] validModActions) Global Native

; Retorna uma lista de strings com os nomes de todos os mods conectados a um input.
String[] Function GetListeners(Int inputType, Int inputID) Global Native


; ===============================================================================================
; FUNÇÕES DE MAPEAMENTO ESPECÍFICAS - ACTION
; ===============================================================================================
; Retorna as configurações de botões de uma Action em formato de Array Int[]
Int[] Function GetActionKeyData(Int actionID) Global Native

; Retorna os tempos customizados de uma Action em formato de Array Float[]
Float[] Function GetActionTimings(Int actionID) Global Native

; Atualiza os tempos customizados de uma Action
Bool Function UpdateActionTimings(Int actionID, Bool useCustom, Float tapWindow, Float holdDuration) Global Native

; Atualiza o mapeamento de botões, tipos e contagem de taps de uma Action.
Bool Function UpdateActionMapping(Int actionID, Int pcMainKey, Int pcMainAction, Int pcMainTap, Int pcModifierKey, Int pcModAction, Int pcModTap, Int padMainKey, Int padMainAction, Int padMainTap, Int padModifierKey, Int padModAction, Int padModTap, Int padGestureStick, String newName = "") Global Native


; ===============================================================================================
; FUNÇÕES DE MAPEAMENTO ESPECÍFICAS - MOTION
; ===============================================================================================
; Retorna a sequência gravada (IDs inteiros) de um Motion Input.
; Use isGamepad = True para controle ou False para Teclado/Rato.
; Cada sequência suporta no máximo 20 inputs.
Int[] Function GetMotionSequence(Int motionID, Bool isGamepad) Global Native

; Retorna a janela de tempo máxima definida (Time Window) para executar a sequência de forma válida.
; Os inputs devem ser consecutivos e seguir exatamente a ordem configurada dentro dessa janela.
Float Function GetMotionTimeWindow(Int motionID) Global Native

; Atualiza ou sobrepõe os dados de um Motion Input. Retorna False se a sequência ou nome colidirem com um Motion Input já existente.
Bool Function UpdateMotionMapping(Int motionID, Int[] pcSequence, Int[] padSequence, Float timeWindow, String newName = "") Global Native


; ===============================================================================================
; EVENTOS GLOBAIS (Mod Events)
; ===============================================================================================

; O Input Manager envia os seguintes eventos via SendModEvent:
;
; "InputManager_ActionTriggered"
; - Chamado quando a combinação de teclas é validada (Tap, Tap Multiplo, Gesto, Hold Começo).
;  - Param 1: string actionName (Nome da ação)
;  - Param 2: int actionID (ID da ação)
;
; "InputManager_ActionReleased"
; - Chamado quando a tecla é solta (útil para ações de segurar).
;  - Param 1: string actionName
;  - Param 2: int actionID
;
; "InputManager_MotionTriggered"
; - Chamado quando uma sequência de botões é executada corretamente no tempo estipulado.
;  - Param 1: string motionName
;  - Param 2: int motionID
;
; "InputManager_MotionInputUpdated"
; - Chamado em tempo real quando o Input Manager registra um input usado no reconhecimento de Motions.
;  - Param 1: string payload no formato "device|input1,input2,input3"
;    device é "pc" ou "pad"; o histórico ordenado contém até os 12 inputs mais recentes
;    que ainda formam um prefixo válido. O histórico pode estar vazio, por exemplo: "pc|".
;  - Param 2: int lastInput (código numérico do input mais recente)
;
; IDs virtuais de direção usados nas sequências e no payload:
; 5000=Up, 5001=Down, 5002=Left, 5003=Right
; 5004=Up-Right, 5005=Up-Left, 5006=Down-Right, 5007=Down-Left
