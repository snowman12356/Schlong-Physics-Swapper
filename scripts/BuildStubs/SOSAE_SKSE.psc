Scriptname SOSAE_SKSE Hidden

; Build-only declaration for Schlongs of Skyrim AE 4.0.0.
;
; SPS does not call any SOS AE native. The angle is owned by SPS_PoseAngle, which
; needs no SOS script at all; only the graph events and skeleton shipped by SOS
; are used. These declarations exist so a bridge that queries the mod's addon and
; rank data still compiles against the current API.
;
; SOSAE_SKSE.SetSchlongBend was removed in 4.0.0 and must not be reintroduced:
; the function no longer exists in the script or in SchlongsOfSkyrim.dll.

String Function GetActorAddonName(Actor akActor) Global Native
Int Function GetActorRank(Actor akActor) Global Native
Armor Function GetActorAddonArmor(Actor akActor) Global Native
Bool Function SetActorAddonName(Actor akActor, String asAddonName) Global Native
Bool Function SetActorRank(Actor akActor, Int aiRank) Global Native
Bool Function SetActorAddonArmor(Actor akActor, Armor akArmor) Global Native
