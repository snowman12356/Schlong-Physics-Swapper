Scriptname SPS_PositionBridge Hidden

Int Function GetSPSBridgeVersion() Global
    Return 6
EndFunction

; Version 6: no variable, parameter or cast in this script may use a SexLab (or
; other optional mod) script type. The player's angle event is sent through
; this script for every user, and 2.3.0 to 2.3.0.2 crashed on the first call
; into it when SexLab was not installed and SexLabFramework could not be
; linked. SexLab reads go through SPS_SexLabBridge, which is only called when
; SexLab is present; calls to other scripts' global functions are fine.

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
        ; Via SPS_SexLabBridge: this script must not declare SexLab types (v6).
        ; A missing or older SexLab bridge returns 0 here: unavailable.
        If SPS_SexLabBridge.GetSPSBridgeVersion() < 6
            Return 0
        EndIf
        Int sexLabState = SPS_SexLabBridge.ReadActorSceneState(targetActor)
        If sexLabState != 1
            Return sexLabState
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

; Version 5: the role of an actor in a scene. 0 unknown, 1 receiving, 2
; penetrating, -1 SexLab requested but unresolvable. OStim first, then SexLab
; (SPS_SexLabBridge.ReadActorRole). Only the requested providers are touched.
Int Function ReadActorSceneRole(Actor targetActor, Bool checkSexLab, Bool checkOStim) Global
    If targetActor == None
        Return 0
    EndIf
    If checkOStim
        Int ostimRole = SPS_OStimBridge.GetActorRole(targetActor)
        If ostimRole > 0
            Return ostimRole
        EndIf
    EndIf
    If checkSexLab
        ; A missing or older SexLab bridge would answer 0 ("no role"); report
        ; it as failed instead.
        If SPS_SexLabBridge.GetSPSBridgeVersion() < 6
            Return -1
        EndIf
        Return SPS_SexLabBridge.ReadActorRole(targetActor)
    EndIf
    Return 0
EndFunction

; -1 failed/stale, otherwise the ReadActorSceneRole value.
Int Function GetActorSceneRole(String token, Bool checkSexLab, Bool checkOStim) Global
    Actor targetActor = ResolveSceneRequest(token)
    If targetActor == None || !SceneRequestCurrent(token, targetActor)
        Return -1
    EndIf
    Int role = ReadActorSceneRole(targetActor, checkSexLab, checkOStim)
    If !SceneRequestCurrent(token, targetActor)
        Return -1
    EndIf
    Return role
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

; Sends only the SOS/TNG stage event, which sets the angle. No Schlongs of
; Skyrim native is called: SOS AE 4.0.0 removed SOSAE_SKSE.SetSchlongBend.
; unusedNative is kept for the bridge contract.
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
