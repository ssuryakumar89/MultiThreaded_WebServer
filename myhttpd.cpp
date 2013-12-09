#include <cstdlib>
#include <exception>
#include <iostream>
#include "queue.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fstream>
#include <pthread.h>
#include <time.h>
#include <sstream>
#include <iomanip>
#include <string>
#include <semaphore.h>
#include <string.h>
#include <dirent.h>
bool DEBUG_MODE = false;
bool LOGGING_ENABLED = false;
bool REQUEST_SERVED = true;
pthread_mutex_t exec_queue = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;
int soctype=SOCK_STREAM;
int s;
int sleept=60;
char *host = NULL;
int port=8080;
fd_set master;
#define THREAD_NUM 4
#define FCFS 0
#define SJF 1
struct requests{
	char request_type[10];
	char path[200];
	char http_ver[15];
	int sockfd;
	bool path_found;
	bool is_dir;
	std::string log;
};
queue<requests> tqueue;
queue<requests> q;
void* setup_server(void *);
void* execution_thread(void *);
void handlerq(int, std::string);
void* scheduler(void *);
char * get_timestamp();
void display_help();
int sched=FCFS;
char root[200];
char logfile[200];
int main(int argc, char* argv[])
{
	getcwd(root,sizeof root);
	strcpy(logfile,root);
	strcat(logfile,"log.txt");
	int rlength=strlen(root);
	int stcount;
	for(stcount=rlength-1;stcount>=0;stcount--)
	{
		if(root[stcount]=='/')
		{
			root[stcount+1]='\0';
			break;
		}
	}
	int threadnum=THREAD_NUM;
	for (int i = 1; i < argc; i++)
	{
		if (strncmp(argv[i],"-d",2)==0)
		{
			DEBUG_MODE = true;
		}
		else if (strncmp(argv[i],"-h",2)==0)
		{
			display_help();
			exit(0);
		}
		else if (strncmp(argv[i],"-l",2)==0)
		{
			LOGGING_ENABLED = true;
			strcpy(logfile,argv[i+1]);
			i++;
		}
		else if (strncmp(argv[i],"-p",2)==0)
		{
			port =atoi(argv[i+1]);
			i++;
		}
		else if (strncmp(argv[i],"-r",2)==0)
		{
			int plength=strlen(argv[i+1]);
			if(argv[i+1][plength-1]!= '/')
			{
				strcpy(root,argv[i+1]);
				i++;
			}
			else
			{
				strcpy(root,argv[i+1]);
				int rtlen=strlen(root);
				root[rtlen-1]='\0';
				i++;
			}
			int rlength=strlen(root);
			for(int i=rlength-1;i>=0;i--)
			{
				if(root[i]=='/')
				{
					root[i+1]='\0';
					break;
				}
			}
		}
		else if (strncmp(argv[i],"-t",2)==0)
		{
			sleept = atoi(argv[i+1]);
			i++;
		}
		else if (strncmp(argv[i],"-n",2)==0)
		{
			threadnum = atoi(argv[i+1]);
			i++;
		}
		else if (strncmp(argv[i],"-s",2)==0)
		{
			if(strncmp(argv[i+1],"SJF",3)==0)
			{
				sched = SJF;
				i++;
			}
			else i++;
		}
	}
	if(!DEBUG_MODE)
	{
		signal(SIGCHLD, SIG_IGN);
		pid_t pid,sid;
		if((pid=fork())>0)
		{
			std::cout<<"Process ID: "<<pid<<std::endl;
			exit(0);
		}
		sid=setsid();
		signal (SIGHUP, SIG_IGN);
	}
	pthread_t t1,t2;
	pthread_t threads[threadnum];
	pthread_attr_t attr;
	if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		exit(1);
	}
	pthread_attr_init(&attr);
	pthread_create(&t1,NULL,setup_server,NULL);
	pthread_create(&t2,NULL,scheduler,NULL);
	for (int i=0;i<threadnum;i++)
	{
		pthread_create(&threads[i],NULL,execution_thread,NULL);
	}
	pthread_join(t1,NULL);
	pthread_join(t2,NULL);
	for (int i=0;i<threadnum;i++)
	{
		pthread_join(threads[1],NULL);
	}

}

