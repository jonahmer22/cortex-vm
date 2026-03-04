all:
	gcc main.c ./src/*.c -I ./include -o ./cortex-vm

clean:
	rm ./cortex-vm
