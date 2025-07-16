all: coord_gen dist_processor

# This is how to use MSVC for building
#MSVC_ENV := "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
#$(MSVC_ENV) && cl /Fe:decode8086.exe main.c

MSVC_ENV := "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

coord_gen:
#	gcc -O3 -o coord_gen.exe coord_gen.c random_number_generator.c common_funcs.c -lm
	cl /O2 /Fe:coord_gen.exe coord_gen.c random_number_generator.c common_funcs.c

dist_processor:
#	gcc -O3 -o dist_processor.exe dist_processor.c common_funcs.c json_parser.c timer.c -lm
	cl /O2 /Fe:dist_processor.exe dist_processor.c common_funcs.c json_parser.c timer.c

dist_processor_debug:
	cl /Zi /Fe:dist_processor.exe dist_processor.c common_funcs.c json_parser.c timer.c

timer_test:
#	gcc -O3 -o timer_test.exe timer_test.c timer.c -lm
	cl /O2 /Fe:timer_test.exe timer_test.c timer.c

clean:
	rm -f *.pdb
	rm -f *.ilk
	rm -f *.obj
	rm -f coord_gen.exe
	rm -f dist_processor.exe
	rm -f timer_test.exe