void display_help()
{
	std::cout<<"Usage: myhttpd [-d] [-h] [-l file] [-p port] [-r dir] [-t time] [-n threadnum] [-s shed]"<<std::endl;
	std::cout<<"All arguments are optional"<<std::endl;
	std::cout<<"	-d: Enter debugging mode. myhttpd will not be daemonized, it will accept one connection at a time and logging to stdout will be enabled"<<std::endl;
	std::cout<<"	-h: Usage summary"<<std::endl;
	std::cout<<"	-l: Log all requests to the file specified by file "<<std::endl;
	std::cout<<"	-p: Listen on the given port. Default is 8080"<<std::endl;
	std::cout<<"	-r: Set the root directory to dir"<<std::endl;
	std::cout<<"	-t: Set the queuing time to 'time' seconds. Default is 60 seconds"<<std::endl;
	std::cout<<"	-n: Set the number of execution thread to threadnum. Default is 4"<<std::endl;
	std::cout<<"	-s: Scheduling policy, either FCFS or SJF. Default is FCFS"<<std::endl;
}

int get_fsize(char *fname)
{
	struct stat status;
	FILE *finfo;
	pthread_mutex_lock(&exec_queue);
	finfo= fopen(fname,"r");
	if(finfo==NULL)
	{
		pthread_mutex_unlock(&exec_queue);
		return -1;
	}
	else{
		fstat(fileno(finfo),&status);
		fclose(finfo);
		pthread_mutex_unlock(&exec_queue);
		return status.st_size;
	}
}

