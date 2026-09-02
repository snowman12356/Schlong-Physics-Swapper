Scriptname SPS_PositionBridge Hidden

Int Function GetSPSBridgeVersion() Global
    Return 1
EndFunction

Bool Function SendPlayerAnimationEvent(String eventName) Global
    Actor player = Game.GetPlayer()
    If player == None || eventName == ""
        Return False
    EndIf

    Debug.SendAnimationEvent(player, eventName)
    Return True
EndFunction

Bool Function SetPlayerSchlongBend(Int bend) Global
    Actor player = Game.GetPlayer()
    If player == None
        Return False
    EndIf

    SOSAE_SKSE.SetSchlongBend(player, bend)
    Return True
EndFunction
