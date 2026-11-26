Scriptname SPS_OStimBridge Hidden

Int Function GetSPSBridgeVersion() Global
    Return 4
EndFunction

; The role of any actor in an OStim scene (player and NPCs).
; 0 unknown, 1 receiving/on the bottom, 2 penis in use or dominant/on top. Called only from
; inside the actor bridge after native target validation.
Int Function GetActorRole(Actor targetActor) Global
    If targetActor == None || !OActor.IsInOStim(targetActor)
        Return 0
    EndIf
    Int threadID = OActor.GetThreadID(targetActor)
    If threadID < 0
        Return 0
    EndIf
    Int position = OThread.GetActorPosition(threadID, targetActor)
    If position < 0
        Return 0
    EndIf
    String sceneId = OThread.GetScene(threadID)
    If sceneId == ""
        Return 0
    EndIf
    ; The scene's actions decide first. OStim NG action files name
    ; whose penis an action uses ("requirements": ["penis"] on the action actor
    ; or target, SKSE/Plugins/OStim/actions/*.json). The position tags alone
    ; read the receiver of a blowjob, handjob or titjob as unknown or receiving,
    ; and a man on his back in cowgirl ("onbottom") as receiving, so his shaft
    ; stayed soft. -1 means not found.
    If OMetadata.FindAnyActionForActorCSV(sceneId, position, "vaginalsex,analsex,rubbingpenisagainstface,facial,cumonbutt,cumonchest,cumonvulva") >= 0
        Return 2
    EndIf
    If OMetadata.FindAnyActionForTargetCSV(sceneId, position, "blowjob,deepthroating,penilelicking,handjob,boobjob,footjob,thighjob,buttjob,tailjob,grindingpenis,malemasturbation") >= 0
        Return 2
    EndIf
    If OMetadata.HasActorTag(sceneId, position, "dominant") || OMetadata.HasActorTag(sceneId, position, "ontop")
        Return 2
    EndIf
    If OMetadata.HasActorTag(sceneId, position, "onbottom")
        Return 1
    EndIf
    Return 0
EndFunction

; Called only from inside the actor bridge after native target validation.
; Actor membership covers NPC threads and a player/NPC shared scene alike.
Int Function GetActorSceneState(Actor targetActor) Global
    If targetActor == None
        Return 0
    EndIf
    If OActor.IsInOStim(targetActor)
        Return 2
    EndIf
    Return 1
EndFunction

; Returns 0 when the current OStim scene cannot describe the player's role,
; 1 when the player is receiving/on the bottom, or 2 when the player's penis is
; used by an action or the player is dominant/on top. The player's thread is always ThreadID 0 (OThread.psc:3),
; which GetActorRole resolves through OActor.GetThreadID.
Int Function GetPlayerRole() Global
    Return GetActorRole(Game.GetPlayer())
EndFunction
