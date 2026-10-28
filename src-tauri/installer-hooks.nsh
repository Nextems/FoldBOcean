!macro NSIS_HOOK_PREINSTALL
  IfFileExists "$INSTDIR\FoldBOcean.exe" 0 +3
    ExecWait '"$INSTDIR\FoldBOcean.exe" --quit-all'
    Sleep 800
!macroend

!macro NSIS_HOOK_POSTINSTALL
  CreateDirectory "$SMPROGRAMS\FoldBOcean"
  CreateShortcut "$SMPROGRAMS\FoldBOcean\FoldBOcean — панель.lnk" "$INSTDIR\FoldBOcean.exe" "--background" "$INSTDIR\FoldBOcean.exe"
  ; Preserve the user's existing autostart choice, updating its executable path.
  ReadRegStr $0 HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "FoldBOcean"
  StrCmp $0 "" +2
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "FoldBOcean" '$\"$INSTDIR\FoldBOcean.exe$\" --background'
  IfSilent +2
    Exec '"$INSTDIR\FoldBOcean.exe" --background'
!macroend

!macro NSIS_HOOK_PREUNINSTALL
  IfFileExists "$INSTDIR\FoldBOcean.exe" 0 +3
    ExecWait '"$INSTDIR\FoldBOcean.exe" --quit-all'
    Sleep 800
  ReadRegStr $0 HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "FoldBOcean"
  StrCmp $0 '$\"$INSTDIR\FoldBOcean.exe$\" --background' 0 +2
    DeleteRegValue HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "FoldBOcean"
  Delete "$SMPROGRAMS\FoldBOcean\FoldBOcean — панель.lnk"
  ; User configuration and presets in AppData are intentionally retained.
!macroend
