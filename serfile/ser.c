#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <dirent.h>
#include <pthread.h>
#include <mysql/mysql.h>

int create_sockfd();
void send_dir(char * path,int c);
void recv_dir(char * path,int c);
void del_dir(char * path);
void del_file(char * path);
void* work_thread(void * arg);
struct package
{
	unsigned int msgLen;
	char buff[512];
};

int main()
{
	int sockfd = create_sockfd();
	assert( sockfd != -1 );
	
	while( 1 )
	{
		struct sockaddr_in caddr;
		socklen_t len = sizeof(caddr);
		
		int c = accept(sockfd,(struct sockaddr *)&caddr,&len);
		if( c == -1 )
			continue;
		pthread_t id;
		pthread_create(&id,NULL,work_thread,(void*)c);
	}
	exit(0);	
}

int create_sockfd()
{
	int sockfd = socket(AF_INET,SOCK_STREAM,0);
	if( sockfd == -1 )
		return sockfd;

	struct sockaddr_in saddr;
	memset( &saddr,0,sizeof(saddr) );
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(6000); //ftp 什么端口？
	saddr.sin_addr.s_addr = inet_addr("127.0.0.1");

	int res = bind( sockfd,(struct sockaddr*)&saddr,sizeof(saddr) );
	if( res == -1 )
		return res;

	listen( sockfd,5 );
	return sockfd;
}

