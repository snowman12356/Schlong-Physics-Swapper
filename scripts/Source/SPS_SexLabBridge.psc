Scriptname SPS_SexLabBridge Hidden

Int Function GetSPSBridgeVersion() Global
    Return 4
EndFunction

Bool Function IsPlayerActive() Global
    Actor player = Game.GetPlayer()
    SexLabFramework sexLab = Game.GetFormFromFile(0xD62, "SexLab.esm") as SexLabFramework
    If player == None || sexLab == None
        Return False
    EndIf

    Return sexLab.IsActorActive(player)
EndFunction

; Returns 0 when P+ cannot describe the current stage, 1 when the player is
; receiving/servicing a partner, or 2 when the player's penis is active.
Int Function GetPlayerRole() Global
    Actor player = Game.GetPlayer()
    SexLabFramework sexLab = Game.GetFormFromFile(0xD62, "SexLab.esm") as SexLabFramework
    If player == None || sexLab == None
        Return 0
    EndIf

    sslThreadController thread = sexLab.GetActorController(player)
    If thread == None || !thread.HasActor(player) || !thread.IsInteractionRegistered()
        Return 0
    EndIf

    ; In P+'s directional interaction API, None/player means the player is the
    ; partner supplying the active penis or grinding action. Penetrating wins
    ; if a complex stage happens to register both directions.
    If thread.HasInteractionType(1, None, player) || thread.HasInteractionType(2, None, player) || thread.HasInteractionType(3, None, player) || thread.HasInteractionType(5, None, player) || thread.HasInteractionType(6, None, player) || thread.HasInteractionType(7, None, player) || thread.HasInteractionType(8, None, player) || thread.HasInteractionType(9, None, player) || thread.HasInteractionType(11, None, player) || thread.HasInteractionType(4, None, player)
        Return 2
    EndIf

    ; player/None means the player is the penetrated or servicing position.
    If thread.HasInteractionType(1, player, None) || thread.HasInteractionType(2, player, None) || thread.HasInteractionType(3, player, None) || thread.HasInteractionType(5, player, None) || thread.HasInteractionType(6, player, None) || thread.HasInteractionType(7, player, None) || thread.HasInteractionType(8, player, None) || thread.HasInteractionType(9, player, None) || thread.HasInteractionType(11, player, None) || thread.HasInteractionType(4, player, None)
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

; Version 4: identity and futa override for the player and managed NPCs.
; The actor is resolved from its FormID here; the native side only asks for
; actors it is already tracking (the player and managed NPCs).
;
; Returns "<addon>|<rank>|<baseSex>|<sex>". addon is "" when SOS AE reports no
; schlong or was not read; the sex fields are -1 when SexLab was not read.
; Returns "" when the FormID does not resolve to an actor.
String Function DescribeActor(Int formID, Bool readSos, Bool readSexLab) Global
    Actor target = Game.GetForm(formID) as Actor
    If target == None
        Return ""
    EndIf
    String addon = ""
    Int rank = 0
    If readSos
        ; SOS AE 4.0 getters only read; they never roll or assign an addon.
        addon = SOSAE_SKSE.GetActorAddonName(target)
        rank = SOSAE_SKSE.GetActorRank(target)
    EndIf
    Int baseSex = -1
    Int sex = -1
    If readSexLab
        baseSex = SexLabRegistry.GetSex(target, True)
        sex = SexLabRegistry.GetSex(target, False)
    EndIf
    Return addon + "|" + rank + "|" + baseSex + "|" + sex
EndFunction

; futa = True: SexLab treats a female (base sex 1) as futa. futa = False clears
; the SexLab override. Both use SexLab P+'s own TreatAsFuta / ClearForcedSex,
; stored by SexLab as a faction rank on the actor.
; Returns 1 applied, 0 not eligible or SexLab unavailable, -1 actor not found.
Int Function SetActorFuta(Int formID, Bool futa) Global
    Actor target = Game.GetForm(formID) as Actor
    If target == None
        Return -1
    EndIf
    SexLabFramework sexLab = Game.GetFormFromFile(0xD62, "SexLab.esm") as SexLabFramework
    If sexLab == None
        Return 0
    EndIf
    If futa
        If SexLabRegistry.GetSex(target, True) != 1
            Return 0
        EndIf
        sexLab.TreatAsFuta(target)
    Else
        sexLab.ClearForcedSex(target)
    EndIf
    Return 1
EndFunction
