Scriptname SexLabFramework extends Quest

; Build-only declaration. The installed SexLab P+ script provides the runtime
; implementation; this file is never shipped in the mod package.
sslThreadController Function GetActorController(Actor akActor)
    Return None
EndFunction

Bool Function IsActorActive(Actor akActor)
    Return False
EndFunction

; P+ SexLabFramework.psc:226 / :246 / :251. Mapping: 0 male, 1 female, 2 futa.
Int Function GetSex(Actor akActor)
    Return 0
EndFunction

Function TreatAsFuta(Actor ActorRef)
EndFunction

Function ClearForcedSex(Actor akActor)
EndFunction
