Scriptname SPS_FSMPBridge Hidden

Int Function GetSPSBridgeVersion() Global
    Return 7
EndFunction

Bool Function EnterOperation(String token) Global Native
Bool Function OperationCurrent(String token) Global Native
Bool Function QueueResetBarrier(String token) Global Native
Bool Function ResetBarrierPassed(String token) Global Native

; One lease covers preparation, settling and the final owner. Expired tokens
; cannot start work or be overtaken while their stack is still running.
; preparation: 0 ordinary handoff, 1 full player reset, 2 equipment reconnect.
; Bridge version 4 adds 3: SMP-off only, retaining the V3 call signature.
; Bridge version 5 adds 4: player reset followed by six-bone soft-pose alignment.
; Bridge version 6 also rejects later steps if the player's 3D unloads.
; Bridge version 7 returns live SMP read-back instead of a bare acknowledgement.
;
; Read-back semantics, verified against Faster HDT-SMP src/dhdtPapyrusFunctions.cpp
; (TogglePhysicsImpl): the returned array reports, per requested bone, whether
; that bone WAS DYNAMIC at the moment of the call. The value is recorded before
; any state change, and it is recorded once per bone even if several skinned
; meshes carry the same bone name. FSMP also early-outs when a bone is already
; in the requested state, so a sample never alters a bone that already matches:
;   result[0] = 1 when the operation is still valid and the player 3D is present
;   result[1] = managed bones that were DYNAMIC at sampling time, or -1 if this
;               path never sampled. Above zero during a disable proves SMP still
;               owned bone(s). A false entry means "kinematic OR not found", so
;               it is never treated as confirmation that a bone stopped.
;   result[2] = bone array length (a short array means an incomplete skeleton)
;   result[3] = reserved, always 0
; FSMP documents the value as the PREVIOUS state, so the sample is taken before
; the corresponding call. On a CBPC->SMP transition the enabling call is not
; used for evidence: it reports the prior (kinematic) state, which says nothing
; about whether SMP subsequently took ownership.
Int[] Function SetPlayerOwnerV3(String token, Bool useCBPC, Int preparation, Int softBend) Global
    Int[] result = new Int[4]
    result[0] = 0
    result[1] = -1
    result[2] = 0
    result[3] = 0
    If !EnterOperation(token)
        Return result
    EndIf
    Actor targetActor = Game.GetPlayer()
    If targetActor == None || !targetActor.Is3DLoaded() || !OperationCurrent(token)
        Return result
    EndIf
    String[] bones = GetPhysicsBones()
    result[2] = bones.Length
    If preparation == 3
        If !useCBPC || !OperationCurrent(token) || !targetActor.Is3DLoaded()
            Return result
        EndIf
        ; Maintain the existing CBPC owner without restarting it or its pose.
        ; This is the only passive drift probe in the whole plugin: the sample is
        ; FSMP's own previous-state read-back for bones SPS already believes are
        ; kinematic. Any dynamic bone here means SMP resumed ownership unseen.
        result[1] = CountDynamicBones(DynamicHDT.TogglePhysics(targetActor, bones, false))
        If OperationCurrent(token) && targetActor.Is3DLoaded()
            result[0] = 1
        EndIf
        Return result
    EndIf
    Int index = 0
    If preparation == 1 || preparation == 4
        If softBend >= 0
            Debug.SendAnimationEvent(targetActor, "SOSFlaccid")
            If !OperationCurrent(token) || !targetActor.Is3DLoaded()
                Return result
            EndIf
            SOSAE_SKSE.SetSchlongBend(targetActor, softBend)
            If !OperationCurrent(token) || !targetActor.Is3DLoaded()
                Return result
            EndIf
        EndIf
        DynamicHDT.ResetPhysics(targetActor, true)
        If !QueueResetBarrier(token)
            Return result
        EndIf
        While !ResetBarrierPassed(token)
            If !OperationCurrent(token) || !targetActor.Is3DLoaded()
                Return result
            EndIf
            Utility.Wait(0.1)
        EndWhile
        Utility.Wait(0.75)
    ElseIf preparation == 2
        While index < bones.Length
            If !OperationCurrent(token) || !targetActor.Is3DLoaded()
                Return result
            EndIf
            CBPCPluginScript.StopPhysics(targetActor, bones[index])
            index += 1
        EndWhile
        If !OperationCurrent(token) || !targetActor.Is3DLoaded()
            Return result
        EndIf
        result[1] = CountDynamicBones(DynamicHDT.TogglePhysics(targetActor, bones, false))
        Utility.Wait(0.35)
    EndIf
    If !OperationCurrent(token) || !targetActor.Is3DLoaded()
        Return result
    EndIf
    index = 0
    If useCBPC
        ; Sampled before the disable so a still-dynamic bone is visible.
        result[1] = CountDynamicBones(DynamicHDT.TogglePhysics(targetActor, bones, false))
        Utility.Wait(0.25)
        While index < bones.Length
            If !OperationCurrent(token) || !targetActor.Is3DLoaded()
                Return result
            EndIf
            CBPCPluginScript.StartPhysics(targetActor, bones[index])
            index += 1
        EndWhile
    Else
        While index < bones.Length
            If !OperationCurrent(token) || !targetActor.Is3DLoaded()
                Return result
            EndIf
            CBPCPluginScript.StopPhysics(targetActor, bones[index])
            index += 1
        EndWhile
        If preparation == 4
            If !OperationCurrent(token) || !targetActor.Is3DLoaded()
                Return result
            EndIf
            ; Reloaded SMP bodies are already dynamic. Briefly follow the
            ; skeleton after the reload so their simulated transforms catch up
            ; with the soft pose before dynamics resume. Do not freeze before
            ; rebuilding, and do not reset unrelated physics systems.
            DynamicHDT.TogglePhysics(targetActor, bones, false)
        EndIf
        Utility.Wait(0.25)
        If !OperationCurrent(token) || !targetActor.Is3DLoaded()
            Return result
        EndIf
        DynamicHDT.TogglePhysics(targetActor, bones, true)
    EndIf
    ; Execution acknowledgment is not read-back of live engine ownership. Only
    ; result[1] carries evidence, and only for the disable direction.
    If OperationCurrent(token) && targetActor.Is3DLoaded()
        result[0] = 1
    EndIf
    Return result
