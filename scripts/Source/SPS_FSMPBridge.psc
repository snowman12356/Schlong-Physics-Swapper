Scriptname SPS_FSMPBridge Hidden

; Builds the string array inside Papyrus instead of asking the native SPS DLL
; to allocate it through the VM during a load transition.
Function TogglePhysics(Actor targetActor, Bool enabled) Global
    If targetActor == None
        Return
    EndIf

    String[] bones = new String[6]
    bones[0] = "NPC Genitals01 [Gen01]"
    bones[1] = "NPC Genitals02 [Gen02]"
    bones[2] = "NPC Genitals03 [Gen03]"
    bones[3] = "NPC Genitals04 [Gen04]"
    bones[4] = "NPC Genitals05 [Gen05]"
    bones[5] = "NPC Genitals06 [Gen06]"

    DynamicHDT.TogglePhysics(targetActor, bones, enabled)
EndFunction
