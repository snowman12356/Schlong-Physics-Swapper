# Third-party notice

Schlong Physics Swapper calls public Papyrus interfaces supplied by SKSE Menu
Framework, Faster HDT-SMP, CBPC, OSL Aroused, SexLab P+, and Schlongs of Skyrim
AE. Those projects are not bundled and remain subject to their own terms.

## OSL Aroused compatibility override

The included `OSLAroused_Main.psc` is derived from the public OSL Aroused 2.9.0
source at <https://github.com/ozooma10/OSLAroused>. It adds one player check to
`UpdateSOSPosition`, preventing OSL from sending player `SOSFlaccid`/`SOSBend`
events while retaining NPC behavior. The compiled `.pex` was validated against
the installed OSL Aroused 2.9.2 script used during development.

OSL Aroused is released under the Unlicense. A verbatim copy is included at
`compat/OSL Aroused/LICENSE.OSLAroused-Unlicense.txt`. The derivative source and
binary may therefore be copied, modified, compiled, and distributed under those
terms. This repository does not bundle any other OSL Aroused files.
