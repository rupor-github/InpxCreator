call "%VS100COMNTOOLS%\..\..\VC\vcvarsall.bat" x86

ml /coff /Zi /c /Flmatch686.lst match686.asm
ml /coff /Zi /c /Flinffas32.lst inffas32.asm