void* work_thread(void * arg)
{
	int c = (int)arg;

	int flag = 1;
	char ready_path[100] = "/home/Ricky/WorkSpace/file_transfer/usr/";
	char home_path[100] = "/home/Ricky/WorkSpace/file_transfer/usr/";
	if(access(home_path,F_OK) == -1)
	  assert(mkdir(home_path,0755) != -1);
	while( 1 )
	{
		struct package package_ready;
		memset(&package_ready,0,4 + 512);
		if((recv(c,&package_ready,4,0)) <= 0)
		{
			flag = 0;
			break;
		}
		int ready_length = htonl(package_ready.msgLen);
		if((recv(c,package_ready.buff,ready_length,0)) <= 0)
		{
			flag = 0;
			break;
		}
		
		MYSQL *mpcon = mysql_init((MYSQL*)0);
		MYSQL_RES *mp_res;
		MYSQL_ROW mp_row;
		if(!mysql_real_connect(mpcon,"127.0.0.1","root","ZzZ123628!",NULL,3306,NULL,0))
		{
			printf("sql connection fail\n");
			memset(&package_ready,0,4 + 512);
			strcat(package_ready.buff,"err");
			ready_length = strlen(package_ready.buff);
			package_ready.msgLen = ntohl(ready_length);
			assert((send(c,&package_ready,4 + ready_length,0)) != -1);
			flag = 0;
			break;
		}
		if(mysql_select_db(mpcon,"file_transfer"))
		{
			printf("database connection fail\n");
			memset(&package_ready,0,4 + 512);
			strcat(package_ready.buff,"err");
			ready_length = strlen(package_ready.buff);
			package_ready.msgLen = htonl(ready_length);
			assert((send(c,&package_ready,4 + ready_length,0)) != -1);
			flag = 0;
			break;
		}
		char *ready[3] = {0};
		char *ready_p = NULL;
		char save_ready[512] = {0};
		strcpy(save_ready,package_ready.buff);
		char *ready_s = strtok_r(save_ready,"#",&ready_p);
		int i = 0;
		while(ready_s != NULL)
		{
			ready[i] = ready_s;
			ready_s = strtok_r(NULL,"#",&ready_p);++i;
		}
		if( ready[0] == NULL || ready[1] == NULL || ready[2] == NULL)
		{
			printf("package err\n");
			memset(&package_ready,0,4 + 512);
			strcat(package_ready.buff,"input err");
			ready_length = strlen(package_ready.buff);
			package_ready.msgLen = htonl(ready_length);
			assert((send(c,&package_ready,4 + ready_length,0)) != -1);
			flag = 0;
			break;
		}
		if(strcmp(ready[0],"2") == 0)
		{
			char cmd[100] = "select * from usr where name = '";
			strcat(cmd,ready[1]);strcat(cmd,"';");
			if(mysql_real_query(mpcon,cmd,strlen(cmd)))
			{
				printf("select fail\n");
				memset(&package_ready,0,4 + 512);
				strcat(package_ready.buff,"err");
				ready_length = strlen(package_ready.buff);
				package_ready.msgLen = htonl(ready_length);
				assert((send(c,&package_ready,4 + ready_length,0)) != -1);
				flag = 0;
				break;
			}
			mp_res = mysql_store_result(mpcon);
			mp_row = mysql_fetch_row(mp_res);
			if(NULL != mp_row)
			{
				memset(&package_ready,0,4 + 512);
				strcat(package_ready.buff,"name already exists");
				ready_length = strlen(package_ready.buff);
				package_ready.msgLen = htonl(ready_length);
				assert((send(c,&package_ready,4 + ready_length,0)) != -1);
				continue;
			}

			memset(cmd,0,100);
			strcpy(cmd,"insert into usr values('");
			strcat(cmd,ready[1]);strcat(cmd,"','");
			strcat(cmd,ready[2]);strcat(cmd,"');");
			if(mysql_real_query(mpcon,cmd,strlen(cmd)))
			{
				printf("insert fail\n");
				memset(&package_ready,0,4 + 512);
				strcat(package_ready.buff,"err");
				ready_length = strlen(package_ready.buff);
				package_ready.msgLen = htonl(ready_length);
				assert((send(c,&package_ready,4 + ready_length,0)) != -1);
				flag = 0;
				break;
			}
			memset(&package_ready,0,4 + 512);
			strcat(package_ready.buff,"register ok");
			ready_length = strlen(package_ready.buff);
			package_ready.msgLen = htonl(ready_length);
			assert((send(c,&package_ready,4 + ready_length,0)) != -1);
			continue;
		}
		else if(strcmp(ready[0],"1") == 0)
		{
			char cmd[100] = "select * from usr where name = '";
			strcat(cmd,ready[1]);strcat(cmd,"';");
			if(mysql_real_query(mpcon,cmd,strlen(cmd)))
			{
				printf("select fail\n");
				memset(&package_ready,0,4 + 512);
				strcat(package_ready.buff,"err");
				ready_length = strlen(package_ready.buff);
				package_ready.msgLen = htonl(ready_length);
				assert((send(c,&package_ready,4 + ready_length,0)) != -1);
				flag = 0;
				break;
			}
			mp_res = mysql_store_result(mpcon);
			mp_row = mysql_fetch_row(mp_res);
			if(NULL == mp_row || strcmp(mp_row[1],ready[2]) != 0)
			{
				memset(&package_ready,0,4 + 512);
				strcat(package_ready.buff,"user or password err");
				ready_length = strlen(package_ready.buff);
				package_ready.msgLen = htonl(ready_length);
				assert((send(c,&package_ready,4 + ready_length,0)) != -1);
				continue;
			}
			memset(&package_ready,0,4 + 512);
			strcat(package_ready.buff,"ok");
			ready_length = strlen(package_ready.buff);
			package_ready.msgLen = htonl(ready_length);
			assert((send(c,&package_ready,4 + ready_length,0)) != -1);
			strcat(ready_path,ready[1]);
			strcat(home_path,ready[1]);strcat(home_path,"/");
			if(access(ready_path,F_OK) == -1)
			  assert(mkdir(ready_path,0775) != -1);
			strcat(ready_path,"/");
			break;
		}
	}

	while(flag)
	{
		struct package package_order;
		memset(&package_order,0,4 + 512);
		if((recv(c,&package_order,4,0)) <= 0)
			break;
		int length = ntohl(package_order.msgLen);
		if((recv(c,package_order.buff,length,0)) <= 0)
		  break;
		printf("recv:%s\n",package_order.buff);
		
		char *p = NULL;
		char *s = strtok_r(package_order.buff," ",&p);
		char *myargv[10] = {0};int i = 0;
		while(s != NULL)
		{
			myargv[i] = s;
			s = strtok_r(NULL," ",&p);++i;
		}
		
		if(strcmp(myargv[0],"cd") == 0)
		{
			struct package package_msg;
			memset(&package_msg,0,4 + 512);
			if(strcmp(myargv[1],"..") == 0)
			{
				if(strcmp(ready_path,home_path) == 0)
				{
					strcat(package_msg.buff,"已处于您的根目录\n");
					length = strlen(package_msg.buff);
					package_msg.msgLen = htonl(length);
					assert(send(c,&package_msg,4 + length,0) != -1);
				}
				else
				{
					int index = strlen(ready_path) - 1;
					assert(index != 0);
					ready_path[index--] = 0;
					while(ready_path[index] != '/' && index != 0)
					{
						ready_path[index--] = 0;
					}
					strcat(package_msg.buff,"#ok#");
					length = strlen(package_msg.buff);
					package_msg.msgLen = htonl(length);
					assert(send(c,&package_msg,4 + length,0) != -1);
				}
			}
			else
			{
				char query_path[100] = {0};
				strcpy(query_path,ready_path);
				strcat(query_path,myargv[1]);
				DIR *dirpo = opendir(query_path);
				if(dirpo != NULL)//如果用户cd一个文件还会出错
				{
					closedir(dirpo);
					strcat(ready_path,myargv[1]);
					strcat(ready_path,"/");
					strcat(package_msg.buff,"#ok#");
					length = strlen(package_msg.buff);
					package_msg.msgLen = htonl(length);
					assert(send(c,&package_msg,4 + length,0) != -1);
				}
				else
				{
					strcat(package_msg.buff,"目录不存在\n");
					length = strlen(package_msg.buff);
					package_msg.msgLen = htonl(length);
					assert(send(c,&package_msg,4 + length,0) != -1);
				}

			}
		}
		else if(strcmp(myargv[0],"download") == 0)
		{
			int n = 1;
			while(myargv[n] != NULL)
			{
				char path[100] = {0};
				strcpy(path,ready_path);
				strcat(path,myargv[n]);
				struct package package_msg;
				memset(&package_msg,0,4 + 512);
				DIR *dir = opendir(path);
				if(NULL != dir)
				{
					closedir(dir);
					strcat(path,"/");
					strcat(package_msg.buff,"this is a dir");
					length = strlen(package_msg.buff);
					package_msg.msgLen = htonl(length);
					assert((send(c,&package_msg,4 + length,0)) != -1);
					send_dir(path,c);
					++n;continue;
				}
				int ffd = open(path,O_RDONLY);
				if(ffd == -1)
				{
					strcat(package_msg.buff,myargv[n]);
					strcat(package_msg.buff,"不存在");
					length = strlen(package_msg.buff);
					package_msg.msgLen = htonl(length);
					assert((send(c,&package_msg,4 + length,0)) != -1);
					++n;continue;
				}
				int file_size = lseek(ffd,0,SEEK_END);
				lseek(ffd,0,SEEK_SET);
				sprintf(package_msg.buff,"%d",file_size);
				length = strlen(package_msg.buff);
				package_msg.msgLen = htonl(length);
				assert((send(c,&package_msg,4 + length,0)) != -1);
				do
				{
					memset(&package_msg,0,4 + 512);
					length = read(ffd,package_msg.buff,512);
					if(length == -1)
					  break;
					package_msg.msgLen = htonl(length);
					if((send(c,&package_msg,4 + length,0)) == -1)
					  break;
				}while(length == 512);
				close(ffd);++n;
			}
			struct package package_msg;
			memset(&package_msg,0,4 + 512);
			strcat(package_msg.buff,"#ok#");
			length = strlen(package_msg.buff);
			package_msg.msgLen = htonl(length);
			assert((send(c,&package_msg,4 + length,0)) != -1);
		}
		else if(strcmp(myargv[0],"upload") == 0)
		{
			int n = 1;
			while(myargv[n] != NULL)
			{
				struct package package_msg;
				memset(&package_msg,0,4 + 512);
				if((recv(c,&package_msg,4,0)) <= 0)
				  break;
				length = ntohl(package_msg.msgLen);
				if((recv(c,&package_msg.buff,length,0)) <= 0)
				  break;

				if(strcmp(package_msg.buff,"no such file") == 0)
				  break;
				else if(strcmp(package_msg.buff,"this is a dir") == 0)
				{
					char path[100] = {0};
					strcpy(path,ready_path);
					strcat(path,myargv[n]);strcat(path,"/");
					recv_dir(path,c);
					++n;continue;
				}
				else
				{
					char save_md5[33] = {0};
					strcpy(save_md5,package_msg.buff);
					char path[100] = {0};
					char save_path[100] = "/home/Ricky/WorkSpace/file_transfer/allfile/";
					if(access(save_path,F_OK) == -1)
					  assert(mkdir(save_path,0755) != -1);
					strcpy(path,ready_path);
					strcat(path,myargv[n]);//strcat(save_path,myargv[n]);
					MYSQL *mpcon = mysql_init((MYSQL*)0);
					MYSQL_RES *mp_res;
					MYSQL_ROW mp_row;
					if(!mysql_real_connect(mpcon,"127.0.0.1","root","ZzZ123628!",NULL,3306,NULL,0))
					{
						printf("sql connection fail\n");
						memset(&package_msg,0,4 + 512);
						strcat(package_msg.buff,"err");
						length = strlen(package_msg.buff);
						package_msg.msgLen = ntohl(length);
						assert((send(c,&package_msg,4 + length,0)) != -1);
						break;
					}
					if(mysql_select_db(mpcon,"file_transfer"))
					{
						printf("database connection fail\n");
						memset(&package_msg,0,4 + 512);
						strcat(package_msg.buff,"err");
						length = strlen(package_msg.buff);
						package_msg.msgLen = htonl(length);
						assert((send(c,&package_msg,4 + length,0)) != -1);
						break;
					}
					char cmd[100] = "select * from md5 where md5 = '";
					strcat(cmd,package_msg.buff);strcat(cmd,"';");
					if(mysql_real_query(mpcon,cmd,strlen(cmd)))
					{
						printf("query fail\n");
						memset(&package_msg,0,4 + 512);
						strcat(package_msg.buff,"err");
						length = strlen(package_msg.buff);
						package_msg.msgLen = htonl(length);
						assert((send(c,&package_msg,4 + length,0)) != -1);
						break;
					}
					mp_res = mysql_store_result(mpcon);
					mp_row = mysql_fetch_row(mp_res);
					if(mp_row != NULL)
					{
						int i = atoi(mp_row[0]);++i;
						char count[10] = {0};
						sprintf(count,"%d",i);
						memset(cmd,0,100);
						strcat(cmd,"update md5 set count = ");
						strcat(cmd,count);
						strcat(cmd," where md5 = '");strcat(cmd,mp_row[1]);
						strcat(cmd,"';");
						if(mysql_real_query(mpcon,cmd,strlen(cmd)))
						{
							printf("update fial\n");
							memset(&package_msg,0,4 + 512);
							strcat(package_msg.buff,"err");
							length = strlen(package_msg.buff);
							package_msg.msgLen = htonl(length);
							assert((send(c,&package_msg,4 + length,0)) != -1);
							break;
						}
						assert(link(mp_row[2],path) != -1);
						memset(&package_msg,0,4 + 512);
						strcat(package_msg.buff,"传输完成");
						length = strlen(package_msg.buff);
						package_msg.msgLen = htonl(length);
						assert((send(c,&package_msg,4 + length,0)) != -1);
						++n;
					}
					else
					{
						memset(&package_msg,0,4 + 512);
						strcat(package_msg.buff,"开始传输");
						length = strlen(package_msg.buff);
						package_msg.msgLen = htonl(length);
						assert((send(c,&package_msg,4 + length,0)) != -1);
						int index = 1;char file_index[10] = {0};
						sprintf(file_index,"%d",index);strcat(save_path,file_index);
						while(access(save_path,F_OK) == 0)
						{
							int i = strlen(save_path) - 1;
							while(save_path[i] != '/' && i != 0)
								save_path[i--]= 0;
							memset(file_index,0,10);++index;
							sprintf(file_index,"%d",index);
							strcat(save_path,file_index);
						}
						int ffd = open(save_path,O_WRONLY | O_CREAT,0600);
						assert(ffd != -1);
						do
						{
							memset(&package_msg,0,4 + 512);
							if((recv(c,&package_msg,4,0)) == -1)
								break;
							length = ntohl(package_msg.msgLen);
							if((recv(c,package_msg.buff,length,0)) == -1)
								break;
							assert(write(ffd,package_msg.buff,length) != -1);
						}while(length == 512);
						struct stat fbuff;
						assert(fstat(ffd,&fbuff) != -1);
						int inode = fbuff.st_ino;
						char file_inode[10] = {0};
						sprintf(file_inode,"%d",inode);
						assert(link(save_path,path) != -1);
						++n;close(ffd);//将该文件md5写入mysql
						MYSQL *mpcon = mysql_init((MYSQL*)0);
						if(!mysql_real_connect(mpcon,"127.0.0.1","root","ZzZ123628!",NULL,3306,NULL,0))
						{
							printf("connect fail\n");
							break;
						}
						if(mysql_select_db(mpcon,"file_transfer"))
						{
							printf("select db fail\n");
							break;
						}
						char cmd[100] = "insert into md5 values(1,'";
						strcat(cmd,save_md5);strcat(cmd,"','");
						strcat(cmd,save_path);strcat(cmd,"',");
						strcat(cmd,file_inode);strcat(cmd,");");
						if(mysql_real_query(mpcon,cmd,strlen(cmd)))
						{
							printf("insert fail\n");
							break;
						}
						memset(cmd,0,100);
					}
				}
			}
			struct package package_msg;
			memset(&package_msg,0,4 + 512);
			strcat(package_msg.buff,"#ok#");
			length = strlen(package_msg.buff);
			package_msg.msgLen = htonl(length);
			assert((send(c,&package_msg,4 + length,0)) != -1);
		}
		else
		{
			int fd[2];
			int res = pipe(fd);
			pid_t pid = fork();
			assert(res != -1 && pid != -1);
		
			if(pid == 0)
			{
				close(fd[0]);
				assert(dup2(fd[1],1) != -1);
				assert(dup2(fd[1],2) != -1);
				assert(chdir(ready_path) != -1);
				char order[128] = {"/home/Ricky/WorkSpace/file_transfer/mybin/"};
				strcat(order,myargv[0]);
				if(strcmp(myargv[0],"rm") == 0)
				{
					if(strcmp(myargv[1],"-rf") == 0)
					{
						int i = 2;
						while(myargv[i] != NULL)
						{
							char del_path[100] = {0};
							strcat(del_path,ready_path);
							strcat(del_path,myargv[i]);
							DIR *dir = opendir(del_path);
							if(NULL != dir)
							{
								strcat(del_path,"/");
								del_dir(del_path);
								closedir(dir);++i;continue;
							}
							else
							{
								del_file(del_path);
								++i;
							}

						}
					}
					else
					{
						int i = 1;
						while(myargv[i] != NULL)
						{
							char del_path[100] = {0};
							strcat(del_path,ready_path);
							strcat(del_path,myargv[i]);
							del_file(del_path);
							++i;
						}
					}
				}
				execv(order,myargv);
				perror("");
				exit(0);
			}
			else
			{
				wait(NULL);
				close(fd[1]);
				struct package package_msg;
				memset(&package_msg,0,4 + 512);
				strcpy(package_msg.buff,"#ok#");
				assert((read(fd[0],package_msg.buff,512)) != -1);
				length = strlen(package_msg.buff);
				package_msg.msgLen = htonl(length);
				if((send(c,&package_msg,4 + length,0)) <= -1)
				{
					close(fd[0]);
					break;
				}
				close(fd[0]);
			}
		}
	}	
	close(c);
	printf("one client over\n");
}

