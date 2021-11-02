# Use virport to link the sockets, write port=S(number) to write to a port,  read port=S(number) to read from a port
virtport:
	sudo socat -d  -d  PTY,link=/dev/ttyS10,mode=777   PTY,link=/dev/ttyS11,mode=777
write:
	gcc -Wall -o exec/writenoncanonical writenoncanonical.c && ./exec/writenoncanonical /dev/tty$(port)
read:
	gcc -Wall -o exec/noncanonical noncanonical.c && ./exec/noncanonical /dev/tty$(port)
