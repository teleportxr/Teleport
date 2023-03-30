echo on
setlocal enabledelayedexpansion
set fls=
echo inputs %1 %2
set TIDY=%1
echo TIDY %TIDY%

cd %2

FOR /r %%i IN (*.html) DO (
	set fls=!fls! %%i
)

echo off
del *.new
ver>nul

for %%a in (!fls!) do (
	echo %%a
	call set trg=%%a
	set trg=!trg:html=new!
	echo call %TIDY% %%a > !trg!
	call %TIDY% %%a > !trg!
)

for %%a in (!fls!) do (
	call set trg=%%a
	set trg=!trg:html=new!
	copy /b/v/y !trg! %%a
)

del /s /q *.new
