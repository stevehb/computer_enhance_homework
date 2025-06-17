all: build_exe

# This is how to use MSVC for building
#MSVC_ENV := "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
#$(MSVC_ENV) && cl /Fe:decode8086.exe main.c

coord_gen:
	gcc -O3 -o coord_gen.exe coord_gen.c random_number_generator.c common_funcs.c -lm

dist_processor:
	gcc -g -o dist_processor.exe dist_processor.c common_funcs.c json_parser.c -lm

clean:
	rm -f decode8086.exe
