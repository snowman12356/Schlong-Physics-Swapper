Scriptname sslThreadController extends Quest

; Build-only declarations for the public SexLab P+ interaction API.
Bool Function HasActor(Actor akActor)
    Return False
EndFunction

Bool Function IsInteractionRegistered()
    Return False
EndFunction

Bool Function HasInteractionType(Int aiType, Actor akPosition = None, Actor akPartner = None)
    Return False
EndFunction
