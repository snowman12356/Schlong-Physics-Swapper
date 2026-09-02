Scriptname SPS_ArousalBridge Hidden

Int Function GetSPSBridgeVersion() Global
    Return 1
EndFunction

; Legacy OSL fallback. Current OSL is read through its native DLL export.
Float Function GetPlayerArousal() Global
    Actor player = Game.GetPlayer()
    If player == None
        Return -1.0
    EndIf

    Return OSLArousedNative.GetArousal(player)
EndFunction
