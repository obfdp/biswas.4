all: oss user clock 

oss: oss.o
	gcc -g -o oss oss.o
	
user: user.o
	gcc -g -o user user.o
	
clock: clock.o
	gcc -g -o clock clock.o
	
oss.o: oss.c
	gcc -g -c oss.c

user.o: user.c 
	gcc -g -c user.c
	
clock.o: clock.c
	gcc -g -c clock.c
	
clean: remove

remove:
	rm *.o oss user clock *.out

success: 
	$(info SUCCESS)
	


