debug:
	gcc -Wall -g -O0 -o out main.c stb_image.h tinyobj_loader_c.h -lm -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi -DDEBUG
	./out

run:
	gcc -Wall -O3 -o out main.c stb_image.h tinyobj_loader_c.h -lm -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi
	./out
