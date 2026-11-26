Scriptname SPS_IdentityBridge Hidden

; Called for the player and managed NPCs whenever SOS AE or SexLab is present,
; including games without SexLab. No variable, parameter or cast in this script
; may use a SexLab (or other optional mod) script type; SexLab sex reads go
; through SPS_SexLabBridge, which the DLL only asks for when SexLab is loaded.

Int Function GetSPSBridgeVersion() Global
    Return 1
EndFunction

; FormID to Actor through the SPS DLL. Game.GetForm returns None for FormIDs
; >= 0x80000000 (light-plugin FE and runtime-created FF references).
Actor Function ResolveIdentityActor(Int formID) Global Native

; Returns "<addon>|<rank>|<baseSex>|<sex>". addon is "" when SOS AE reports no
; schlong or was not read; the sex fields are -1 when SexLab was not read.
; Returns "" when the FormID does not resolve to an actor.
String Function DescribeActor(Int formID, Bool readSos, Bool readSexLab) Global
    Actor target = ResolveIdentityActor(formID)
    If target == None
        Return ""
    EndIf
    String addon = ""
    Int rank = 0
    If readSos
        ; SOS AE 4.0 getters only read; they never roll or assign an addon.
        addon = SOSAE_SKSE.GetActorAddonName(target)
        rank = SOSAE_SKSE.GetActorRank(target)
    EndIf
    Int baseSex = -1
    Int sex = -1
    If readSexLab
        baseSex = SPS_SexLabBridge.GetActorSex(target, True)
        sex = SPS_SexLabBridge.GetActorSex(target, False)
    EndIf
    Return addon + "|" + rank + "|" + baseSex + "|" + sex
EndFunction
