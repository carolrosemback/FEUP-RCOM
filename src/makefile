write:
	gcc -Wall app.c datalink.c statistics.c -o app && ./app /dev/tty$(port) T $(file)
read:
	gcc -Wall app.c datalink.c statistics.c -o app && ./app /dev/tty$(port) R
# Use virport to link the sockets
virtport:
	sudo socat -d -d PTY,link=/dev/ttyS10,mode=777  PTY,link=/dev/ttyS11,mode=777

