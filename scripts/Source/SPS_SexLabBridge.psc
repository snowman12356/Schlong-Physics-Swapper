Scriptname SPS_SexLabBridge Hidden

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
