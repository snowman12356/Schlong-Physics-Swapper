Scriptname SPS_PoseAngle Hidden

; SPS-owned schlong angle (experimental, Position Method=3).
;
; Schlongs of Skyrim AE 4.0.0 removed SOSAE_SKSE.SetSchlongBend. The plugin
; rotates 'NPC GenitalsBase [GenBase]' only; the bones below it are left to CBPC
; or Faster HDT-SMP. The SOS/TNG animation graph rewrites GenBase every frame,
; so the angle does not persist while SOS/TNG animates the bone.

Int Function GetSPSBridgeVersion() Global
    Return 1
EndFunction

; aiBend is clamped to 0-20, the same range the SOS angle used.
; afDegreesAtMaxBend is the tilt at bend 20. Pass -1 to keep the value from the
; INI (Position/PoseAngleDegrees), which is what the physics bridge does.
; Returns false when the actor has no 3D or no base bone, so a missing skeleton
; is never reported as an applied pose.
Bool Function SetActorPoseAngle(Actor akActor, Int aiBend, Float afDegreesAtMaxBend) Global Native

; Restores the orientation captured before SPS first wrote to the bone.
Bool Function ClearActorPoseAngle(Actor akActor) Global Native

; Re-reads the current orientation as the reference the angle is applied to.
; Needed after another mod repositions the assembly, and while tuning.
Bool Function RecaptureActorPoseAngle(Actor akActor) Global Native

; [0] = base bone missing, [1] = bone present, no SPS pose, [2] = posed
Int Function ReadActorPoseAngle(Actor akActor) Global Native

; Writes the resolved bone frame to the SPS log so the pitch axis and sign can
; be confirmed in game.
Function LogActorPoseFrame(Actor akActor) Global Native
