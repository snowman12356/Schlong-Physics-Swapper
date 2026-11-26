Scriptname SPS_FSMPBridge Hidden

Int Function GetSPSBridgeVersion() Global
    Return 12
EndFunction

Bool Function EnterOperation(String token) Global Native
Bool Function OperationCurrent(String token) Global Native
Bool Function QueueResetBarrier(String token) Global Native
Bool Function ResetBarrierPassed(String token) Global Native

; Actor-scoped guards for managed NPC ownership. These are separate from the
; player entry points: a token resolves to one actor, and every mutation below
; re-checks it so a removed, reloaded or superseded NPC target is rejected
; instead of silently resolving to the player.
Bool Function OwnershipRequestCurrent(String token, Actor targetActor) Global Native
Actor Function ResolveOwnershipRequest(String token) Global Native

; One lease covers preparation, settling and the final owner. Expired tokens
; cannot start work or be overtaken while their stack is still running.
; preparation: 0 ordinary handoff, 1 full player reset, 2 equipment reconnect,
; 3 SMP-off drift probe only, 6 clean soft rest bake, 7 CBPC release only. Later
; steps are rejected if the player's 3D unloads. Every reset-barrier wait is
; bounded: a barrier that never passes ends the stack instead of holding the lease.
; The bones are sampled again after an SMP enable. FSMP returns
; the state before the call and does not change a bone that is already dynamic,
; so a true entry on that second call is proof the bone is dynamic. A false
; entry is still "kinematic or not found" and proves nothing. CBPC's
; StartPhysics and StopPhysics return nothing and are not part of this proof.
;   result[0] = 1 when the operation is still valid and the player 3D is present
;   result[1] = managed bones that were DYNAMIC at the disable sample, or -1
;               when this path never sampled a disable. A false entry is never
;               proof that SMP let go.
;   result[2] = bone array length
;   result[3] = managed bones DYNAMIC on the post-enable sample, or -1 when
;               this path did not ask SMP to take the bones. Equal to result[2]
;               is the only certain SMP handoff.
Int[] Function SetPlayerOwnerV3(String token, Bool useCBPC, Int preparation, Int softBend) Global
    Int[] result = new Int[4]
    result[0] = 0
    result[1] = -1
    result[2] = 0
    result[3] = -1
    If !EnterOperation(token)
        Return result
    EndIf
    Actor targetActor = Game.GetPlayer()
    If targetActor == None || !targetActor.Is3DLoaded() || !OperationCurrent(token)
        Return result
    EndIf
    String[] bones = GetPhysicsBones()
    result[2] = bones.Length
    If preparation == 3
        If !useCBPC || !OperationCurrent(token) || !targetActor.Is3DLoaded()
            Return result
        EndIf
        ; Maintain the existing CBPC owner without restarting it or its pose.
        ; This is the only passive drift probe in the whole plugin: the sample is
        ; FSMP's own previous-state read-back for bones SPS already believes are
        ; kinematic. Any dynamic bone here means SMP resumed ownership unseen.
        result[1] = CountDynamicBones(DynamicHDT.TogglePhysics(targetActor, bones, false))
        If OperationCurrent(token) && targetActor.Is3DLoaded()
            result[0] = 1
        EndIf
        Return result
    EndIf
    Int index = 0
    Int barrierWaits = 0
    If preparation == 7
        ; CBPC release only. CBPC keeps its stop flags in a map keyed
        ; by actor and node name and clears that whole map on every game load,
        ; so after a load CBPC simulates the shaft again next to SMP until SPS
        ; stops it. This stops CBPC on the six bones and touches nothing else:
        ; no FSMP call, no pose, no reset. A later CBPC owner starts it again.
        If useCBPC
            Return result
        EndIf
        While index < bones.Length
            If !OperationCurrent(token) || !targetActor.Is3DLoaded()
                Return result
            EndIf
            CBPCPluginScript.StopPhysics(targetActor, bones[index])
            index += 1
        EndWhile
        If OperationCurrent(token) && targetActor.Is3DLoaded()
            result[0] = 1
        EndIf
        Return result
    EndIf
    If preparation == 6
        ; Clean soft rest bake. A reset bakes SMP's rest frame from
        ; the bones' pose at that moment (FSMP builds each joint frame from the
        ; current transforms), so resetting while SMP still drives a swinging or
        ; scene-stretched shaft bakes that pose in. Hand the six bones to the
        ; animation first, play the resting clip and let it blend in, then
        ; rebuild: the full reset creates new, dynamic SMP bodies from that pose,
        ; so the final TogglePhysics(true) is only the proof sample.
        ; softBend: 0-18 = SOSBend(softBend - 9), else no clip.
        If useCBPC
            Return result
        EndIf
        ; Stop CBPC first: CBPC clears its stop flags on every load (its stop
        ; flags are keyed by actor and node name), so it may be running again.
        While index < bones.Length
            If !OperationCurrent(token) || !targetActor.Is3DLoaded()
                Return result
            EndIf
            CBPCPluginScript.StopPhysics(targetActor, bones[index])
            index += 1
        EndWhile
        If !OperationCurrent(token) || !targetActor.Is3DLoaded()
            Return result
        EndIf
        result[1] = CountDynamicBones(DynamicHDT.TogglePhysics(targetActor, bones, false))
        If !OperationCurrent(token) || !targetActor.Is3DLoaded()
            Return result
        EndIf
        If softBend >= 0 && softBend <= 18
            Debug.SendAnimationEvent(targetActor, "SOSBend" + (softBend - 9))
        EndIf
        Utility.Wait(0.9)
        If !OperationCurrent(token) || !targetActor.Is3DLoaded()
            Return result
        EndIf
        DynamicHDT.ResetPhysics(targetActor, true)
        If !QueueResetBarrier(token)
            Return result
        EndIf
        barrierWaits = 0
        While !ResetBarrierPassed(token)
            If !OperationCurrent(token) || !targetActor.Is3DLoaded() || barrierWaits >= 100
                Return result
            EndIf
            barrierWaits += 1
            Utility.Wait(0.1)
        EndWhile
        Utility.Wait(0.4)
        If !OperationCurrent(token) || !targetActor.Is3DLoaded()
            Return result
        EndIf
        result[3] = CountDynamicBones(DynamicHDT.TogglePhysics(targetActor, bones, true))
        If OperationCurrent(token) && targetActor.Is3DLoaded()
            result[0] = 1
        EndIf
        Return result
    EndIf
    If preparation == 1
        If softBend >= 0
            Debug.SendAnimationEvent(targetActor, "SOSFlaccid")
            If !OperationCurrent(token) || !targetActor.Is3DLoaded()
                Return result
            EndIf
            ; Experimental: SPS_PoseAngle rotates GenBase, but the SOS/TNG
            ; animation graph rewrites that bone every frame, so the angle does
            ; not persist. SOS AE 4.0.0 removed SOSAE_SKSE.SetSchlongBend.
            SPS_PoseAngle.SetActorPoseAngle(targetActor, softBend, -1.0)
            If !OperationCurrent(token) || !targetActor.Is3DLoaded()
                Return result
            EndIf
        EndIf
        DynamicHDT.ResetPhysics(targetActor, true)
        If !QueueResetBarrier(token)
            Return result
        EndIf
        barrierWaits = 0
        While !ResetBarrierPassed(token)
            If !OperationCurrent(token) || !targetActor.Is3DLoaded() || barrierWaits >= 100
                Return result
            EndIf
            barrierWaits += 1
            Utility.Wait(0.1)
        EndWhile
        Utility.Wait(0.75)
    ElseIf preparation == 2
        While index < bones.Length
            If !OperationCurrent(token) || !targetActor.Is3DLoaded()
                Return result
            EndIf
            CBPCPluginScript.StopPhysics(targetActor, bones[index])
            index += 1
        EndWhile
        If !OperationCurrent(token) || !targetActor.Is3DLoaded()
            Return result
        EndIf
        result[1] = CountDynamicBones(DynamicHDT.TogglePhysics(targetActor, bones, false))
        Utility.Wait(0.35)
    EndIf
    If !OperationCurrent(token) || !targetActor.Is3DLoaded()
        Return result
    EndIf
    index = 0
    If useCBPC
        ; Sampled before the disable so a still-dynamic bone is visible.
        ; Preparation 2 already took that sample above; the second disable then
        ; only repeats the call and must not overwrite the sample.
        If preparation != 2
            result[1] = CountDynamicBones(DynamicHDT.TogglePhysics(targetActor, bones, false))
        Else
            DynamicHDT.TogglePhysics(targetActor, bones, false)
        EndIf
        Utility.Wait(0.25)
        While index < bones.Length
            If !OperationCurrent(token) || !targetActor.Is3DLoaded()
                Return result
            EndIf
            CBPCPluginScript.StartPhysics(targetActor, bones[index])
            index += 1
        EndWhile
    Else
        While index < bones.Length
            If !OperationCurrent(token) || !targetActor.Is3DLoaded()
                Return result
            EndIf
            CBPCPluginScript.StopPhysics(targetActor, bones[index])
            index += 1
        EndWhile
        Utility.Wait(0.25)
        If !OperationCurrent(token) || !targetActor.Is3DLoaded()
            Return result
        EndIf
        DynamicHDT.TogglePhysics(targetActor, bones, true)
        If !OperationCurrent(token) || !targetActor.Is3DLoaded()
            Return result
        EndIf
        result[3] = CountDynamicBones(DynamicHDT.TogglePhysics(targetActor, bones, true))
    EndIf
    If OperationCurrent(token) && targetActor.Is3DLoaded()
        result[0] = 1
    EndIf
    Return result
