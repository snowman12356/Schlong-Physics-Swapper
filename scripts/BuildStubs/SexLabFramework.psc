Scriptname SexLabFramework extends Quest

; Build-only declaration. The installed SexLab P+ script provides the runtime
; implementation; this file is never shipped in the mod package.
sslThreadController Function GetActorController(Actor akActor)
    Return None
EndFunction

Bool Function IsActorActive(Actor akActor)
    Return False
EndFunction

; P+ SexLabFramework.psc:246 / :251.
Function TreatAsFuta(Actor ActorRef)
EndFunction

Function ClearForcedSex(Actor akActor)
EndFunction
