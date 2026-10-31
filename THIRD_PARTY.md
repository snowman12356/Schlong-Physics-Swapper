# Third-party notice

The GPL-3.0-or-later licence in the repository root covers only original
Schlong Physics Swapper material authored for this project. It does not replace
or relicense third-party material.

Schlong Physics Swapper calls or reads public compatibility interfaces supplied
by SKSE Menu Framework, Faster HDT-SMP, CBPC, OSL Aroused, SLO Aroused NG,
classic SexLab Aroused Redux, SexLab P+, OStim Standalone, Schlongs of Skyrim
AE, and The New Gentleman. Those projects are not bundled and remain subject
to their own terms. The optional SPS OStim bridge calls OStim's public Papyrus
interfaces and contains no OStim code or assets.

The DLL is built with CommonLibSSE-NG, SKSE Menu Framework 3 headers, and the
permissively licensed C++ dependencies declared by the build. Binary release
archives include their applicable copyright and licence notices in the
`Licenses` directory and in
[THIRD_PARTY_LICENSES.txt](Licenses/Third-Party-Software-Licenses.txt). These
notices do not imply that the upstream projects endorse Schlong Physics
Swapper.

CommonLibSSE-NG is pinned to `alandtse/CommonLibSSE-NG` v15.4.0 or newer. That
project relicensed from MIT to GPL-3.0-or-later (with its Modding Exception and
GPL-3.0 Linking Exception) at v15, so the compiled DLL is distributed under
GPL-3.0-or-later as a combined work. Because the DLL is a combined work with a
statically linked GPL-3.0 component, corresponding source must be made available
alongside binary releases. The permissive notices for the remaining dependencies
are unaffected.

## Optional legacy OSL Aroused compatibility

The optional `OSLAroused_Main.psc` is derived from the public OSL Aroused 2.9.0
source at <https://github.com/ozooma10/OSLAroused>. It adds one player check to
the old Papyrus `UpdateSOSPosition` function, preventing OSL from sending player
`SOSFlaccid`/`SOSBend` events while retaining NPC behavior. It is offered only
for OSL Aroused 2.9.0 through 2.9.2 and must not be installed with OSL 2.9.3 or
newer, where SOS position control moved into the native DLL.

OSL Aroused is released under the Unlicense. A verbatim copy is included at
`compat/OSL Aroused/LICENSE.OSLAroused-Unlicense.txt`. The derivative source and
binary may therefore be copied, modified, compiled, and distributed under those
terms. This repository does not bundle any other OSL Aroused files.

## Optional GT SOFTBODY + SPS compatibility XML

The opt-in `MaleGenitals.xml`, `MaleGenitalsSoft.xml` and
`MaleGenitalsToAnus.xml` files combine SPS-authored six-bone genital dynamics
and collision exclusions compatible with SOS, TNG and UBE meshes with collision
profiles derived from GT SOFTBODY 3.37.2 by Goutou:
<https://www.nexusmods.com/skyrimspecialedition/mods/152103>.

The SOFTBODY Nexus permissions allow modified releases and asset reuse with
credit to the original creator. The author notes prohibit commercial use and
inclusion in directly or indirectly paid compilations. This optional SPS
component is distributed free of charge and must remain non-commercial.