EndFunction

; Actor-scoped ownership handoff for an explicitly managed NPC. This mirrors the
; ordering of SetPlayerOwnerV3 without the player-only steps: no SOS flaccid
; event and no soft-pose bend, because those are driven by player settings and
; would fight the NPC's own configured pose. ResetPhysics runs only for
; preparation 5, which the plugin requests once after a CBPC to SMP handoff.
;
; preparation: 0 ordinary handoff, 1 stop both engines before reattaching,
; 5 reset only (SMP target; DynamicHDT.ResetPhysics and nothing else).
; The caller must already hold the managed-actor lease; this function verifies
; the resolved actor on both sides of every engine mutation.
;   result[0] = 1 when the operation stayed current through the final step
;   result[1] = managed bones DYNAMIC at the disable sample, or -1 if unsampled
;   result[2] = managed bone array length
;   result[3] = managed bones DYNAMIC after the SMP enable, or -1 when the
;               target is CBPC. Equal to result[2] is the only certain sample.
Int[] Function SetActorOwnerV3(String token, Bool useCBPC, Int preparation) Global
    Int[] result = new Int[4]
    result[0] = 0
    result[1] = -1
    result[2] = 0
    result[3] = -1
    If !EnterOperation(token)
        Return result
    EndIf
    Actor targetActor = ResolveOwnershipRequest(token)
    If targetActor == None || !OwnershipRequestCurrent(token, targetActor) || !OperationCurrent(token) || !targetActor.Is3DLoaded()
        Return result
    EndIf
    String[] bones = GetPhysicsBones()
    result[2] = bones.Length
    Int index = 0
    If preparation == 5
        ; Reset only, for the post-handoff soft refresh when SMP already owns the
        ; NPC's bones. SMP-only: a CBPC target is refused. The final
        ; TogglePhysics(true) is just the proof sample, since FSMP leaves a bone
        ; that is already dynamic unchanged.
        If useCBPC
            Return result
        EndIf
        DynamicHDT.ResetPhysics(targetActor, true)
        If !QueueResetBarrier(token)
            Return result
        EndIf
        Int barrierWaits = 0
        While !ResetBarrierPassed(token)
            If !OwnershipRequestCurrent(token, targetActor) || !OperationCurrent(token) || !targetActor.Is3DLoaded() || barrierWaits >= 100
                Return result
            EndIf
            barrierWaits += 1
            Utility.Wait(0.1)
        EndWhile
        Utility.Wait(0.75)
        If !OwnershipRequestCurrent(token, targetActor) || !OperationCurrent(token) || !targetActor.Is3DLoaded()
            Return result
        EndIf
        result[3] = CountDynamicBones(DynamicHDT.TogglePhysics(targetActor, bones, true))
        If OwnershipRequestCurrent(token, targetActor) && OperationCurrent(token) && targetActor.Is3DLoaded()
            result[0] = 1
        EndIf
        Return result
    EndIf
    If preparation == 1
        While index < bones.Length
            If !OwnershipRequestCurrent(token, targetActor) || !OperationCurrent(token)
                Return result
            EndIf
            CBPCPluginScript.StopPhysics(targetActor, bones[index])
            index += 1
        EndWhile
        If !OwnershipRequestCurrent(token, targetActor) || !OperationCurrent(token) || !targetActor.Is3DLoaded()
            Return result
        EndIf
        DynamicHDT.TogglePhysics(targetActor, bones, false)
        Utility.Wait(0.25)
    EndIf
    If !OwnershipRequestCurrent(token, targetActor) || !OperationCurrent(token) || !targetActor.Is3DLoaded()
        Return result
    EndIf
    index = 0
    If useCBPC
        ; Sampled before the disable so a still-dynamic bone is visible.
        result[1] = CountDynamicBones(DynamicHDT.TogglePhysics(targetActor, bones, false))
        Utility.Wait(0.25)
        While index < bones.Length
            If !OwnershipRequestCurrent(token, targetActor) || !OperationCurrent(token)
                Return result
            EndIf
            CBPCPluginScript.StartPhysics(targetActor, bones[index])
            index += 1
        EndWhile
    Else
        While index < bones.Length
            If !OwnershipRequestCurrent(token, targetActor) || !OperationCurrent(token)
                Return result
            EndIf
            CBPCPluginScript.StopPhysics(targetActor, bones[index])
            index += 1
        EndWhile
        Utility.Wait(0.25)
        If !OwnershipRequestCurrent(token, targetActor) || !OperationCurrent(token) || !targetActor.Is3DLoaded()
            Return result
        EndIf
        DynamicHDT.TogglePhysics(targetActor, bones, true)
        If !OwnershipRequestCurrent(token, targetActor) || !OperationCurrent(token) || !targetActor.Is3DLoaded()
            Return result
        EndIf
        result[3] = CountDynamicBones(DynamicHDT.TogglePhysics(targetActor, bones, true))
    EndIf
    If OwnershipRequestCurrent(token, targetActor) && OperationCurrent(token) && targetActor.Is3DLoaded()
        result[0] = 1
    EndIf
    Return result
EndFunction

; -1 when FSMP returned no array (missing FSMP, null actor or empty bone list).
; Comparing an array to None logs "Cannot cast from None to Bool[]" on every
; call, so the array is tested as a Bool instead.
Int Function CountDynamicBones(Bool[] previousStates) Global
    If !previousStates
        Return -1
    EndIf
    Int count = 0
    Int index = 0
    While index < previousStates.Length
        If previousStates[index]
            count += 1
        EndIf
        index += 1
    EndWhile
    Return count
EndFunction

String[] Function GetPhysicsBones() Global
    String[] bones = new String[6]
    bones[0] = "NPC Genitals01 [Gen01]"
    bones[1] = "NPC Genitals02 [Gen02]"
    bones[2] = "NPC Genitals03 [Gen03]"
    bones[3] = "NPC Genitals04 [Gen04]"
    bones[4] = "NPC Genitals05 [Gen05]"
    bones[5] = "NPC Genitals06 [Gen06]"
    Return bones
EndFunction
