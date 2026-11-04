# Request to Shizof: read-only CBPC physics-state query

Draft for the user to send (CBPC Nexus page, mod 21224: Posts, or a private
message to Shizof). Not sent by the agent. Written against CBPC 1.7.3 source
(`CBPC SSE - 2.3.1 - Source 1.7.3`).

---

**Subject:** Small request: read-only query for whether a node is simulated

Hi Shizof,

Up front: I build SPS with AI assistance. I direct the design and test
everything in game, and an AI coding assistant helps write and review the code.
That's how this request was put together too: the AI read your published 1.7.3
source, and I've checked the findings below. Please treat the code sketch as a
suggestion that may contain mistakes, not as tested code.

I'm the author of Schlong Physics Swapper (SPS, Nexus 188174). SPS hands the six
`NPC Genitals01-06` bones between Faster HDT-SMP and CBPC, using your
`CBPCPluginScript.StartPhysics` / `StopPhysics`. Thanks for exposing those, and for
publishing the source.

FSMP's `TogglePhysics` returns each bone's previous state, so SPS can confirm
the SMP side of a handoff. For CBPC there's no way to read anything back.
`StartPhysics`/`StopPhysics` return nothing and `GetVersion()` returns `"1"`. So
SPS can only say it *sent* the commands, not that CBPC is simulating the bones.
Reading your source, the information already exists: the node has a `Thing` in
the actor's bound `SimObj`, and its `ActorNodeStoppedPhysicsMap` entry isn't
`true`.

Would you consider adding a read-only query? Something like:

```papyrus
; True when CBPC currently has this node bound for the actor and it is not stopped.
bool function IsPhysicsActive(Actor npc, String nodeName) global native
; Optional, so callers can feature-detect the query.
int function GetApiVersion() global native
```

A possible implementation using what's already in `config.cpp` / `scan.cpp`. It
was AI-drafted from your source and hasn't been compiled. You know the threading
far better than I do (for example, iterating `things` while `bind()` clears it),
so please treat it as a sketch:

```cpp
bool IsPhysicsActive(StaticFunctionTag*, Actor* actor, BSFixedString nodeName)
{
    if (!actor) return false;
    const std::string key = GetActorNodeString(actor, nodeName);
    auto stop = ActorNodeStoppedPhysicsMap.find(key);
    if (stop != ActorNodeStoppedPhysicsMap.end() && stop->second) return false;
    auto it = actors.find(actor->formID);
    if (it == actors.end() || !it->second.isBound()) return false;
    for (auto& group : it->second.things)
        for (auto& thing : group.second)
            if (thing.second.actorNodeString == key) return true;
    return false;
}
```

If it's easy, an exported C function for SKSE plugins (for example
`int CBPC_GetPhysicsState(UInt32 formID, const char* node)` returning -1 unknown,
0 not simulated, 1 simulated) would also let plugins query without going through
Papyrus. That helps right after a save load, when the Papyrus VM is busy.

SPS would treat it as optional: use it when present, and keep today's behaviour
when it isn't. Users who don't run SPS aren't affected.

One related observation, in case it's useful: `ActorNodeStoppedPhysicsMap` is
cleared on Main Menu and on PostLoadGame. That makes sense, but it means any
mod that stopped a node has to stop it again after every load. A query like the
one above would let such mods check that cheaply.

Thanks for considering it. CBPC is a great mod.

snowman12356 (Schlong Physics Swapper)
