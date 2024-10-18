all: build_exe

# This is how to use MSVC for building
#MSVC_ENV := "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
#$(MSVC_ENV) && cl /Fe:decode8086.exe main.c

build_exe: main.c
	gcc -g -o decode8086.exe main.c

clean:
	rm -f decode8086.exe
