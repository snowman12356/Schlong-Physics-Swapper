Scriptname sslThreadController extends Quest

; Build-only declarations. In SexLab P+ these live on sslThreadModel, which
; sslThreadController extends (sslThreadModel.psc:405-411).
Bool Function HasActor(Actor akActor)
    Return False
EndFunction

Bool Function IsInteractionRegistered()
    Return False
EndFunction

Bool Function HasActiveInteractionAny(Actor akPosition, Int[] aiInterTypes)
    Return False
EndFunction

Int Function GetThreadID()
    Return -1
EndFunction
