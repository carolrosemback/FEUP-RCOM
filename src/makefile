transfer:
	gcc -Wall app.c datalink.c -o app && ./app /dev/tty$(port)

# Use virport to link the sockets
virtport:
	sudo socat -d  -d  PTY,link=/dev/ttyS10,mode=777   PTY,link=/dev/ttyS11,mode=777