void send_dir(char *path,int c)
{
	DIR *dir = opendir(path);
	struct dirent *safe = (struct dirent *)malloc(sizeof(struct dirent));
	struct dirent *file = NULL;
	struct package package_msg;
	int length = 0;
	memset(&package_msg,0,4 + 512);
	if(NULL != dir)
	{
		while((!readdir_r(dir,safe,&file)) && file != NULL)
		{
			if(file->d_type == 8)
			{
				char file_path[100] = {0};
				strcat(file_path,path);
				strcat(file_path,file->d_name);
				int ffd = open(file_path,O_RDONLY);
				if(-1 == ffd)
				{
					//后续填充
				}
				memset(&package_msg,0,4 + 512);
				strcpy(package_msg.buff,"file");
				int file_size = lseek(ffd,0,SEEK_END);
				lseek(ffd,0,SEEK_SET);
				sprintf(package_msg.buff + 4,"%d",file_size);
				strcat(package_msg.buff,"#");strcat(package_msg.buff,file->d_name);
				length = strlen(package_msg.buff);
				package_msg.msgLen = htonl(length);
				assert((send(c,&package_msg,4 + length,0)) != -1);
				do
				{
					memset(&package_msg,0,4 + 512);
					length = read(ffd,package_msg.buff,512);
					if(length == -1)
					  break;
					package_msg.msgLen = htonl(length);
					if((send(c,&package_msg,4 + length,0)) == -1)
					  break;
				}while(length == 512);
				close(ffd);
			}
			else if(file->d_type == 4 && file->d_name[0] != '.')
			{
				char dir_path[100] = {0};
				memset(&package_msg,0,4 + 512);
				strcat(dir_path,file->d_name);
				strcat(package_msg.buff,dir_path);
				length = strlen(package_msg.buff);
				package_msg.msgLen = htonl(length);
				assert((send(c,&package_msg,4 + length,0)) != -1);
				memset(dir_path,0,100);
				strcat(dir_path,path);
				strcat(dir_path,file->d_name);strcat(dir_path,"/");
				send_dir(dir_path,c);
			}
		}
		closedir(dir);
		memset(&package_msg,0,4 + 512);
		strcat(package_msg.buff,"end");
		length = strlen(package_msg.buff);
		package_msg.msgLen = htonl(length);
		assert((send(c,&package_msg,4 + length,0)) != -1);
	}
	else
	  printf("open dir [%s] failed.\n",path);
	free(safe);
}

