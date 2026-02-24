# Microsoft Developer Studio Project File - Name="toodee" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=toodee - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "toodee.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "toodee.mak" CFG="toodee - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "toodee - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "toodee - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "toodee - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /Ob2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "RELEASE_MODE" /FR /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x809 /d "NDEBUG"
# ADD RSC /l 0x809 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib agl.lib alleg.lib user32.lib gdi32.lib opengl32.lib glu32.lib fmodvc.lib /nologo /subsystem:windows /machine:I386

!ELSEIF  "$(CFG)" == "toodee - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "ALLEGRO_NO_ASM" /FR /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x809 /d "_DEBUG"
# ADD RSC /l 0x809 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib agld.lib alld.lib user32.lib gdi32.lib opengl32.lib glu32.lib fmodvc.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# SUBTRACT LINK32 /profile

!ENDIF 

# Begin Target

# Name "toodee - Win32 Release"
# Name "toodee - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\ai_nodes.cpp
# End Source File
# Begin Source File

SOURCE=.\ai_zones.cpp
# End Source File
# Begin Source File

SOURCE=.\arrays.cpp
# End Source File
# Begin Source File

SOURCE=.\control.cpp

!IF  "$(CFG)" == "toodee - Win32 Release"

# ADD CPP /D "FORTIFY"

!ELSEIF  "$(CFG)" == "toodee - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\events.cpp
# End Source File
# Begin Source File

SOURCE=.\file_stuff.cpp
# End Source File
# Begin Source File

SOURCE=.\font_routines.cpp
# End Source File
# Begin Source File

SOURCE=.\FORTIFY.CPP
# End Source File
# Begin Source File

SOURCE=.\funkyfont.cpp
# End Source File
# Begin Source File

SOURCE=.\game.cpp
# End Source File
# Begin Source File

SOURCE=.\global_param_list.cpp
# End Source File
# Begin Source File

SOURCE=.\graphics.cpp
# End Source File
# Begin Source File

SOURCE=.\helpertag.cpp
# End Source File
# Begin Source File

SOURCE=.\hiscores.cpp
# End Source File
# Begin Source File

SOURCE=.\main.cpp
# End Source File
# Begin Source File

SOURCE=.\math_stuff.cpp
# End Source File
# Begin Source File

SOURCE=.\new_editor.cpp
# End Source File
# Begin Source File

SOURCE=.\object_collision.cpp
# End Source File
# Begin Source File

SOURCE=.\output.cpp
# End Source File
# Begin Source File

SOURCE=.\parser.cpp
# End Source File
# Begin Source File

SOURCE=.\particles.cpp
# End Source File
# Begin Source File

SOURCE=.\paths.cpp
# End Source File
# Begin Source File

SOURCE=.\report.cpp
# End Source File
# Begin Source File

SOURCE=.\roomzones.cpp
# End Source File
# Begin Source File

SOURCE=.\scripting.cpp
# End Source File
# Begin Source File

SOURCE=.\simple_gui.cpp
# End Source File
# Begin Source File

SOURCE=.\sound.cpp
# End Source File
# Begin Source File

SOURCE=.\spawn_points.cpp
# End Source File
# Begin Source File

SOURCE=.\string_stuff.cpp
# End Source File
# Begin Source File

SOURCE=.\textfiles.cpp
# End Source File
# Begin Source File

SOURCE=.\tilegroup.cpp
# End Source File
# Begin Source File

SOURCE=.\tilemaps.cpp
# End Source File
# Begin Source File

SOURCE=.\tilesets.cpp
# End Source File
# Begin Source File

SOURCE=.\vector_collision.cpp
# End Source File
# Begin Source File

SOURCE=.\world_collision.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\ai_nodes.h
# End Source File
# Begin Source File

SOURCE=.\ai_zones.h
# End Source File
# Begin Source File

SOURCE=.\arrays.h
# End Source File
# Begin Source File

SOURCE=.\bitshift_amount.h
# End Source File
# Begin Source File

SOURCE=.\control.h
# End Source File
# Begin Source File

SOURCE=.\events.h
# End Source File
# Begin Source File

SOURCE=.\file_stuff.h
# End Source File
# Begin Source File

SOURCE=.\font_routines.h
# End Source File
# Begin Source File

SOURCE=.\FORTIFY.H
# End Source File
# Begin Source File

SOURCE=.\funkyfont.h
# End Source File
# Begin Source File

SOURCE=.\game.h
# End Source File
# Begin Source File

SOURCE=.\glext.h
# End Source File
# Begin Source File

SOURCE=.\global_param_list.h
# End Source File
# Begin Source File

SOURCE=.\graphics.h
# End Source File
# Begin Source File

SOURCE=.\helpertag.h
# End Source File
# Begin Source File

SOURCE=.\hiscores.h
# End Source File
# Begin Source File

SOURCE=.\main.h
# End Source File
# Begin Source File

SOURCE=.\math_stuff.h
# End Source File
# Begin Source File

SOURCE=.\new_editor.h
# End Source File
# Begin Source File

SOURCE=.\object_collision.h
# End Source File
# Begin Source File

SOURCE=.\output.h
# End Source File
# Begin Source File

SOURCE=.\parser.h
# End Source File
# Begin Source File

SOURCE=.\particles.h
# End Source File
# Begin Source File

SOURCE=.\paths.h
# End Source File
# Begin Source File

SOURCE=.\physics.h
# End Source File
# Begin Source File

SOURCE=.\report.h
# End Source File
# Begin Source File

SOURCE=.\roomzones.h
# End Source File
# Begin Source File

SOURCE=.\scripting.h
# End Source File
# Begin Source File

SOURCE=.\scripting_enums.h
# End Source File
# Begin Source File

SOURCE=.\shared_collision_defines.h
# End Source File
# Begin Source File

SOURCE=.\simple_gui.h
# End Source File
# Begin Source File

SOURCE=.\sound.h
# End Source File
# Begin Source File

SOURCE=.\spawn_points.h
# End Source File
# Begin Source File

SOURCE=.\string_size_constants.h
# End Source File
# Begin Source File

SOURCE=.\string_stuff.h
# End Source File
# Begin Source File

SOURCE=.\textfiles.h
# End Source File
# Begin Source File

SOURCE=.\tilegroup.h
# End Source File
# Begin Source File

SOURCE=.\tilemaps.h
# End Source File
# Begin Source File

SOURCE=.\tilesets.h
# End Source File
# Begin Source File

SOURCE=.\UFORTIFY.H
# End Source File
# Begin Source File

SOURCE=.\vector_collision.h
# End Source File
# Begin Source File

SOURCE=.\version.h
# End Source File
# Begin Source File

SOURCE=.\world_collision.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
