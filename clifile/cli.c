#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <openssl/md5.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/stat.h>

void calculate_md5(int fd,char md[]);
void recv_dir(char *path,int sockfd);
void send_dir(char *path,int sockfd);
struct package
{
	unsigned int msgLen;
	char buff[512];
};

int main()
{
	int sockfd = socket(AF_INET,SOCK_STREAM,0);
	assert( sockfd != -1);

	struct sockaddr_in saddr;
	memset(&saddr,0,sizeof(saddr));
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(6000);
	saddr.sin_addr.s_addr = inet_addr("127.0.0.1");

	int res = connect(sockfd,(struct sockaddr*)&saddr,sizeof(saddr));
	assert( res != -1);

	while(1)
	{
		char choose = 0;
		printf("使用服务需要用户名和密码，登录输入1，注册输入2，退出输入3：");
		fflush(stdout);
		scanf("%c",&choose);
		if('1' == choose || '2' == choose)
		{
			struct package package_ready;
			memset(&package_ready,0,4 + 512);
			package_ready.buff[0] = choose;
			strcat(package_ready.buff,"#");
			printf("usr name:");fflush(stdout);
			int c = 0;//用于清理缓冲区
			while((c = getchar()) != '\n' && c != EOF);
			scanf("%s",package_ready.buff + 2);
			int ready_length = strlen(package_ready.buff);
			int usr_pwd_flag = 1,index = 2;
			while(package_ready.buff[index] != 0)
			{
				if(!isalnum(package_ready.buff[index++]))
				  usr_pwd_flag = 0;
			}
			package_ready.buff[ready_length] = '#';
			printf("password:");fflush(stdout);fflush(stdin);
			scanf("%s",package_ready.buff + ready_length + 1);
			index = ready_length + 1;
			while(package_ready.buff[index] != 0)
			{
				if(!isalnum(package_ready.buff[index++]))
				  usr_pwd_flag = 0;
			}
			if(0 == usr_pwd_flag)
			{
				printf("用户名或密码含有非法字符，请重新输入\n");
				while((c = getchar()) != '\n' && c != EOF);
				continue;
			}
			ready_length = strlen(package_ready.buff);
			package_ready.msgLen = htonl(ready_length);
			assert((send(sockfd,&package_ready,4 + ready_length,0)) != -1);
			
			memset(&package_ready,0,4 + 512);
			if((recv(sockfd,&package_ready,4,0)) <= 0)
			{
				printf("ser err\n");
				close(sockfd);exit(0);
			}
			ready_length = ntohl(package_ready.msgLen);
			if((recv(sockfd,package_ready.buff,ready_length,0)) <= 0)
			{
				printf("ser err\n");
				close(sockfd);exit(0);
			}
			while((c = getchar()) != '\n' && c != EOF);

			if(strcmp(package_ready.buff,"ok") == 0)
				break;
			else if(strcmp(package_ready.buff,"err") == 0)
			{
				close(sockfd);
				exit(0);
			}
			else if(strcmp(package_ready.buff,"input err") == 0)
			{
				printf("输入不符合格式（请不要包含符号），请重新选择服务\n");
				continue;
			}
			else if(strcmp(package_ready.buff,"name already exists") == 0)
			{
				printf("用户名已存在，请重新选择服务\n");
				continue;
			}
			else if(strcmp(package_ready.buff,"user or password err") == 0)
			{
				printf("用户名或密码错误，请重新选择服务\n");
				continue;
			}
			else
			{
				printf("%s\n",package_ready.buff);
				continue;
			}
		}
		else if('3' == choose)
		{
			printf("bye\n");
			exit(0);
		}
		else
		{
			printf("不合法选项，请重新选择\n");
			int c = 0;
			while((c = getchar()) != '\n' && c != EOF);
			continue;
		}
	}

	while( 1 )
	{
		printf("connect >> ");
		fflush(stdout);setbuf(stdin,NULL);
		struct package package_order;
		memset(&package_order,0,4 + 512);
		fgets(package_order.buff,512,stdin);
		int length = strlen(package_order.buff) - 1;
		package_order.buff[length] = 0;
		package_order.msgLen = htonl(length);
		if(package_order.buff[0] == 0)
		  continue;
	
		char order_buff[512] = {0};
		strcpy(order_buff,package_order.buff);
		char *myargv[10] = {0};
		int i = 0;
		char *s = strtok(order_buff," ");
		while(s != NULL)
		{
			myargv[i++] = s;
			s = strtok(NULL," ");
		}

		if( strcmp(myargv[0],"exit") == 0)
			break;
		else if(strcmp(myargv[0],"show") == 0)
		{
			myargv[0] = "ls";
			pid_t pid = fork();
			if(pid == 0)
			{
				execvp(myargv[0],myargv);
				exit(0);
			}
			wait(NULL);
			continue;
		}
		else if( strcmp(myargv[0],"download") == 0 )
		{
			assert((send(sockfd,&package_order,4 + length,0)) != -1);
			int n = 1;
			while(myargv[n] != NULL)
			{
				struct package package_msg;
				memset(&package_msg,0,4 + 512);
				if((recv(sockfd,&package_msg,4,0)) <= 0)
				{
					printf("ser error\n");
					break;
				}
				length = ntohl(package_msg.msgLen);
				if((recv(sockfd,package_msg.buff,length,0)) <= 0)
				{
					printf("ser error\n");
					break;
				}
				char temp[100] = {0};
				strcat(temp,myargv[n]);	strcat(temp,"不存在");
				if((strcmp(package_msg.buff,temp)) == 0)
				{
					printf("%s不存在\n",myargv[n]);
					++n;continue;
				}
				else if(strcmp(package_msg.buff,"this is a dir") == 0)
				{
					char path[100] = "./down/";
					strcat(path,myargv[n]);strcat(path,"/");
					recv_dir(path,sockfd);
					++n;continue;
				}
				int file_size = 0,curr_size = 0;
				sscanf(package_msg.buff,"%d",&file_size);
				char path[100] = "./down/";
				strcat(path,myargv[n]);
				int ffd = open(path,O_WRONLY | O_CREAT,0600);
				assert(ffd != -1);
				printf("文件:%s 大小:%d\n",myargv[n],file_size);
				do
				{
					memset(&package_msg,0,4 + 512);
					assert((recv(sockfd,&package_msg,4,0)) != -1 );
					length = ntohl(package_msg.msgLen);
					assert((recv(sockfd,package_msg.buff,length,0)) != -1);
					assert(write(ffd,package_msg.buff,length) != -1);
					curr_size +=length;
					float f = curr_size * 100.0 / file_size;
					printf("下载:%.2f%%\r",f);fflush(stdout);
				}while(length == 512);
				printf("\n");
				close(ffd);++n;
			}
		}
		else if(strcmp(myargv[0],"upload") == 0)
		{
			assert((send(sockfd,&package_order,4 + length,0)) != -1);
			int n = 1;
			while(myargv[n] != NULL)
			{
				char path[100] = "./";
				strcat(path,myargv[n]);
				struct package package_msg;
				memset(&package_msg,0,4 + 512);
				DIR *dir = opendir(path);
				if(NULL != dir)
				{
					strcat(path,"/");
					strcat(package_msg.buff,"this is a dir");
					length = strlen(package_msg.buff);
					package_msg.msgLen = htonl(length);
					assert((send(sockfd,&package_msg,4 + length,0)) != -1);
					send_dir(path,sockfd);
					++n;closedir(dir);continue;
				}
				int ffd = open(path,O_RDONLY);
				if(ffd == -1)
				{
					printf("no such file\n");
					strcat(package_msg.buff,"no such file");
					length = strlen(package_msg.buff);
					package_msg.msgLen = htonl(length);
					assert((send(sockfd,&package_msg,4 + length,0)) != -1);
					++n;continue;
				}
				calculate_md5(ffd,package_msg.buff);//计算md5
				length = strlen(package_msg.buff);
				package_msg.msgLen = htonl(length);
				assert((send(sockfd,&package_msg,4 + length,0)) != -1);
				memset(&package_msg,0,4 + 512);
				if(recv(sockfd,&package_msg,4,0) <= 0)
				{
					printf("ser err\n");
					close(ffd);break;
				}
				length = ntohl(package_msg.msgLen);
				if(recv(sockfd,package_msg.buff,length,0) <= 0)
				{
					printf("ser err\n");
					close(ffd);break;
				}
		
				if(strcmp(package_msg.buff,"传输完成") == 0)
				{
					int file_size = 0;
					file_size = lseek(ffd,0,SEEK_END);lseek(ffd,0,SEEK_SET);
					printf("文件:%s 大小:%d 完成秒传\n",path,file_size);
					++n;close(ffd);
					continue;
				}
				else if(strcmp(package_msg.buff,"开始传输") == 0)
				{
					int file_size = 0,curr_size = 0;
					file_size = lseek(ffd,0,SEEK_END);
					lseek(ffd,0,SEEK_SET);
					printf("文件:%s, 大小:%d\n",path,file_size);
					do
					{
						memset(&package_msg,0,4 + 512);
						length = read(ffd,package_msg.buff,512);
						assert(length != -1);
						package_msg.msgLen = htonl(length);
						assert((send(sockfd,&package_msg,4 + length,0)) != -1);
						curr_size += length;
						float f = curr_size * 100.0 / file_size;
						printf("上传:%.2f%%\r",f);fflush(stdout);
					}while(length == 512);
					printf("\n");
					++n;close(ffd);
				}
				else
				{
					printf("something wrong\n");
					close(ffd);break;
				}
			}
		}
		else
			assert((send(sockfd,&package_order,4 + length,0)) != -1);
		struct package package_msg;
		memset(&package_msg,0,4 + 512);
		assert((recv(sockfd,&package_msg,4,0)) != -1);
		length = ntohl(package_msg.msgLen);
		assert((recv(sockfd,package_msg.buff,length,0)) != -1);
		if(strcmp(package_msg.buff,"#ok#") == 0)
		  continue;
		printf("%s",package_msg.buff);
		fflush(stdout);
	}
	close(sockfd);
	exit(0);
}