void recv_dir(char * path,int c)
{
	struct package package_msg;
	if(access(path,F_OK) == -1)
	  assert((mkdir(path,0755)) != -1);
	while(1)
	{
		memset(&package_msg,0,4 + 512);
		if((recv(c,&package_msg,4,0)) <= 0)
		  return;
		int length = ntohl(package_msg.msgLen);
		if((recv(c,package_msg.buff,length,0)) <= 0)
		  return;

		if(strncmp(package_msg.buff,"file",4) == 0)
		{
			char temp[100] = {0};
			strcat(temp,package_msg.buff);
			char *safe = NULL;
			char *file_name,*md5;
			file_name = strtok_r(temp,"#",&safe);
			file_name = strtok_r(NULL,"#",&safe);
			md5 = strtok_r(NULL,"#",&safe);
			char file_path[100] = {0};
			strcat(file_path,path);
			strcat(file_path,file_name);
			MYSQL *mpcon = mysql_init((MYSQL*)0);
			MYSQL_RES *mp_res;
			MYSQL_ROW mp_row;
			if(!mysql_real_connect(mpcon,"127.0.0.1","root","ZzZ123628!",NULL,3306,NULL,0))
			{
				printf("sql connect failed\n");
				memset(&package_msg,0,4 + 512);
				strcat(package_msg.buff,"err");
				length = strlen(package_msg.buff);
				package_msg.msgLen = ntohl(length);
				assert((send(c,&package_msg,4 + length,0)) !=  -1);
				return;
			}
			if(mysql_select_db(mpcon,"file_transfer"))
			{
				printf("database connection failed\n");
				memset(&package_msg,0,4 + 512);
				strcat(package_msg.buff,"err");
				length = strlen(package_msg.buff);
				package_msg.msgLen = ntohl(length);
				assert((send(c,&package_msg,4 + length,0)) !=  -1);
				return;
			}
			char cmd[150] = "select * from md5 where md5 = '";
			strcat(cmd,md5);strcat(cmd,"';");
			if(mysql_real_query(mpcon,cmd,strlen(cmd)))
			{
				printf("query failed\n");
				memset(&package_msg,0,4 + 512);
				strcat(package_msg.buff,"err");
				length = strlen(package_msg.buff);
				package_msg.msgLen = ntohl(length);
				assert((send(c,&package_msg,4 + length,0)) !=  -1);
				return;
			}
			mp_res = mysql_store_result(mpcon);
			mp_row = mysql_fetch_row(mp_res);
			if(mp_row != NULL)
			{
				int i = atoi(mp_row[0]);++i;
				char count[10] = {0};
				sprintf(count,"%d",i);
				memset(cmd,0,150);
				strcat(cmd,"update md5 set count = ");
				strcat(cmd,count);
				strcat(cmd," where md5 = '");strcat(cmd,mp_row[1]);
				strcat(cmd,"';");
				if(mysql_real_query(mpcon,cmd,strlen(cmd)))
				{
					printf("update failed\n");
					memset(&package_msg,0,4 + 512);
					strcat(package_msg.buff,"err");
					length = strlen(package_msg.buff);
					package_msg.msgLen = ntohl(length);
					assert((send(c,&package_msg,4 + length,0)) !=  -1);
				}
				assert(link(mp_row[2],file_path) != -1);
				memset(&package_msg,0,4 + 512);
				strcat(package_msg.buff,"传输完成");
				length = strlen(package_msg.buff);
				package_msg.msgLen = ntohl(length);
				assert((send(c,&package_msg,4 + length,0)) !=  -1);
				return;
			}
			else
			{
				memset(&package_msg,0,4 + 512);
				strcat(package_msg.buff,"开始传输");
				length = strlen(package_msg.buff);
				package_msg.msgLen = htonl(length);
				assert((send(c,&package_msg,4 + length,0)) != -1);
				int index = 1;char file_index[10] = {0};
				char save_path[100] = "/home/Ricky/WorkSpace/file_transfer/allfile/";
				sprintf(file_index,"%d",index);strcat(save_path,file_index);
				while(access(save_path,F_OK) == 0)
				{
					int i = strlen(save_path) - 1;
					while(save_path[i] != '/' && i != 0)
					  save_path[i--] = 0;
					memset(file_index,0,100);++index;
					sprintf(file_index,"%d",index);
					strcat(save_path,file_index);
				}
				int ffd = open(save_path,O_WRONLY | O_CREAT,0600);
				assert(ffd != -1);
				do
				{
					memset(&package_msg,0,4 + 512);
					if(recv(c,&package_msg,4,0) == -1)
					  return;
					length = ntohl(package_msg.msgLen);
					assert(length != -1);
					if(recv(c,package_msg.buff,length,0) == -1)
					  return;
					assert(write(ffd,package_msg.buff,length) != -1);
				}while(length == 512);
				struct stat fbuff;
				assert(fstat(ffd,&fbuff) != -1);
				int inode = fbuff.st_ino;
				char file_inode[10] = {0};
				sprintf(file_inode,"%d",inode);
				char cmd[150] = {0};
				memset(cmd,0,150);
				strcat(cmd,"insert into md5 values(1,'");
				strcat(cmd,md5);strcat(cmd,"','");
				strcat(cmd,save_path);strcat(cmd,"',");
				strcat(cmd,file_inode);strcat(cmd,");");
				if(mysql_real_query(mpcon,cmd,strlen(cmd)))
					printf("insert failed\n");
				assert(link(save_path,file_path) != -1);
				close(ffd);
			}
		}
		else if(strcmp(package_msg.buff,"end") == 0)
		  return;
		else
		{
			char dir_path[100] = {0};
			strcat(dir_path,path);
			strcat(dir_path,package_msg.buff);strcat(dir_path,"/");
			recv_dir(dir_path,c);
		}
	}
}

