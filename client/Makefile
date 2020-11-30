client.out : main.o
	gcc -o client.out main.o -lpthread

main.o : main.c
	gcc -c -o main.o main.c


clean :
	rm *.o *.out
