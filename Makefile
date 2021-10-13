all: npshell

npshell:npshell.cpp Process.cpp Process.h
	g++ npshell.cpp Process.cpp -o npshell

clean:
	rm -f npshell