Scriptname SPS_PositionBridge Hidden

Int Function GetSPSBridgeVersion() Global
    Return 4
EndFunction

Bool Function ActorRequestCurrent(String token, Actor targetActor) Global Native
Actor Function ResolveActorRequest(String token) Global Native
Bool Function SceneRequestCurrent(String token, Actor targetActor) Global Native
Actor Function ResolveSceneRequest(String token) Global Native

; 0 unavailable, 1 clear, 2 active. Only installed/available providers are
; requested by the native adapter. A failed query must not grant permission.
Int Function ReadActorSceneState(Actor targetActor, Bool checkOSL, Bool checkSexLab, Bool checkOStim) Global
    If targetActor == None
        Return 0
    EndIf
    If checkOSL
        If OSLArousedNative.IsInScene(targetActor)
            Return 2
        EndIf
    EndIf
    If checkSexLab
        SexLabFramework sexLab = Game.GetFormFromFile(0xD62, "SexLab.esm") as SexLabFramework
        If sexLab == None
            Return 0
        EndIf
        If sexLab.IsActorActive(targetActor)
            Return 2
        EndIf
    EndIf
    If checkOStim
        Int ostimState = SPS_OStimBridge.GetActorSceneState(targetActor)
        If ostimState != 1
            Return ostimState
        EndIf
    EndIf
    Return 1
EndFunction

Int Function GetActorSceneState(String token, Bool checkOSL, Bool checkSexLab, Bool checkOStim) Global
    Actor targetActor = ResolveSceneRequest(token)
    If targetActor == None || !SceneRequestCurrent(token, targetActor)
        Return 0
    EndIf
    Int sceneState = ReadActorSceneState(targetActor, checkOSL, checkSexLab, checkOStim)
    If !SceneRequestCurrent(token, targetActor)
        Return 0
    EndIf
    Return sceneState
EndFunction

; -1 failed/stale, 0 deferred to scene ownership, 1 command completed.
; Basic SOS behavior sends only the legacy event; it does not change engines.
Int Function ApplyBasicActorPosition(String token, String eventName, Bool checkOSL, Bool checkSexLab, Bool checkOStim) Global
    If !SPS_FSMPBridge.EnterOperation(token)
        Return -1
    EndIf
    Actor targetActor = ResolveActorRequest(token)
    If targetActor == None || !ActorRequestCurrent(token, targetActor) || !SPS_FSMPBridge.OperationCurrent(token)
        Return -1
    EndIf
    Int sceneState = ReadActorSceneState(targetActor, checkOSL, checkSexLab, checkOStim)
    If sceneState != 1
        If sceneState == 2
            Return 0
        EndIf
        Return -1
    EndIf
    If !ActorRequestCurrent(token, targetActor) || !SPS_FSMPBridge.OperationCurrent(token)
        Return -1
    EndIf
    Debug.SendAnimationEvent(targetActor, eventName)
    If !ActorRequestCurrent(token, targetActor) || !SPS_FSMPBridge.OperationCurrent(token)
        Return -1
    EndIf
    Return 1
EndFunction

; Sends only the stage event. The angle itself is applied through
; SPS_PoseAngle, which does not depend on any Schlongs of Skyrim native: SOS AE
; 4.0.0 removed SOSAE_SKSE.SetSchlongBend.
Bool Function ApplyActorPosition(String token, String eventName, Bool unusedNative) Global
    If !SPS_FSMPBridge.EnterOperation(token)
        Return False
    EndIf
    Actor targetActor = ResolveActorRequest(token)
    If targetActor == None || !ActorRequestCurrent(token, targetActor) || !SPS_FSMPBridge.OperationCurrent(token)
        Return False
    EndIf
    If eventName != ""
        Debug.SendAnimationEvent(targetActor, eventName)
    EndIf
    Return ActorRequestCurrent(token, targetActor) && SPS_FSMPBridge.OperationCurrent(token)
EndFunction

Bool Function SendPlayerAnimationEvent(String eventName) Global
    Actor player = Game.GetPlayer()
    If player == None || eventName == ""
        Return False
    EndIf

    Debug.SendAnimationEvent(player, eventName)
    Return True
EndFunction

; -1 keeps the angle configured in the INI (Position/PoseAngleDegrees).
Bool Function SetPlayerPoseAngle(Int bend, Float degreesAtMaxBend) Global
    Actor player = Game.GetPlayer()
    If player == None
        Return False
    EndIf

    Return SPS_PoseAngle.SetActorPoseAngle(player, bend, degreesAtMaxBend)
EndFunction
