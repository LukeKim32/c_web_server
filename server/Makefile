server.out : requestQueue.o main.o worker.o http.o hashMap.o
	gcc -o server.out requestQueue.o main.o worker.o http.o hashMap.o -lpthread

main.o : main.c
	gcc -c -o main.o main.c

requestQueue.o : requestQueue.c
	gcc -c -o requestQueue.o requestQueue.c -lpthread

worker.o : worker.c
	gcc -c -o worker.o worker.c -lpthread

http.o : http.c
	gcc -c -o http.o http.c

hashMap.o : hashMap.c
	gcc -c -o hashMap.o hashMap.c

clean :
	rm *.o server.out

