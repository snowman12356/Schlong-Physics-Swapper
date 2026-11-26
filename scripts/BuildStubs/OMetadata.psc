Scriptname OMetadata Hidden

; Build-only declaration verified against OStim NG's public Papyrus API.
; The installed OStim script provides the native runtime implementation.
Bool Function HasActorTag(String sceneID, Int position, String tag) Global Native

; OMetadata.psc (OStim NG, D:\Modding\usefulmods\ostim ng\Scripts\Source):
; index of the first matching action for that action actor/target, otherwise -1.
Int Function FindAnyActionForActorCSV(String Id, Int Position, String Types) Global Native
Int Function FindAnyActionForTargetCSV(String Id, Int Position, String Types) Global Native
