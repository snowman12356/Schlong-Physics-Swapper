# Third-party notice

The MIT licence in the repository root covers only original Schlong Physics
Swapper material authored for this project. It does not replace or relicense
third-party material.

Schlong Physics Swapper calls or reads public compatibility interfaces supplied
by SKSE Menu Framework, Faster HDT-SMP, CBPC, OSL Aroused, SLO Aroused NG,
classic SexLab Aroused Redux, SexLab P+, Schlongs of Skyrim AE, and The New
Gentleman. Those projects are not bundled and remain subject to their own
terms.

The DLL is built with CommonLibSSE-NG, SKSE Menu Framework 3 headers, and the
permissively licensed C++ dependencies declared by the build. Binary release
archives include their applicable copyright and licence notices in the
`Licenses` directory and in
[THIRD_PARTY_LICENSES.txt](Licenses/Third-Party-Software-Licenses.txt). These
notices do not imply that the upstream projects endorse Schlong Physics
Swapper.

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