void calculate_md5(int fd,char md[])
{
	unsigned char tmp[16] = {0};
	MD5_CTX ctx;
	MD5_Init(&ctx);
	unsigned long len = 0;
	char buff[1024*16];
	while((len = read(fd,buff,1024)) > 0)
	  MD5_Update(&ctx,buff,len);
	lseek(fd,0,SEEK_SET);
	MD5_Final(tmp,&ctx);
	int i = 0;int n = 0;
	for(;i < 16;++i)
	{
		sprintf(&md[n],"%02x",tmp[i]);
		n += 2;
	}
}

void recv_dir(char *path,int sockfd)
{
	struct package package_msg;
	if(access(path,F_OK) == -1)
		assert((mkdir(path,0775)) != -1);
	while(1)
	{
		memset(&package_msg,0,4 + 512);
		if((recv(sockfd,&package_msg,4,0)) <= 0)
		{
			printf("ser error\n");
			return;
		}
		int length = ntohl(package_msg.msgLen);
		if((recv(sockfd,package_msg.buff,length,0)) <= 0)
		{
			printf("ser error\n");
			return;
		}

		if(strncmp(package_msg.buff,"file",4) == 0)
		{
			int file_size = 0,curr_size = 0;
			sscanf(package_msg.buff + 4,"%d",&file_size);
			char temp[100] = {0};
			strcat(temp,package_msg.buff);
			char *file_name = strtok(temp,"#");
			file_name = strtok(NULL,"#");
			char file_path[100] = {0};
			strcat(file_path,path);
			strcat(file_path,file_name);
			int ffd = open(file_path,O_WRONLY | O_CREAT,0600);
			assert(-1 != ffd);
			printf("文件:%s 大小:%d\n",file_path,file_size);
			do
			{
				memset(&package_msg,0,4 + 512);
				assert((recv(sockfd,&package_msg,4,0)) != -1);
				length = ntohl(package_msg.msgLen);
				assert((recv(sockfd,package_msg.buff,length,0)) != -1);
				assert(write(ffd,package_msg.buff,length) != -1);
				curr_size += length;
				float f = curr_size * 100.0 / file_size;
				printf("下载:%.2f%%\r",f);fflush(stdout);
			}while(length == 512);
			printf("\n");
			close(ffd);
		}
		else if(strcmp(package_msg.buff,"end") == 0)
		  return;
		else
		{
			char dir_path[100] = {0};
			strcat(dir_path,path);
			strcat(dir_path,package_msg.buff);strcat(dir_path,"/");
			recv_dir(dir_path,sockfd);
		}
	}
}

