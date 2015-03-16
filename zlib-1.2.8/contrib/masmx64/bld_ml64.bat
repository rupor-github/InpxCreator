call "%VS120COMNTOOLS%\..\..\VC\vcvarsall.bat" amd64

ml64.exe /Flinffasx64 /c /Zi inffasx64.asm
ml64.exe /Flgvmat64   /c /Zi gvmat64.asm
