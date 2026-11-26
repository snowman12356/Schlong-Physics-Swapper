Scriptname SPS_SexLabBridge Hidden

Int Function GetSPSBridgeVersion() Global
    Return 9
EndFunction

; Declares SexLab types, so the DLL calls this script only when SexLab is
; loaded. Callers that also run without SexLab (SPS_PositionBridge,
; SPS_IdentityBridge) reach it through global calls only.

; Actor-level SexLab reads for SPS_PositionBridge. 0 unavailable, 1 clear, 2 active.
Int Function ReadActorSceneState(Actor target) Global
    SexLabFramework sexLab = Game.GetFormFromFile(0xD62, "SexLab.esm") as SexLabFramework
    If target == None || sexLab == None
        Return 0
    EndIf
    If sexLab.IsActorActive(target)
        Return 2
    EndIf
    Return 1
EndFunction

; -1 when SexLab cannot be resolved (failed, not "no role"), otherwise the
; ReadSexLabRole value.
Int Function ReadActorRole(Actor target) Global
    SexLabFramework sexLab = Game.GetFormFromFile(0xD62, "SexLab.esm") as SexLabFramework
    If target == None || sexLab == None
        Return -1
    EndIf
    Return ReadSexLabRole(sexLab.GetActorController(target), target)
EndFunction

; FormID to Actor through the SPS DLL. Game.GetForm returns None for FormIDs
; >= 0x80000000 (light-plugin FE and runtime-created FF references), and
; Game.GetFormEx does not compile against this project's script sources.
Actor Function ResolveIdentityActor(Int formID) Global Native

; SexLab's sex for an actor (SexLabRegistry mapping: 0 male, 1 female, 2 futa,
; 3 creature male, 4 creature female); baseSex = True ignores SexLab's override.
; Called by SPS_IdentityBridge only when SexLab is loaded.
Int Function GetActorSex(Actor target, Bool baseSex) Global
    If target == None
        Return -1
    EndIf
    Return SexLabRegistry.GetSex(target, baseSex)
EndFunction

; Returns 0 when P+ cannot describe the current stage, 1 when the player is
; receiving/servicing a partner, or 2 when the player's penis is active.
Int Function GetPlayerRole() Global
    Actor player = Game.GetPlayer()
    SexLabFramework sexLab = Game.GetFormFromFile(0xD62, "SexLab.esm") as SexLabFramework
    If player == None || sexLab == None
        Return 0
    EndIf

    Return ReadSexLabRole(sexLab.GetActorController(player), player)
EndFunction

; Diagnostic: P+'s own description of the player's current interactions
; (sslThreadModel.GetInteractionStringImpl), logged by the DLL when the SexLab
; role changes so a wrong role can be traced to what P+ detected. "" when the
; player is not in a P+ thread.
String Function GetPlayerInteractionString() Global
    Actor player = Game.GetPlayer()
    SexLabFramework sexLab = Game.GetFormFromFile(0xD62, "SexLab.esm") as SexLabFramework
    If player == None || sexLab == None
        Return ""
    EndIf
    sslThreadController thread = sexLab.GetActorController(player)
    If thread == None
        Return ""
    EndIf
    Return thread.GetInteractionStringImpl(player)
EndFunction

; Shared by GetPlayerRole and ReadActorRole (used by SPS_PositionBridge).
; 0 unknown, 1 receiving, 2 penetrating. Uses P+'s
; sslThreadModel.HasActiveInteractionAny(Actor akPosition, int[] aiInterTypes).
; Type IDs are the SexLabThread.psc constants as of P+ 2.19.0, which renumbered
; them and removed HasInteractionType; re-check them after a P+ update.
; Penetrating wins if a complex stage registers both directions.
; Grinding: P+'s property comments call aGrinding the buttocks side, but P+'s
; own code treats it as the penis side (sslActorAlias.psc:122-128 and
; sslThreadModel.psc:2008, aVaginal || aGrinding), so SPS follows the code.
Int Function ReadSexLabRole(sslThreadController thread, Actor target) Global
    If thread == None || target == None || !thread.HasActor(target) || !thread.IsInteractionRegistered()
        Return 0
    EndIf

    Int[] penetrating = new Int[10]
    penetrating[0] = 23 ; aVaginal
    penetrating[1] = 25 ; aAnal
    penetrating[2] = 21 ; aSkullfuck
    penetrating[3] = 20 ; pDeepthroat
    penetrating[4] = 18 ; pOral
    penetrating[5] = 16 ; pLickingShaft
    penetrating[6] = 12 ; pBoobJob
    penetrating[7] = 10 ; pFootJob
    penetrating[8] = 8  ; pHandJob
    penetrating[9] = 3  ; aGrinding (see above)
    If thread.HasActiveInteractionAny(target, penetrating)
        Return 2
    EndIf

    Int[] receiving = new Int[10]
    receiving[0] = 24 ; pVaginal
    receiving[1] = 26 ; pAnal
    receiving[2] = 22 ; pSkullfuck
    receiving[3] = 19 ; aDeepthroat
    receiving[4] = 17 ; aOral
    receiving[5] = 15 ; aLickingShaft
    receiving[6] = 11 ; aBoobJob
    receiving[7] = 9  ; aFootJob
    receiving[8] = 7  ; aHandJob
    receiving[9] = 4  ; pGrinding
    If thread.HasActiveInteractionAny(target, receiving)
        Return 1
    EndIf

    Return 0
EndFunction

; Native-visible SexLab events contain the thread ID. Resolve the player's
; thread without calling its collision/interaction API during registration.
Int Function GetPlayerThreadID() Global
    Actor player = Game.GetPlayer()
    SexLabFramework sexLab = Game.GetFormFromFile(0xD62, "SexLab.esm") as SexLabFramework
    If player == None || sexLab == None
        Return -2
    EndIf
    sslThreadController thread = sexLab.GetActorController(player)
    If thread == None || !thread.HasActor(player)
        Return -1
    EndIf
    Return thread.GetThreadID()
EndFunction

; futa = True: SexLab treats the actor as futa whatever its base sex (the menu decides
; when to offer this; a creature base sex is a deliberate "force"). futa = False clears
; the SexLab override. Both use SexLab P+'s own TreatAsFuta / ClearForcedSex,
; stored by SexLab as a faction rank on the actor.
; Returns 1 applied, 0 SexLab unavailable, -1 actor not found.
Int Function SetActorFuta(Int formID, Bool futa) Global
    Actor target = ResolveIdentityActor(formID)
    If target == None
        Return -1
    EndIf
    SexLabFramework sexLab = Game.GetFormFromFile(0xD62, "SexLab.esm") as SexLabFramework
    If sexLab == None
        Return 0
    EndIf
    If futa
        sexLab.TreatAsFuta(target)
    Else
        sexLab.ClearForcedSex(target)
    EndIf
    Return 1
EndFunction