EndFunction

Int Function CountDynamicBones(Bool[] previousStates) Global
    If previousStates == None
        Return -1
    EndIf
    Int count = 0
    Int index = 0
    While index < previousStates.Length
        If previousStates[index]
            count += 1
        EndIf
        index += 1
    EndWhile
    Return count
EndFunction

String[] Function GetPhysicsBones() Global
    String[] bones = new String[6]
    bones[0] = "NPC Genitals01 [Gen01]"
    bones[1] = "NPC Genitals02 [Gen02]"
    bones[2] = "NPC Genitals03 [Gen03]"
    bones[3] = "NPC Genitals04 [Gen04]"
    bones[4] = "NPC Genitals05 [Gen05]"
    bones[5] = "NPC Genitals06 [Gen06]"
    Return bones
EndFunction

; Resolve the player and build the string array inside Papyrus instead of
; asking the native SPS DLL to pack Actor or String[] values while the VM is
; still settling after a load transition.
Function TogglePlayerPhysics(Bool enabled) Global
    Actor targetActor = Game.GetPlayer()
    If targetActor == None
        Return
    EndIf

    String[] bones = GetPhysicsBones()
    DynamicHDT.TogglePhysics(targetActor, bones, enabled)
EndFunction

; Perform both halves of a physics handoff in one Papyrus stack. Separate
; native dispatches can execute late or out of order on busy games, leaving
; FSMP active after SPS has already reported CBPC as the selected owner.
Function SetPlayerOwner(Bool useCBPC) Global
    Bool ignored = SetPlayerOwnerV2(useCBPC)
EndFunction

; Version 2 returns only after the ordered transaction has run. The native
; plugin uses this result to distinguish a queued VM call from a completed
; handoff while preserving SetPlayerOwner for older external callers.
Bool Function SetPlayerOwnerV2(Bool useCBPC) Global
    Actor targetActor = Game.GetPlayer()
    If targetActor == None
        Return false
    EndIf

    String[] bones = GetPhysicsBones()
    Int index = 0
    If useCBPC
        DynamicHDT.TogglePhysics(targetActor, bones, false)
        ; FSMP changes ownership on its own native update. Yield briefly so it
        ; has actually detached before CBPC is asked to claim the same bones.
        Utility.Wait(0.25)
        While index < bones.Length
            CBPCPluginScript.StartPhysics(targetActor, bones[index])
            index += 1
        EndWhile
    Else
        While index < bones.Length
            CBPCPluginScript.StopPhysics(targetActor, bones[index])
            index += 1
        EndWhile
        ; CBPC also applies node changes asynchronously. Starting FSMP in the
        ; same frame can leave both engines believing that they own the bones.
        Utility.Wait(0.25)
        DynamicHDT.TogglePhysics(targetActor, bones, true)
    EndIf
    Return true
EndFunction

; Equipment recovery briefly releases both engines from the old mesh before
; SetPlayerOwner(true) attaches CBPC to the replacement bones.
Function ReleasePlayerPhysics() Global
    Actor targetActor = Game.GetPlayer()
    If targetActor == None
        Return
    EndIf

    String[] bones = GetPhysicsBones()
    Int index = 0
    While index < bones.Length
        CBPCPluginScript.StopPhysics(targetActor, bones[index])
        index += 1
    EndWhile
    DynamicHDT.TogglePhysics(targetActor, bones, false)
EndFunction

; ResetPhysics only needs a Bool from the native DLL. Resolving the player here
; avoids the same unsafe native Actor packing path used by delayed SMP resets.
Function ResetPlayerPhysics(Bool full) Global
    Actor targetActor = Game.GetPlayer()
    If targetActor == None
        Return
    EndIf

    DynamicHDT.ResetPhysics(targetActor, full)
EndFunction
