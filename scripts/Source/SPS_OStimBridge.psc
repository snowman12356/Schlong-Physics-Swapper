Scriptname SPS_OStimBridge Hidden

Int Function GetSPSBridgeVersion() Global
    Return 1
EndFunction

; Returns 0 when the current OStim scene cannot describe the player's role,
; 1 when the player is receiving/on the bottom, or 2 when the player is
; dominant/on top. OStim's main player thread is thread 0.
Int Function GetPlayerRole() Global
    Actor player = Game.GetPlayer()
    If player == None
        Return 0
    EndIf

    Int position = OThread.GetActorPosition(0, player)
    If position < 0
        Return 0
    EndIf

    String sceneId = OThread.GetScene(0)
    If sceneId == ""
        Return 0
    EndIf

    If OMetadata.HasActorTag(sceneId, position, "dominant") || OMetadata.HasActorTag(sceneId, position, "ontop")
        Return 2
    EndIf

    If OMetadata.HasActorTag(sceneId, position, "onbottom")
        Return 1
    EndIf

    Return 0
EndFunction
