default: weather

#CFLAGS  = -W -Wall -Wextra -Werror
CFLAGS += --std=c++2a

LFLAGS  = -lpthread -lX11 -lpng -lGL
	
CFLAGS_WEB += --std=c++2a 
CFLAGS_WEB += -O3
CFLAGS_WEB += -s USE_LIBPNG=1
CFLAGS_WEB += -s ALLOW_MEMORY_GROWTH=1
CFLAGS_WEB += -s USE_GLFW=3 
CFLAGS_WEB += -s LLD_REPORT_UNDEFINED 
CFLAGS_WEB += -s MAX_WEBGL_VERSION=2 
CFLAGS_WEB += -s MIN_WEBGL_VERSION=2

weather:
	g++ $(CFLAGS) weather.cc -o app $(LFLAGS)
	
test:
	g++ $(CFLAGS) test.cc -o testing $(LFLAGS)
	
test_web:
	em++ $(CFLAGS_WEB) test.cc -o testing.html
	
clean:
	rm -f testing testing.js testing.wasm testing.html app

