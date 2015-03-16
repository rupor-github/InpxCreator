call "%VS120COMNTOOLS%\..\..\VC\vcvarsall.bat" x86

ml /coff /safeseh /Zi /c /Flmatch686.lst match686.asm
ml /coff /safeseh /Zi /c /Flinffas32.lst inffas32.asm
