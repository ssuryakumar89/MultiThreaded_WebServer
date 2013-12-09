default: myhttpd
myhttpd: myhttpd.cpp
	g++ -pthread myhttpd.cpp -o myhttpd
clean:
	$(RM) myhttpd
