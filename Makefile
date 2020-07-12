#all:
#	g++ read_send_test.cpp ngx_mem_pool.cpp -o t1 -lpthread -std=c++11 -ltcmalloc #-g
#clean:
#	rm app

all:
	g++ read_send_test.cpp ngx_c_crc32.cpp -o t4 -lpthread -std=c++11 -ltcmalloc -g
clean:
	rm app

#all:
#	g++ read_send_test.cpp fallocator.cpp -o t4 -std=c++11 -lpthread -ltcmalloc
#clean:
#	rm app
