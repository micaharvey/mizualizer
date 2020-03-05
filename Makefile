
CXX=g++ -w
INCLUDES=
FLAGS=-D__MACOSX_CORE__ -O3 -c
LIBS=-framework CoreAudio -framework CoreMIDI -framework CoreFoundation \
	-framework IOKit -framework Carbon  -framework OpenGL \
	-framework GLUT -framework Foundation \
	-framework AppKit -lstdc++ -lm
	
OBJS=   RtAudio.o Mizualizer.o chuck_fft.o x-vector3d.o x-fun.o

Mizualizer: $(OBJS)
	$(CXX) -o Mizualizer $(OBJS) $(LIBS)

Mizualizer.o: Mizualizer.cpp RtAudio.h
	$(CXX) $(FLAGS) Mizualizer.cpp

RtAudio.o: RtAudio.h RtAudio.cpp RtError.h
	$(CXX) $(FLAGS) RtAudio.cpp

chuck_fft.o: chuck_fft.h chuck_fft.c
	$(CXX) $(FLAGS) chuck_fft.c

x-vector3d.o: x-vector3d.h x-vector3d.cpp
	$(CXX) $(FLAGS) x-vector3d.cpp

x-fun.o: x-fun.h x-fun.cpp
	$(CXX) $(FLAGS) x-fun.cpp

clean:
	rm -f *~ *# *.o Mizualizer
