# OSL Aroused player-position compatibility override

Upstream: <https://github.com/ozooma10/OSLAroused>

Base source: `contrib/Distribution/PapyrusSources/OSLAroused_Main.psc` from the
public `2.9.0` tag. The installed binary was validated with OSL Aroused 2.9.2.

The patch changes `UpdateSOSPosition` from:

```papyrus
if(act == none || !EnableSOSIntegration)
```

to:

```papyrus
if(act == none || act == PlayerRef || !EnableSOSIntegration)
```

This leaves all NPC SOS behavior unchanged and prevents OSL from competing with
Schlong Physics Swapper for the player's SOS position. See
`LICENSE.OSLAroused-Unlicense.txt` for the upstream terms.