void del_file(char * path)
{
	MYSQL *mpcon = mysql_init((MYSQL*)0);
	MYSQL_RES *mp_res;
	MYSQL_ROW mp_row;
	if(!mysql_real_connect(mpcon,"127.0.0.1","root","ZzZ123628!",NULL,3306,NULL,0))
	{
		printf("connect failed\n");
		return;
	}
	if(mysql_select_db(mpcon,"file_transfer"))
	{
		printf("selecr db failed\n");
		return;
	}
	int ffd = open(path,O_RDONLY);
	if(ffd == -1)
	{
		printf("open failed\n");
		return;
	}
	struct stat fbuff;
	assert(fstat(ffd,&fbuff) != -1);
	int inode = fbuff.st_ino;
	char file_inode[10] = {0};
	sprintf(file_inode,"%d",inode);
	char cmd[150] = "select * from md5 where inode = ";
	strcat(cmd,file_inode);strcat(cmd,";");
	if(mysql_real_query(mpcon,cmd,strlen(cmd)))
	{
		printf("select failed\n");
		return;
	}
	mp_res = mysql_store_result(mpcon);
	mp_row = mysql_fetch_row(mp_res);
	if(mp_row == NULL)
	  return;
	if(strcmp(mp_row[0],"1") == 0)
	{
		char del_path[100] = {0};
		strcat(del_path,mp_row[2]);
		assert(unlink(del_path) != -1);
		memset(cmd,0,150);
		strcat(cmd,"delete from md5 where inode = ");
		strcat(cmd,file_inode);strcat(cmd,";");
		if(mysql_real_query(mpcon,cmd,strlen(cmd)))
		{
			printf("delete fialed\n");
			return;
		}

	}
	else
	{
		int n = atoi(mp_row[0]);--n;
		char count[10] = {0};
		sprintf(count,"%d",n);
		memset(cmd,0,150);
		strcat(cmd,"update md5 set count = ");
		strcat(cmd,count);strcat(cmd," where inode = ");
		strcat(cmd,file_inode);strcat(cmd,";");
		if(mysql_real_query(mpcon,cmd,strlen(cmd)))
		{
			printf("undate failed\n");
			return;
		}
		close(ffd);
	}
}

void del_dir(char * path)
{
	DIR *dir = opendir(path);
	struct dirent *safe = (struct dirent *)malloc(sizeof(struct dirent));
	struct dirent *file = NULL;
	if(NULL != dir)
	{
		while(!readdir_r(dir,safe,&file) && file != NULL)
		{
			if(file->d_type == 8)
			{
				char file_path[100] = {0};
				strcat(file_path,path);
				strcat(file_path,file->d_name);
				del_file(file_path);
			}
			else if(file->d_type == 4 && file->d_name[0] != '.')
			{
				char dir_path[100] = {0};
				strcat(dir_path,path);
				strcat(dir_path,file->d_name);
				strcat(dir_path,"/");
				del_dir(dir_path);
			}
		}
	}
	free(safe);closedir(dir);
}
