mycontainer: main.c
	gcc -o mycontainer main.c

run: mycontainer
	sudo ./$<

clean:
	rm -rf mycontainer