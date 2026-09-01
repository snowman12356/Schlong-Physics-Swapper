Scriptname SPS_FSMPBridge Hidden

Int Function GetSPSBridgeVersion() Global
    Return 2
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
