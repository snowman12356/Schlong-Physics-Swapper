Scriptname SPS_FSMPBridge Hidden

Int Function GetSPSBridgeVersion() Global
    Return 6
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
Bool Function SetPlayerOwnerV3(String token, Bool useCBPC, Int preparation, Int softBend) Global
    If !EnterOperation(token)
        Return false
    EndIf
    Actor targetActor = Game.GetPlayer()
    If targetActor == None || !targetActor.Is3DLoaded() || !OperationCurrent(token)
        Return false
    EndIf
    String[] bones = GetPhysicsBones()
    If preparation == 3
        If !useCBPC || !OperationCurrent(token) || !targetActor.Is3DLoaded()
            Return false
        EndIf
        ; Maintain the existing CBPC owner without restarting it or its pose.
        DynamicHDT.TogglePhysics(targetActor, bones, false)
        Return OperationCurrent(token) && targetActor.Is3DLoaded()
    EndIf
    Int index = 0
    If preparation == 1 || preparation == 4
        If softBend >= 0
            Debug.SendAnimationEvent(targetActor, "SOSFlaccid")
            If !OperationCurrent(token) || !targetActor.Is3DLoaded()
                Return false
            EndIf
            SOSAE_SKSE.SetSchlongBend(targetActor, softBend)
            If !OperationCurrent(token) || !targetActor.Is3DLoaded()
                Return false
            EndIf
        EndIf
        DynamicHDT.ResetPhysics(targetActor, true)
        If !QueueResetBarrier(token)
            Return false
        EndIf
        While !ResetBarrierPassed(token)
            If !OperationCurrent(token) || !targetActor.Is3DLoaded()
                Return false
            EndIf
            Utility.Wait(0.1)
        EndWhile
        Utility.Wait(0.75)
    ElseIf preparation == 2
        While index < bones.Length
            If !OperationCurrent(token) || !targetActor.Is3DLoaded()
                Return false
            EndIf
            CBPCPluginScript.StopPhysics(targetActor, bones[index])
            index += 1
        EndWhile
        If !OperationCurrent(token) || !targetActor.Is3DLoaded()
            Return false
        EndIf
        DynamicHDT.TogglePhysics(targetActor, bones, false)
        Utility.Wait(0.35)
    EndIf
    If !OperationCurrent(token) || !targetActor.Is3DLoaded()
        Return false
    EndIf
    index = 0
    If useCBPC
        DynamicHDT.TogglePhysics(targetActor, bones, false)
        Utility.Wait(0.25)
        While index < bones.Length
            If !OperationCurrent(token) || !targetActor.Is3DLoaded()
                Return false
            EndIf
            CBPCPluginScript.StartPhysics(targetActor, bones[index])
            index += 1
        EndWhile
    Else
        While index < bones.Length
            If !OperationCurrent(token) || !targetActor.Is3DLoaded()
                Return false
            EndIf
            CBPCPluginScript.StopPhysics(targetActor, bones[index])
            index += 1
        EndWhile
        If preparation == 4
            If !OperationCurrent(token) || !targetActor.Is3DLoaded()
                Return false
            EndIf
            ; Reloaded SMP bodies are already dynamic. Briefly follow the
            ; skeleton after the reload so their simulated transforms catch up
            ; with the soft pose before dynamics resume. Do not freeze before
            ; rebuilding, and do not reset unrelated physics systems.
            DynamicHDT.TogglePhysics(targetActor, bones, false)
        EndIf
        Utility.Wait(0.25)
        If !OperationCurrent(token) || !targetActor.Is3DLoaded()
            Return false
        EndIf
        DynamicHDT.TogglePhysics(targetActor, bones, true)
    EndIf
    ; Execution acknowledgment is not read-back of live engine ownership.
    Return OperationCurrent(token) && targetActor.Is3DLoaded()
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
