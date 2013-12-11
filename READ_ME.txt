/***************************************/
Multithreaded Webserver 

Written in : C

/***************************************/

1.1 What is this?
1.2 Version 
1.3 Contents
1.4 Compiling
1.5 Running 
1.6 Request Format
1.7 Output


1.1 What is this?
----------------------------------------

	This is a Multithreaded webserver developed (as Project requiremet for Operating Systesm (CSE 521) ) which serves request for files.  It is a multithreaded 	server serving files under 2 types of scheduling algorithm, namely First Come First Served(FCFS) and Shortest Job First (SJF).It is written in C/C++. It doesnt log all the requests by default but can be enabled by the command -l. Here the request is accepted by queuing thread and put into queue and the. scheduler thread takes the requests and assign to one of the thread in the threadpool.The server runs in deamon mode in the background and serves the files as requested.
For complete list of the commands refer to '-h' option ie. 'myhttpd -h'


1.2 Version
----------------------------------------

This is the version 1.0 developed by us ( Started Sep 28/2012 & ended on Oct 29/2012). 
	
1.3 Contents
----------------------------------------
The program folder contains two code files and a make file. The two code files "queue.h" which implements a FIFO queue of singly linked list structure. "main.cpp" implements all the threading and other modules for serving the file request. The make file (along with the options) compiles the program. Also it contains two image files for example use.

1.4 Compiling
----------------------------------------
he program is compiled in Linux environment using " g++ myhttp.cpp -o myhttp ". This produces a runnable file "myhttpd".
makefile has been provided along with the tar package.untar the contents into a directory and enter 'make' which should create an executable file 'myhttpd'.
	
1.5 Running
-----------------------------------------
	The program is run by typing " ./myhttp [OPTIONS] " where OPTIONS are 
	
	a) -d : Run the program in debug mode.Does not deamonize the program and there is no logging. All the logging is directed to stdout.
	b) -h : Prints a usage summary with all options and exit.
	c) -l file : Log all the requests to the given file. 
	d) -p port : Listen on the given port. Default port is 8080
	e) -r dir  : Set the root directory for the http server to dir.
	f) -t time : Set the queuing time to time seconds.Default time is 60 seconds.
	g) -n threadnum : No of threads in the execution threadpool. Default number of threads is 4.
	h) -s sched: Scheduling policy to given policy. Values possible are "FCFS" or "SJF". Default is "FCFS"
Note: User is expected to enter only the valid inputs in command line arguments. Invalid inputs may result in unspecified behaviour.
	A html file and an image file have been provided with the package.
1.6 Request format
-----------------------------------------

	1) Telnet to the machine and the port on which myhttpd is running and then enter the below command.
			"GET/HEAD file_name HTTP_VERSION"
			example : GET /home/csegrad/OS/Katrina.jpg HTTP/1.0
	2) Through browser " timberlake.cse.buffalo:portno/~<dir>/filename "
			example : timberlake.cse.buffalo:4567/~OS/Katrina.jpg
Note: ~ and the string following upto the first '/' will be root directory relative to which the requests will be served.
			
1.7 Output 
--------------------------------------------
	* HEAD request gives the metadata but not the actual content
	* GET request gives back the actual data requested and after this the connection is terminated.q
	

	
