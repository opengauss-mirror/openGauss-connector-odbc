; psqlODBC.nsi
;
; This script is refered to example2.nsi and created for install psqlODBC.
; It will install odbc library into a directory that the user selects.
;
;--------------------------------

; The name of the installer
Name "psqlODBC"

; The file to write
OutFile "psqlODBC.exe"

; Request application privileges for Windows Vista and higher
RequestExecutionLevel admin

; Build Unicode installer
Unicode True

; The default installation directory
InstallDir $PROGRAMFILES\psqlODBC

; Registry key to check for directory (so if you install again, it will 
; overwrite the old one automatically)
InstallDirRegKey HKLM "Software\psqlODBC.ODBC.Driver" "Install_Dir"

;InstallDirRegKey HKLM "SOFTWARE\ODBC\ODBCINST.INI" "PostgreSQL Unicode" 

;--------------------------------

; Pages

Page components
Page directory
Page instfiles

UninstPage uninstConfirm
UninstPage instfiles

;--------------------------------

; The stuff to install
Section "ODBC Driver (required)"

  SectionIn RO
  
  ; Set output path to the installation directory.
  SetOutPath $INSTDIR
  
  ; Put file there
  File "win32_dll\libcrypto-1_1.dll"
  File "win32_dll\libssl-1_1.dll"
  File "win32_dll\psqlodbc35w.dll"
  
  SetRegView 32
  ; Write the installation path into the registry
  WriteRegStr HKLM "SOFTWARE\psqlODBC.ODBC.Driver" "Install_Dir" "$INSTDIR"
  
  ; Write ODBC Driver information into the registry 
  WriteRegStr HKLM "SOFTWARE\ODBC\ODBCINST.INI\ODBC Drivers" "PostgreSQL Unicode" "Installed"
  
  WriteRegStr HKLM "SOFTWARE\ODBC\ODBCINST.INI\PostgreSQL Unicode" "APILevel" "1"
  WriteRegStr HKLM "SOFTWARE\ODBC\ODBCINST.INI\PostgreSQL Unicode" "ConnectFunctions" "YYN"
  WriteRegStr HKLM "SOFTWARE\ODBC\ODBCINST.INI\PostgreSQL Unicode" "Driver" "$INSTDIR\psqlodbc35w.dll"
  WriteRegStr HKLM "SOFTWARE\ODBC\ODBCINST.INI\PostgreSQL Unicode" "DriverODBCVer" "03.51"
  WriteRegStr HKLM "SOFTWARE\ODBC\ODBCINST.INI\PostgreSQL Unicode" "FileUsage" "0"
  WriteRegStr HKLM "SOFTWARE\ODBC\ODBCINST.INI\PostgreSQL Unicode" "Setup" "$INSTDIR\psqlodbc35w.dll"
  WriteRegStr HKLM "SOFTWARE\ODBC\ODBCINST.INI\PostgreSQL Unicode" "SQLLevel" "1"
  WriteRegDWORD HKLM "SOFTWARE\ODBC\ODBCINST.INI\PostgreSQL Unicode" "UsageCount" 1

  ; Write the uninstall keys for Windows
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\psqlODBC" "DisplayName" "psqlODBC"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\psqlODBC" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\psqlODBC" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\psqlODBC" "NoRepair" 1

  SetRegView 64
  ; Write the installation path into the registry
  WriteRegStr HKLM "SOFTWARE\psqlODBC.ODBC.Driver" "Install_Dir" "$INSTDIR"
  
  ; Write ODBC Driver information into the registry 
  WriteRegStr HKLM "SOFTWARE\ODBC\ODBCINST.INI\ODBC Drivers" "PostgreSQL Unicode" "Installed"
  
  WriteRegStr HKLM "SOFTWARE\ODBC\ODBCINST.INI\PostgreSQL Unicode" "APILevel" "1"
  WriteRegStr HKLM "SOFTWARE\ODBC\ODBCINST.INI\PostgreSQL Unicode" "ConnectFunctions" "YYN"
  WriteRegStr HKLM "SOFTWARE\ODBC\ODBCINST.INI\PostgreSQL Unicode" "Driver" "$INSTDIR\psqlodbc35w.dll"
  WriteRegStr HKLM "SOFTWARE\ODBC\ODBCINST.INI\PostgreSQL Unicode" "DriverODBCVer" "03.51"
  WriteRegStr HKLM "SOFTWARE\ODBC\ODBCINST.INI\PostgreSQL Unicode" "FileUsage" "0"
  WriteRegStr HKLM "SOFTWARE\ODBC\ODBCINST.INI\PostgreSQL Unicode" "Setup" "$INSTDIR\psqlodbc35w.dll"
  WriteRegStr HKLM "SOFTWARE\ODBC\ODBCINST.INI\PostgreSQL Unicode" "SQLLevel" "1"
  WriteRegDWORD HKLM "SOFTWARE\ODBC\ODBCINST.INI\PostgreSQL Unicode" "UsageCount" 1

  ; Write the uninstall keys for Windows
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\psqlODBC" "DisplayName" "psqlODBC"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\psqlODBC" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\psqlODBC" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\psqlODBC" "NoRepair" 1

  WriteUninstaller "$INSTDIR\uninstall.exe"
SectionEnd

;--------------------------------

; Uninstaller

Section "Uninstall"
	
  SetRegView 32
  ; Remove registry keys
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\psqlODBC"

  DeleteRegKey HKLM "SOFTWARE\psqlODBC.ODBC.Driver"
  DeleteRegValue HKLM "SOFTWARE\ODBC\ODBCINST.INI\ODBC Drivers" "PostgreSQL Unicode"
  DeleteRegKey HKLM "SOFTWARE\ODBC\ODBCINST.INI\PostgreSQL Unicode" 

  SetRegView 64
  ; Remove registry keys
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\psqlODBC"

  DeleteRegKey HKLM "SOFTWARE\psqlODBC.ODBC.Driver"
  DeleteRegValue HKLM "SOFTWARE\ODBC\ODBCINST.INI\ODBC Drivers" "PostgreSQL Unicode"
  DeleteRegKey HKLM "SOFTWARE\ODBC\ODBCINST.INI\PostgreSQL Unicode" 

  ; Remove files and uninstaller
  Delete $INSTDIR\libcrypto-1_1.dll
  Delete $INSTDIR\libssl-1_1.dll
  Delete $INSTDIR\psqlodbc35w.dll
  Delete "$INSTDIR\uninstall.exe"

  ; Remove directories
  RMDir "$INSTDIR"

SectionEnd
