Scriptname SPS_ArousalBridge Hidden

Int Function GetSPSBridgeVersion() Global
    Return 3
EndFunction

Bool Function ActorRequestCurrent(String token, Actor targetActor) Global Native
Actor Function ResolveActorRequest(String token) Global Native
Bool Function ArousalWriteRequestCurrent(String token, Actor targetActor) Global Native
Actor Function ResolveArousalWriteRequest(String token) Global Native

Float Function GetActorArousal(String token) Global
    Actor targetActor = ResolveActorRequest(token)
    If targetActor == None || !ActorRequestCurrent(token, targetActor)
        Return -1.0
    EndIf
    Float reading = OSLArousedNative.GetArousal(targetActor)
    If !ActorRequestCurrent(token, targetActor)
        Return -1.0
    EndIf
    Return reading
EndFunction

Float Function SetActorArousal(String token, Float value) Global
    Actor targetActor = ResolveArousalWriteRequest(token)
    If targetActor == None || !ArousalWriteRequestCurrent(token, targetActor)
        Return -1.0
    EndIf
    Float result = OSLArousedNative.SetArousal(targetActor, value)
    If !ArousalWriteRequestCurrent(token, targetActor)
        Return -1.0
    EndIf
    Return result
EndFunction

; Legacy OSL fallback. Current OSL is read through its native DLL export.
Float Function GetPlayerArousal() Global
    Actor player = Game.GetPlayer()
    If player == None
        Return -1.0
    EndIf

    Return OSLArousedNative.GetArousal(player)
EndFunction
