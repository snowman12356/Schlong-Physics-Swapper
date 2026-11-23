Scriptname SOSAE_SKSE Hidden

; Build-only declaration for Schlongs of Skyrim AE 4.0.0.
;
; SPS calls only the read-only GetActorAddonName and GetActorRank
; (SPS_SexLabBridge.DescribeActor). Angles use the SOS graph events.
;
; SOSAE_SKSE.SetSchlongBend was removed in 4.0.0 and must not be reintroduced:
; the function no longer exists in the script or in SchlongsOfSkyrim.dll.

String Function GetActorAddonName(Actor akActor) Global Native
Int Function GetActorRank(Actor akActor) Global Native
