Scriptname OSLArousedNative Hidden

; Build-only declaration. OSL Aroused supplies the native implementation at runtime.
Float Function GetArousal(Actor actor) Global Native
; Verified against the installed OSL Aroused source and its own MCM setter.
Float Function SetArousal(Actor actor, Float value) Global Native
; Verified against the installed OSL Aroused source; read-only scene membership.
Bool Function IsInScene(Actor actor) Global Native
