INCLUDES  = -I./incl
SOURCES = -I./src

all: main myclient myserver perfclient perfserver
clean:
	rm -rf main myclient myserver perfclient perfserver

myclient: incl/init.h incl/device.h incl/packetio.h incl/ip.h incl/socket.h incl/mylock.h incl/routing.h incl/tcp.h incl/timer.h src/init.cpp src/device.cpp src/packetio.cpp src/ip.cpp  src/socket.cpp src/routing.cpp src/tcp.cpp src/mylock.cpp src/timer.cpp checkpoints/echo_client.c checkpoints/unp.c checkpoints/unp.h 
	g++ -g ${INCLUDES} ${SOURCES} src/init.cpp src/device.cpp src/packetio.cpp src/ip.cpp  src/socket.cpp src/routing.cpp src/tcp.cpp src/mylock.cpp src/timer.cpp checkpoints/echo_client.c checkpoints/unp.c -lpcap -pthread -Wl,--wrap=socket,--wrap=bind,--wrap=listen,--wrap=connect,--wrap=accept,--wrap=read,--wrap=write,--wrap=close,--wrap=getaddrinfo -o myclient -O2

myserver: incl/init.h incl/device.h incl/packetio.h incl/ip.h incl/socket.h incl/mylock.h incl/routing.h incl/tcp.h incl/timer.h src/init.cpp src/device.cpp src/packetio.cpp src/ip.cpp  src/socket.cpp src/routing.cpp src/tcp.cpp src/mylock.cpp src/timer.cpp checkpoints/echo_server.c checkpoints/unp.c checkpoints/unp.h 
	g++ -g ${INCLUDES} ${SOURCES} src/init.cpp src/device.cpp src/packetio.cpp src/ip.cpp  src/socket.cpp src/routing.cpp src/tcp.cpp src/mylock.cpp src/timer.cpp checkpoints/echo_server.c checkpoints/unp.c -lpcap -pthread -Wl,--wrap=socket,--wrap=bind,--wrap=listen,--wrap=connect,--wrap=accept,--wrap=read,--wrap=write,--wrap=close,--wrap=getaddrinfo -o myserver -O2

main: incl/init.h incl/device.h incl/packetio.h incl/ip.h incl/socket.h incl/mylock.h incl/routing.h incl/tcp.h incl/timer.h src/init.cpp src/device.cpp src/packetio.cpp src/ip.cpp  src/socket.cpp src/routing.cpp src/tcp.cpp src/mylock.cpp src/timer.cpp src/main.cpp 
	g++ -g ${INCLUDES} ${SOURCES} src/init.cpp src/device.cpp src/packetio.cpp src/ip.cpp  src/socket.cpp src/routing.cpp src/tcp.cpp src/mylock.cpp src/timer.cpp src/main.cpp checkpoints/unp.c -lpcap -pthread -Wl,--wrap=socket,--wrap=bind,--wrap=listen,--wrap=connect,--wrap=accept,--wrap=read,--wrap=write,--wrap=close,--wrap=getaddrinfo -o main -O2

perfclient: incl/init.h incl/device.h incl/packetio.h incl/ip.h incl/socket.h incl/mylock.h incl/routing.h incl/tcp.h incl/timer.h src/init.cpp src/device.cpp src/packetio.cpp src/ip.cpp  src/socket.cpp src/routing.cpp src/tcp.cpp src/mylock.cpp src/timer.cpp checkpoints/perf_client.c checkpoints/unp.c checkpoints/unp.h 
	g++ -g ${INCLUDES} ${SOURCES} src/init.cpp src/device.cpp src/packetio.cpp src/ip.cpp  src/socket.cpp src/routing.cpp src/tcp.cpp src/mylock.cpp src/timer.cpp checkpoints/perf_client.c checkpoints/unp.c -lpcap -pthread -Wl,--wrap=socket,--wrap=bind,--wrap=listen,--wrap=connect,--wrap=accept,--wrap=read,--wrap=write,--wrap=close,--wrap=getaddrinfo -o perfclient -O2

perfserver: incl/init.h incl/device.h incl/packetio.h incl/ip.h incl/socket.h incl/mylock.h incl/routing.h incl/tcp.h incl/timer.h src/init.cpp src/device.cpp src/packetio.cpp src/ip.cpp  src/socket.cpp src/routing.cpp src/tcp.cpp src/mylock.cpp src/timer.cpp checkpoints/perf_server.c checkpoints/unp.c checkpoints/unp.h 
	g++ -g ${INCLUDES} ${SOURCES} src/init.cpp src/device.cpp src/packetio.cpp src/ip.cpp  src/socket.cpp src/routing.cpp src/tcp.cpp src/mylock.cpp src/timer.cpp checkpoints/perf_server.c checkpoints/unp.c -lpcap -pthread -Wl,--wrap=socket,--wrap=bind,--wrap=listen,--wrap=connect,--wrap=accept,--wrap=read,--wrap=write,--wrap=close,--wrap=getaddrinfo -o perfserver -O2