void send_dir(char *path,int sockfd)
{
	DIR *dir = opendir(path);
	struct dirent *file = NULL;
	struct package package_msg;
	int length = 0;
	memset(&package_msg,0,4 + 512);
	if(NULL != dir)
	{
		while((file = readdir(dir)) != NULL)
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
				char md5[33] = {0};
				calculate_md5(ffd,md5);
				sprintf(package_msg.buff + 4,"%d",file_size);
				strcat(package_msg.buff,"#");
				strcat(package_msg.buff,file->d_name);
				strcat(package_msg.buff,"#");
				strcat(package_msg.buff,md5);
				length = strlen(package_msg.buff);
				package_msg.msgLen = htonl(length);
				assert((send(sockfd,&package_msg,4 + length,0)) != -1);
				int curr_size = 0;
				memset(&package_msg,0,4 + 512);
				if((recv(sockfd,&package_msg,4,0)) <= 0)
				  return;
				length = ntohl(package_msg.msgLen);
				if((recv(sockfd,package_msg.buff,length,0)) <= 0)
				  return;
				if(strcmp(package_msg.buff,"传输完成") == 0)
					printf("文件:%s 大小:%d 完成秒传\n",file_path,file_size);
				else if(strcmp(package_msg.buff,"err") == 0)
				{
					printf("ser err\n");
					return;
				}
				else
				{
					printf("文件:%s 大小:%d\n",file_path,file_size);
					do
					{
						memset(&package_msg,0,4 + 512);
						length = read(ffd,package_msg.buff,512);
						if(length == -1)
							break;
						package_msg.msgLen = htonl(length);
						if((send(sockfd,&package_msg,4 + length,0)) == -1)
							break;
						curr_size += length;
						float f = curr_size * 100.0 / file_size;
						printf("上传:%.2f%%\r",f);fflush(stdout);
					}while(length == 512);
					printf("\n");
				}
				close(ffd);
			}
			else if(file->d_type == 4 && file->d_name[0] != '.')
			{
				char dir_path[100] = {0};
				memset(&package_msg,0,4 + 512);
				strcat(dir_path,file->d_name);
				strcpy(package_msg.buff,dir_path);
				length = strlen(package_msg.buff);
				package_msg.msgLen = htonl(length);
				assert((send(sockfd,&package_msg,4 + length,0)) != -1);
				memset(dir_path,0,100);
				strcat(dir_path,path);
				strcat(dir_path,file->d_name);strcat(dir_path,"/");
				send_dir(dir_path,sockfd);
			}
		}
		closedir(dir);
		memset(&package_msg,0,4 + 512);
		strcat(package_msg.buff,"end");
		length = strlen(package_msg.buff);
		package_msg.msgLen = htonl(length);
		assert((send(sockfd,&package_msg,4 + length,0)) != -1);
	}
	else
	  printf("open dir [%s] failed.\n",path);
}
