Build boost 1.44.0 static here (32 and 64 bits)

bjam msvc architecture=x86 extern-c-nothrow=on --build-dir="%root%\b_b" --build-type=complete --without-python --without-mpi --stagedir="%root%\stage" stage
FOR /D /R "%root%\b_b" %%x IN (*) DO copy "%%x\libboost*.pdb" "%root%\stage\lib\."

bjam architecture=x86 address-model=64 extern-c-nothrow=on --build-dir="%root%\b_b_64" --build-type=complete --without-python --without-mpi --stagedir="%root%\stage64" stage
FOR /D /R "%root%\b_b_64" %%x IN (*) DO copy "%%x\libboost*.pdb" "%root%\stage64\lib\."