void* setup_server(void* t)
{
	struct sockaddr_in serv_addr, remote;
	int newsock;
	socklen_t sine_size;
	int fdmax;
	fd_set temp_sock;
	if ( (s = socket(AF_INET, SOCK_STREAM, 0)) < 0 )
	{
		std::cerr<<"Could not create a listening socket"<<std::endl;
		exit(0);
	}
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family      = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port        = htons(port);
	if ( bind(s, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0 )
	{
		std::cerr<<"Could not bind to the listening socket"<<std::endl;
		exit(0);
	}
	//* 'Logic' of the following part of the code belongs to Beej's guide to network programming   *//
	//* URL: http://beej.us/guide/bgnet/output/html/multipage/advanced.html#select              *//
	//* It starts here             *//
	FD_ZERO(&master);
	listen(s, 5);
	FD_SET(s,&master);
	fdmax=s;
	newsock = s;
	std::string log;
	while (1)
	{
		temp_sock=master;
		if (select(fdmax+1, &temp_sock, NULL, NULL, NULL) == -1) {
			perror("select");
			exit(4);
		}
		for(int i=0; i<=fdmax;i++)
		{
			if (FD_ISSET(i, &temp_sock))
			{
				if (i == s)
				{
					sine_size=sizeof remote;
					newsock = accept(s, (struct sockaddr *) &remote, &sine_size);
					if (newsock==-1)
						continue;
					else
					{
						FD_SET(newsock, &master);
						if (newsock> fdmax)
						{
							fdmax = newsock;
						}
					}
				}
				else
					//* 			It ends here  					*//
				{
					log=inet_ntoa(remote.sin_addr);
					handlerq(i,log);
					FD_CLR(i,&master); //after handling the request don't bother about keeping it in master set
					if(DEBUG_MODE)
					{
						REQUEST_SERVED=false;
						while(REQUEST_SERVED)
						{
							;
						}
					}
				}
			}
		}
	}
	return NULL;
}

void handlerq(int n, std::string log){
	char msg[1000], *headers[3];
	char c;
	int rcvd,fsize;
	int i=0;
	int flag=0;
	struct requests req;
	do
	{
		rcvd=read(n, &c, 1);
		msg[i]=c;
		if(msg[i]== '\n'&& msg[i-1] == '\r')
		{
			flag++;
		}
		else if(msg[i]!='\r')flag=0;
		if(i==2)
		{
			if((msg[0]!='G')&&(msg[1]!='E')&&(msg[2]!='T'))
			{
				msg[2]='\0';
				break;
			}
		}
		else if(i==3)
		{
			if((msg[0]!='H')&&(msg[1]!='E')&&(msg[2]!='A')&&(msg[3]!='D'))
			{
				msg[3]='\0';
				break;
			}
		}
		if (flag==2)
		{
			msg[i+1]='\0';
			break;
		}
		i++;
	}while(1);
	if (rcvd<0)
	{
		std::cerr<<"some error in recv()"<<std::endl;
		close(n);
	}
	else if (rcvd==0)
	{
		std::cerr<<"Client disconnected unexpectedly"<<std::endl;
	}
	else
	{
		log.append(" - [");
		log.append(get_timestamp());
		log.erase(log.length()-1);
		req.log=log;
		headers[0] = strtok (msg, " \t\n");
		headers[1] = strtok (NULL, " \t");
		headers[2] = strtok (NULL, " \t\n");
		if ((headers[0]==NULL)||(headers[1]==NULL)||(headers[2]==NULL))
		{
			shutdown (n, SHUT_RDWR);
			close(n);
		}
		else
		{
			int hlength=0;
			char fullpath[200];
			strcpy(fullpath,root);
			if ( strncmp(headers[0], "GET\0", 4)==0 )
			{
				req.is_dir=false;
				req.path_found=true;
				if(headers[1][1]=='~')
				{
					headers[1]+=2;
					strcat(fullpath,headers[1]);
				}
				else
				{
					strcpy(fullpath,headers[1]);
				}
				hlength=strlen(fullpath);
				if(fullpath[hlength-1]=='/')
				{
					req.is_dir=true;

				}
				else
				{
					if((fsize=get_fsize(fullpath))==-1)
					{
						req.path_found=false;
					}
				}
				strcpy(req.request_type,headers[0]);
				strcpy(req.path,fullpath);
				req.sockfd=n;
				strcpy(req.http_ver,headers[2]);
				q.add(req,fsize);
			}
			else if ( strncmp(headers[0], "HEAD\0", 4)==0)
			{
				req.is_dir=false;
				req.path_found=true;
				if(headers[1][1]=='~')
				{
					headers[1]+=2;
					strcat(fullpath,headers[1]);
				}
				else
				{
					strcpy(fullpath,headers[1]);
				}
				hlength=strlen(fullpath);
				if(fullpath[hlength-1]=='/')
				{
					req.is_dir=true;

				}
				else
				{


					if((fsize=get_fsize(fullpath))==-1)
					{
						req.path_found=false;
					}
				}
				strcpy(req.request_type,headers[0]);
				strcpy(req.path,fullpath);
				req.sockfd=n;
				strcpy(req.http_ver,headers[2]);
				q.add(req,0);
			}
			else
			{
				shutdown (n, SHUT_RDWR);
				close(n);
			}
		}
	}
}


void* scheduler(void *t)
{
	if(!DEBUG_MODE)
	{
		sleep(sleept);
	}
	struct requests r;
	int size=0;
	if (sched==FCFS)
	{
		while(1)
		{
			if(!q.empty())
			{
				q.get(r,size);
				pthread_mutex_lock(&exec_queue);
				tqueue.add(r,size);
				pthread_mutex_unlock(&exec_queue);
			}
		}
	}
	else
	{
		while(1)
		{
			if(!q.empty())
			{
				q.getshrt(r,size);
				pthread_mutex_lock(&exec_queue);
				tqueue.add(r,size);
				pthread_mutex_unlock(&exec_queue);
			}
		}
	}
	return NULL;
}

char * get_timestamp()
{
	char *time_now=new char();
	time_t mytime;
	struct tm *mtime;
	time (&mytime);
	mtime=gmtime(&mytime);
	time_now=asctime(mtime);
	return time_now;

}
char * get_lastm(char *fname)
{
	struct stat f;
	struct tm *mtime;
	char *mod_time=new char();
	if(!stat(fname,&f))
	{
		mtime=gmtime(&f.st_mtime);
		mod_time=asctime(mtime);
	}
	return mod_time;
}
char * filetype(char *fname)
{
	char *ftype;
	ftype= strtok(fname,".");
	ftype= strtok(NULL,".");
	return ftype;

}
void logger(std::string log)
{
	if(!DEBUG_MODE)
	{
		if(LOGGING_ENABLED)
		{
			pthread_mutex_lock(&log_mutex);
			FILE *f;
			f=fopen(logfile,"a");
			fprintf(f,"%s",log.data());
			fclose(f);
			pthread_mutex_unlock(&log_mutex);
		}
	}
	else{
		std::cout<<log;
	}
}
void* execution_thread(void* d)
{
	struct requests r;
	int k;
	bool flag=false;
	std::string log;
	while(1)
	{
		pthread_mutex_lock(&exec_queue);
		if (!tqueue.empty())
		{

			tqueue.get(r,k);
			flag = true;
		}
		pthread_mutex_unlock(&exec_queue);
		if(flag)
		{
			log=r.log;
			//std::cout<<r.path<<std::endl;
			int ifsize=get_fsize(r.path);
			char fsize[4];
			sprintf(fsize,"%d",ifsize);
			char filept[100];
			strcpy(filept,r.path);
			if ( strncmp(r.http_ver, "HTTP/1.0", 8)!=0 && strncmp(r.http_ver, "HTTP/1.1", 8)!=0 )
			{
				write(r.sockfd, "HTTP/1.0 400 Bad Request\r\n", 26);
				write (r.sockfd, "\r\n",2);
				char *tstamp=get_timestamp();
				log.append("] [");
				log.append(tstamp);
				write(r.sockfd,tstamp,(((std::string)tstamp).length()-1));
				write (r.sockfd, "\r\n\r\n",4);
				shutdown (r.sockfd, SHUT_RDWR);
				close(r.sockfd);
				log.erase(log.length()-1);
				log.append("] \"");
				log.append(r.request_type);
				log.append(" ");
				log.append(r.path);
				log.append(" ");
				log.append(r.http_ver);
				log.erase(log.length()-1);
				log.append("\" 400 -1\n");
			}
			else
			{
				if(!r.is_dir)
				{
					if(!r.path_found)
					{
						write (r.sockfd, "HTTP/1.0 404 Not Found\r\n", 24);
						write(r.sockfd, "Date: ",6);
						char *tstamp=get_timestamp();
						log.append("] [");
						log.append(tstamp);
						write(r.sockfd,tstamp,(((std::string)tstamp).length()-1));
						write(r.sockfd,"\r\n",2);
						write(r.sockfd, "Server: ",8);
						write(r.sockfd, "Myhttpd/1.0\r\n",13);
						write(r.sockfd, "Content-Type: text/html\r\n",25);
						write (r.sockfd, "\r\n",2);
						shutdown (r.sockfd, SHUT_RDWR);
						close(r.sockfd);
						log.erase(log.length()-1);
						log.append("] \"");
						log.append(r.request_type);
						log.append(" ");
						log.append(r.path);
						log.append(" ");
						log.append(r.http_ver);
						log.erase(log.length()-1);
						log.append("\" 404 -1\n");
					}
					else
					{
						char *ftype=filetype(filept);
						if( strncmp(r.request_type,"GET\0",4)==0)
						{
							FILE *f;
							char *content=new char[ifsize];
							int bytes=0;
							write(r.sockfd, "HTTP/1.0 200 OK\r\n", 17);
							write(r.sockfd, "Date: ",6);
							char *tstamp=get_timestamp();
							log.append("] [");
							log.append(tstamp);
							write(r.sockfd,tstamp,(((std::string)tstamp).length()-1));
							write(r.sockfd,"\r\n",2);
							write(r.sockfd, "Server: ",8);
							write(r.sockfd, "Myhttpd/1.0\r\n",13);
							write(r.sockfd, "Last-Modified: ",15);
							write(r.sockfd,get_lastm(r.path),(((std::string)get_lastm(r.path)).length()-1));
							write(r.sockfd,"\r\n",2);
							write(r.sockfd,"Content-Type: ",14);
							if((ftype==NULL))
							{
								write(r.sockfd,"text/html\r\n",11);
							}
							else
							{
								if((strncmp(ftype,"html",4)==0)||(strncmp(ftype,"txt",3)==0))

								{
									write(r.sockfd,"text/html\r\n",11);
								}
								if ((strncmp(ftype,"gif",3)==0)||(strncmp(ftype,"jpg",3)==0))
								{
									write(r.sockfd,"image/gif\r\n",11);
								}
							}
							write(r.sockfd, "Content-Length: ",16);
							write(r.sockfd,(char *)fsize,strlen(fsize));
							write(r.sockfd,"\r\n\r\n",4);
							f=fopen(r.path,"rb");
							while((bytes=fread(content,sizeof(char),ifsize,f))>0)
							{
								write(r.sockfd,content,bytes);
							}
							fclose(f);
							delete content;
							write(r.sockfd,"\r\n",2);
							shutdown (r.sockfd, SHUT_RDWR);
							close(r.sockfd);
							log.erase(log.length()-1);
							log.append("] \"");
							log.append(r.request_type);
							log.append(" ");
							log.append(r.path);
							log.append(" ");
							log.append(r.http_ver);
							log.erase(log.length()-1);
							log.append("\" 200 ");
							log.append(fsize);
							log.append("\n");
						}
						else
						{
							write(r.sockfd, "HTTP/1.0 200 OK\r\n", 17);
							write(r.sockfd, "Date: ",6);
							char *tstamp=get_timestamp();
							log.append("] [");
							log.append(tstamp);
							write(r.sockfd,tstamp,(((std::string)tstamp).length()-1));
							write(r.sockfd,"\r\n",2);
							write(r.sockfd, "Server: ",8);
							write(r.sockfd, "Myhttpd/1.0\r\n",13);
							write(r.sockfd, "Last-Modified: ",15);
							write(r.sockfd,get_lastm(r.path),(((std::string)get_lastm(r.path)).length()-1));
							write(r.sockfd,"\r\n",2);
							write(r.sockfd,"Content-Type: ",14);
							if((ftype==NULL))
							{
								write(r.sockfd,"text/html\r\n",11);
							}
							else
							{
								if((strncmp(ftype,"html",4)==0)||(strncmp(ftype,"txt",3)==0))

								{
									write(r.sockfd,"text/html\r\n",11);
								}
								if ((strncmp(ftype,"gif",3)==0)||(strncmp(ftype,"jpg",3)==0))
								{
									write(r.sockfd,"image/gif\r\n",11);
								}
							}
							write(r.sockfd, "Content-Length: ",16);
							write(r.sockfd,(char *)fsize,strlen(fsize));
							write(r.sockfd,"\r\n\r\n",4);
							shutdown (r.sockfd, SHUT_RDWR);
							close(r.sockfd);
							log.erase(log.length()-1);
							log.append("] \"");
							log.append(r.request_type);
							log.append(" ");
							log.append(r.path);
							log.append(" ");
							log.append(r.http_ver);
							log.erase(log.length()-1);
							log.append("\" 200 ");
							log.append(fsize);
							log.append("\n");
						}
					}

				}
				else
				{
					DIR *directory=opendir(r.path);
					if(directory==NULL)
					{
						write (r.sockfd, "HTTP/1.0 404 Not Found\r\n", 24);
						write(r.sockfd, "Date: ",6);
						char *tstamp=get_timestamp();
						log.append("] [");
						log.append(tstamp);
						write(r.sockfd,tstamp,(((std::string)tstamp).length()-1));
						write(r.sockfd,"\r\n",2);
						write(r.sockfd, "Server: ",8);
						write(r.sockfd, "Myhttpd/1.0\r\n",13);
						write(r.sockfd, "Content-Type: text/html\r\n",25);
						write (r.sockfd, "\r\n",2);
						shutdown (r.sockfd, SHUT_RDWR);
						close(r.sockfd);
						log.erase(log.length()-1);
						log.append("] \"");
						log.append(r.request_type);
						log.append(" ");
						log.append(r.path);
						log.append(" ");
						log.append(r.http_ver);
						log.erase(log.length()-1);
						log.append("\" 404 -1\n");
					}
					else
					{
						strcat(filept,"index.html");
						int f2size;
						char dir_size[4];
						if( strncmp(r.request_type,"GET\0",4)==0)
						{
							if((f2size=get_fsize(filept))==-1)
							{
								std::string files_here;
								struct dirent *dir_ent=NULL;
								int i=0;
								dir_ent=readdir(directory);
								while(dir_ent!=NULL)
								{
									if(dir_ent->d_name[0]!='.')
									{
										files_here.append(dir_ent->d_name);
										files_here.append("\n");
									}
									i++;
									dir_ent=readdir(directory);
								}
								write(r.sockfd, "HTTP/1.0 200 OK\r\n", 17);
								write(r.sockfd, "Date: ",6);
								char *tstamp=get_timestamp();
								log.append("] [");
								log.append(tstamp);
								write(r.sockfd,tstamp,(((std::string)tstamp).length()-1));
								write(r.sockfd,"\r\n",2);
								write(r.sockfd, "Server: ",8);
								write(r.sockfd, "Myhttpd/1.0\r\n",13);
								write(r.sockfd, "Last-Modified: ",15);
								write(r.sockfd,get_lastm(filept),(((std::string)get_lastm(filept)).length()-1));
								write(r.sockfd,"\r\n",2);
								write(r.sockfd,"Content-Type: text/html\r\n",25);
								write(r.sockfd, "Content-Length: ",16);
								sprintf(dir_size,"%d",files_here.length());
								write(r.sockfd,(char *)dir_size,strlen(dir_size));
								write(r.sockfd,"\r\n\r\n",4);
								write(r.sockfd,files_here.data(),files_here.length());
								write(r.sockfd,"\r\n",2);
								shutdown (r.sockfd, SHUT_RDWR);
								close(r.sockfd);
								log.erase(log.length()-1);
								log.append("] \"");
								log.append(r.request_type);
								log.append(" ");
								log.append(r.path);
								log.append(" ");
								log.append(r.http_ver);
								log.erase(log.length()-1);
								log.append("\" 200 ");
								log.append(dir_size);
								log.append("\n");
							}
							else
							{
								FILE *f;
								int bytes;
								char *content=new char[f2size];
								write(r.sockfd, "HTTP/1.0 200 OK\r\n", 17);
								write(r.sockfd, "Date: ",6);
								char *tstamp=get_timestamp();
								log.append("] [");
								log.append(tstamp);
								write(r.sockfd,tstamp,(((std::string)tstamp).length()-1));
								write(r.sockfd,"\r\n",2);
								write(r.sockfd, "Server: ",8);
								write(r.sockfd, "Myhttpd/1.0\r\n",13);
								write(r.sockfd, "Last-Modified: ",15);
								write(r.sockfd,get_lastm(filept),(((std::string)get_lastm(filept)).length()-1));
								write(r.sockfd,"\r\n",2);
								write(r.sockfd,"Content-Type: text/html\r\n",25);
								write(r.sockfd, "Content-Length: ",16);
								sprintf(dir_size,"%d",f2size);
								write(r.sockfd,(char *)dir_size,strlen(dir_size));
								write(r.sockfd,"\r\n\r\n",4);
								f=fopen(filept,"rb");
								while((bytes=fread(content,sizeof(char),ifsize,f))>0)
								{
									write(r.sockfd,content,bytes);
								}
								fclose(f);
								delete content;
								shutdown (r.sockfd, SHUT_RDWR);
								close(r.sockfd);
								log.erase(log.length()-1);
								log.append("] \"");
								log.append(r.request_type);
								log.append(" ");
								log.append(r.path);
								log.append(" ");
								log.append(r.http_ver);
								log.erase(log.length()-1);
								log.append("\" 200 ");
								log.append(dir_size);
								log.append("\n");
							}
						}
						else
						{
							if((f2size=get_fsize(filept))==-1)
							{
								std::string files_here;
								struct dirent *dir_ent=NULL;
								dir_ent=readdir(directory);
								while(dir_ent!=NULL)
								{
									if(dir_ent->d_name[0]!='.')
									{
										files_here.append(dir_ent->d_name);
										files_here.append("\r\n");
									}
									dir_ent=readdir(directory);
								}
								write(r.sockfd, "HTTP/1.0 200 OK\r\n", 17);
								write(r.sockfd, "Date: ",6);
								char *tstamp=get_timestamp();
								log.append("] [");
								log.append(tstamp);
								write(r.sockfd,tstamp,(((std::string)tstamp).length()-1));
								write(r.sockfd,"\r\n",2);
								write(r.sockfd, "Server: ",8);
								write(r.sockfd, "Myhttpd/1.0\r\n",13);
								write(r.sockfd, "Last-Modified: ",15);
								write(r.sockfd,get_lastm(filept),(((std::string)get_lastm(filept)).length()-1));
								write(r.sockfd,"\r\n",2);
								write(r.sockfd,"Content-Type: text/html\r\n",25);
								write(r.sockfd, "Content-Length: ",16);
								sprintf(dir_size,"%d",files_here.length());
								write(r.sockfd,(char *)dir_size,strlen(dir_size));
								write(r.sockfd,"\r\n\r\n",4);
								shutdown (r.sockfd, SHUT_RDWR);
								close(r.sockfd);
								log.erase(log.length()-1);
								log.append("] \"");
								log.append(r.request_type);
								log.append(" ");
								log.append(r.path);
								log.append(" ");
								log.append(r.http_ver);
								log.erase(log.length()-1);
								log.append("\" 200 ");
								log.append(dir_size);
								log.append("\n");
							}
							else
							{
								write(r.sockfd, "HTTP/1.0 200 OK\r\n", 17);
								write(r.sockfd, "Date: ",6);
								char *tstamp=get_timestamp();
								log.append("] [");
								log.append(tstamp);
								write(r.sockfd,tstamp,(((std::string)tstamp).length()-1));
								write(r.sockfd,"\r\n",2);
								write(r.sockfd, "Server: ",8);
								write(r.sockfd, "Myhttpd/1.0\r\n",13);
								write(r.sockfd, "Last-Modified: ",15);
								write(r.sockfd,get_lastm(filept),(((std::string)get_lastm(filept)).length()-1));
								write(r.sockfd,"\r\n",2);
								write(r.sockfd,"Content-Type: text/html\r\n",25);
								write(r.sockfd, "Content-Length: ",16);
								sprintf(dir_size,"%d",f2size);
								write(r.sockfd,(char *)dir_size,strlen(dir_size));
								write(r.sockfd,"\r\n\r\n",4);
								shutdown (r.sockfd, SHUT_RDWR);
								close(r.sockfd);
								log.erase(log.length()-1);
								log.append("] \"");
								log.append(r.request_type);
								log.append(" ");
								log.append(r.path);
								log.append(" ");
								log.append(r.http_ver);
								log.erase(log.length()-1);
								log.append("\" 200 ");
								log.append(dir_size);
								log.append("\n");
							}
						}
					}

				}

			}
			logger(log);
			if(DEBUG_MODE)
			{
				REQUEST_SERVED=true;
			}
		}
		flag=false;
	}
	return NULL;
}
