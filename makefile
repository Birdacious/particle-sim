run:
	gcc -Wall -g -O0 -o out main.c -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi -DDEBUG
	./out
