!include "MUI.nsh"
!include "FileFunc.nsh"
!addincludedir .
!include "LogicLib.nsh"
!verbose 2

!ifndef OUTPUT_FILE
	!define OUTPUT_FILE TeleportClientInstaller.exe
!endif

Icon "TeleportIcon.ico"

; The name of the installer
Name "Teleport VR Client"

OutFile "${OUTPUT_FILE}"

RequestExecutionLevel admin

!ifndef TELEPORT_VERSION_NUMBER
	!define TELEPORT_VERSION_NUMBER 1.0.0
!endif
!ifndef TELEPORT_VERSION_NUMBER
	!define TELEPORT_VERSION_NUMBER 1.0.0
!endif
!ifndef TELEPORT_COMMIT
	!define TELEPORT_COMMIT xxxxxxxx
!endif

!ifndef PLATFORM
	!define PLATFORM x64
!endif

!ifndef LOGFILE
	!define LOGFILE ..\teleport_log.md
!endif

!define /file LOG_TEXT "${LOGFILE}"

InstallDir "C:\Program Files\Teleport"

VIAddVersionKey "FileVersion" "${TELEPORT_VERSION_NUMBER}"
VIAddVersionKey "CompanyName" "Simul Software Ltd"
VIAddVersionKey "LegalCopyright" "© Simul Software Ltd"
VIAddVersionKey "FileDescription" "Teleport Client"
VIProductVersion ${TELEPORT_VERSION_NUMBER}.0

!define RELEASE_DIR ${SIMUL_DIR}\build\bin\

!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP "Teleport Logo 150x57.bmp"

!define MUI_ABORTWARNING

!define WELCOME_TITLE 'Teleport Client ${TELEPORT_VERSION_NUMBER}'
!define MUI_ICON "TeleportIcon.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "Teleport Logo 164x314.bmp" 
!define MUI_WELCOMEPAGE_TITLE '${WELCOME_TITLE}'
!define MUI_WELCOMEFINISHPAGE_BITMAP_NOSTRETCH
!define MUI_WELCOMEPAGE_TEXT "This wizard will install Teleport Client ${TELEPORT_VERSION_NUMBER} to your machine.\n\nCommit ${TELEPORT_COMMIT}\n\n${LOG_TEXT}"
!define MUI_DIRECTORYPAGE_TEXT_TOP "Choose the installation directory."
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "TeleportClientLicense.rtf"

!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_LANGUAGE "English"
	
; BEGIN_INSTALLFILES

Section "-Directories"
CreateDirectory "$INSTDIR\build_pc_client"
; Set output path to the installation directory.
SetOutPath $INSTDIR

SectionEnd

Section "-Documentation"
	SetOutPath "$INSTDIR\docs"
	File /nonfatal "..\teleport_log.md"
SectionEnd


Section "-InstallInfo"
	WriteRegStr HKCU "Software\Simul\TeleportClient" "" $INSTDIR
	WriteUninstaller "$INSTDIR\Uninstall.exe"
SectionEnd

Section "Uninstall"
	RMDir /r "$SMPROGRAMS\Simul\Teleport"
	RMDir "$SMPROGRAMS\Simul"
	Delete "$INSTDIR\Uninstall.exe"
	RMDir /r "$INSTDIR\temp"
	Delete "$INSTDIR\*.*"
	RMDir /r "$INSTDIR"
	DeleteRegKey /ifempty HKCU "Software\Simul\TeleportClient"
SectionEnd

