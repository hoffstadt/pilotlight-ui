@setlocal
@set dir=%~dp0
@pushd %dir%
@set PATH=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build;%PATH%
@set PATH=C:\Program Files\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build;%PATH%
@set PATH=C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build;%PATH%
@set PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build;%PATH%
@set PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise/VC\Auxiliary\Build;%PATH%

@REM include paths
@set INCLUDES=/I. /I "%WindowsSdkDir%Include\um" /I "%WindowsSdkDir%Include\shared" /I%VULKAN_SDK%\Include
@set INCLUDES=/I "../.." /I "../../backends" %INCLUDES%

@REM sources
@set SOURCES=main.c
@set SOURCES=../../pl_ui_draw.c ../../pl_ui.c ../../pl_ui_widgets.c ../../backends/pl_ui_vulkan.c ../../pl_ui_demo.c %SOURCES%

@REM remove old files
@IF NOT EXIST ..\out mkdir ..\out
@IF EXIST ..\out\win32_vulkan_example.exe del ..\out\win32_vulkan_example.exe

@REM setup development environment
@call vcvarsall.bat amd64

@REM compiler flags
set CommonCompilerFlags=-nologo -Zc:preprocessor -nologo -std:c11 -W4 -permissive- -Od -MDd -D_USE_MATH_DEFINES -D_DEBUG -Zi

@rem disable warnings
set CommonCompilerFlags=-wd4013 -wd4201 -wd4100 -wd4996 -wd4505 -wd4189 -wd5105 -wd4115 %CommonCompilerFlags%

@REM linker flags
set CommonLinkerFlags=/LIBPATH:"%VULKAN_SDK%/Lib" vulkan-1.lib -incremental:no
set CommonLinkerFlags=Ole32.lib ucrtd.lib gdi32.lib user32.lib comctl32.lib Shell32.lib %CommonLinkerFlags%

@REM compile & link
cl %CommonCompilerFlags% %INCLUDES% %SOURCES% /Fe../out/win32_vulkan_example.exe /Fo../out/ /link %CommonLinkerFlags%

@REM cleanup
del ..\out\*.obj

@popd
@endlocal