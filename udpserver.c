/*
* Authors: Harishkumar Katagal(109915793) & Gagan Nagaraju (109889036)
* FileName: udpserver.c
* 
*/

#include	"unpifiplus.h"
#include "serversupport.h"

void	mydg_echo(int, SA *, socklen_t, SA *);
int createnewsocket(int sockfd, char* final_client_ip, int final_client_port, char* final_server_ip, struct sockaddr_in cliaddr );
int isIpportserviced(char* ip_temp,int port_temp);
void recordipport(char* ip_temp,int port_temp, pid_t pid);
int deleteport(pid_t pid);

/*
* Structure to store the address.
*/
struct sock_info{
	int sockfd;
	struct sockaddr_in ipaddr;
	struct sockaddr_in netaddr;
	struct sockaddr_in subnetaddr;
};


/*
* Structure for storing the IP ADDRESS
*/
struct ipports{
	char ip_addr[50];
	int port;
	pid_t pid;	
}ipport[100];

int ipcount=0;
char rwnd[MAXLINE];

void sig_child(int signo)
{
	pid_t	pid;
	int		status;

	while ( (pid = waitpid(-1, &status, WNOHANG)) > 0){
		printf("Child with pid : %d terminated\n", pid);
		deleteport(pid);
	}
	return;
}

int
main(int argc, char **argv)
{
	int					sockfd, listenfd, connfd;
	int i=0,k=0;
	const int			on = 1;
	pid_t				pid;
	struct ifi_info		*ifi, *ifihead;
	struct sockaddr_in	*sa, cliaddr, wildaddr;
	struct sockaddr_in *printaddr, *netaddr, *subnet;
	struct sockaddr_in peeraddr;
	struct sock_info head[20];
	char buff[MAXLINE];
	char recvline[MAXLINE];
	char filename[50];
	int count =0;
	FILE *fp;
	int port;
	char line[50];
	int window;
	fd_set rset, allset;
	int maxfd=-1;
	int nready;
	int length;
	char client_ip[50];
	int client_port;
	char final_server_ip[50];
	char final_client_ip[50];
	char ip_temp[50];
	int port_temp;
	char subnetStr[50];
	int cmpFlag = -1, subnetFlag = 0;
	void sig_child(int);
	
	Signal(SIGCHLD, sig_child);
	
	
	/*
	* Read port number and sending sliding window size from file.
	*/
	if ((fp = fopen("server.in","r")) == NULL) {
	  printf("Error: cannot open file server.in for reading port number.\n");
	  exit(0);
	}
	fgets(line,50,(FILE*) fp);
	port = atoi(line);
	if(port == 0){
		printf("\nInvalid Port Number Specified in \"server.in\" file.");
		exit(0);
	}
	fgets(line,50,(FILE*) fp);
	window = atoi(line);
	if(window == 0){
		printf("\nInvalid sliding window size specified in server.in file.");
		exit(0);
	}
	
	for (ifihead = ifi = Get_ifi_info_plus(AF_INET, 1);
		 ifi != NULL; ifi = ifi->ifi_next) {
		sockfd = Socket(AF_INET, SOCK_DGRAM, 0);
		head[i].sockfd = sockfd;
		Setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
		sa = (struct sockaddr_in *) ifi->ifi_addr;
		sa->sin_family = AF_INET;
		sa->sin_port = port;
		printaddr = (struct sockaddr_in *)ifi->ifi_addr;
		head[i].ipaddr = *printaddr;
		Bind(sockfd, (SA *) sa, sizeof(*sa));
		maxfd = max(sockfd,maxfd);
		subnet = (struct sockaddr_in *) malloc(sizeof(netaddr));
		if ( (netaddr = (struct sockaddr_in *)ifi->ifi_ntmaddr) != NULL){
			*subnet = *netaddr;
			subnet->sin_addr.s_addr = printaddr->sin_addr.s_addr & netaddr->sin_addr.s_addr;
			head[i].netaddr = *netaddr;
			head[i].subnetaddr = *subnet;
		}
		i++;
	}
	count =i;
	if(count!=0){
		printf("\n\n%d Sockets Created on following addresses,\n\n",count);
		/* To specify in the select. */
		maxfd = maxfd+1;
		/* Initialize the set: all bits off.*/	
		FD_ZERO(&allset);
	}
	else{
		printf("\nCannot find IP address of server.\n");
		exit(0);
	}
	printf("\nFollowing are the address found for server:\n");
	for(i=0;i<count;i++){
		/* Turn on bits for sockets*/
		FD_SET(head[i].sockfd,&allset);
		printf("******************%d********************\n",i);
		Inet_ntop(AF_INET, &head[i].ipaddr.sin_addr,buff,sizeof(buff));
		printf("IP Address: %-20s\n",buff);
		Inet_ntop(AF_INET, &head[i].netaddr.sin_addr,buff,sizeof(buff));
		printf("Network Address: %-20s\n",buff);
		Inet_ntop(AF_INET, &head[i].subnetaddr.sin_addr,buff,sizeof(buff));
		printf("Subnet Address: %-20s\n",buff);
		printf("***************************************\n\n");
	}
	
	for ( ; ; ) {
		for(k=0;k<count;k++){
					FD_SET(head[k].sockfd,&allset);
		}
		/* 
		* Set select on the two sockets. 
		*/
		if((select(maxfd,&allset,NULL,NULL,NULL))<0){
			if(errno == EINTR)
					continue;
				else
					err_sys("Select Error");
		}
		for(i=0;i<count;i++){
			if(FD_ISSET(head[i].sockfd,&allset)){
				/* getpeername(connfd,(SA*) &peeraddr, &length);
				Inet_ntop(AF_INET, &head[i].ipaddr.sin_addr,ip_temp,sizeof(ip_temp));
				port_temp = ntohs(head[i].ipaddr.sin_port); */
				
				/* else{
					recordipport(ip_temp,port_temp);
				} */
				length = sizeof(cliaddr);
				listenfd = head[i].sockfd;
				if((recvfrom(listenfd, recvline, MAXLINE, 0, (SA *) &cliaddr, &length))<0){
					if(errno == EINTR)
						continue;
					else
						err_sys("Receive From Error");
				}
				Inet_ntop(AF_INET, &cliaddr.sin_addr,ip_temp,sizeof(ip_temp));
				port_temp = ntohs(cliaddr.sin_port);
				if(isIpportserviced(ip_temp,port_temp)==1){
					printf("\nIP address %s and port number %d is already requested.\n\n",ip_temp,port_temp);
					break;
				}
				printf("\nNew Request Received from client at: %s\n\n",Sock_ntop((SA*) &cliaddr,length));
				
				strcpy(filename,recvline);
				printf("Filename Requested: %s\n", filename);
				
				if ( (pid = Fork()) == 0) {
					
					for(k=0;k<count;k++){
						if(k==i) continue;
						Close(head[k].sockfd);
					}
					Inet_ntop(AF_INET, &cliaddr.sin_addr,client_ip,sizeof(client_ip));
					client_port = ntohs(cliaddr.sin_port);
					if(strcmp(client_ip,"127.0.0.1")==0){
						/*Set socket option*/
						printf("\nClient IP is localloop back hence setting don't route option.\n\n");
						Setsockopt(listenfd, SOL_SOCKET, SO_DONTROUTE, &on, sizeof(on));
						subnetFlag = 1;
					}
					else{
						for(k=0;k<count;k++){
							Inet_pton(AF_INET,client_ip,&printaddr->sin_addr);
							subnet = (struct sockaddr_in *) malloc(sizeof(printaddr));
							subnet->sin_addr.s_addr = printaddr->sin_addr.s_addr & head[k].netaddr.sin_addr.s_addr;
							Inet_ntop(AF_INET, &subnet->sin_addr,subnetStr,sizeof(subnetStr));
							Inet_ntop(AF_INET, &head[k].subnetaddr.sin_addr,buff,sizeof(buff));
							cmpFlag = (strcmp(buff,subnetStr));
							if(cmpFlag==0){
								printf("\nClient is in same subnet hence setting don't route option.\n\n");
								Setsockopt(listenfd, SOL_SOCKET, SO_DONTROUTE, &on, sizeof(on));
								subnetFlag = 1;
								break;
							}
						}
					}
					if(subnetFlag==0){
						printf("\nClient is not on the same subnet\n\n");
					}
					Inet_ntop(AF_INET, &head[i].ipaddr.sin_addr,final_server_ip,sizeof(final_server_ip));
					
					connfd = createnewsocket(listenfd, client_ip,client_port,final_server_ip,cliaddr);
					Close(listenfd);
					printf("\nClosing listening socket\n\n");
					udp_start_send(connfd,filename,atoi(rwnd),(SA*) &cliaddr,sizeof(cliaddr),window);
					udp_stop(connfd,(SA*) &cliaddr,sizeof(cliaddr));
									
				}
				if(pid>0){
					recordipport(ip_temp,port_temp,pid);
				}
			
			}
		}
	
	}
	printf("\n");
}

int createnewsocket(int sockfd, char* final_client_ip, int final_client_port, char* final_server_ip, struct sockaddr_in cliaddr ){
	int connfd;
	int on=1;
	struct sockaddr_in servaddr,*sa, localaddr,peeraddr;
	int length;
	char sendline[MAXLINE];
	char recvline[MAXLINE];
	fd_set rset;
	int maxfd=-1;
	
	int nready;
	struct timeval timer;
	
	
	connfd = Socket(AF_INET, SOCK_DGRAM, 0);	
	printf("\nCreating new socket for reliable communication.\n");
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = 0;
	Inet_pton(AF_INET, final_server_ip, &servaddr.sin_addr);
	Bind(connfd, (SA *) &servaddr, sizeof(servaddr));
	length = sizeof(localaddr);
	getsockname(connfd,(SA *) &localaddr,&length);
	printf("\nServer address and port number assigned to the socket using getsockname:%s\n\n",Sock_ntop((SA*) &localaddr,length));
	sa = (struct sockaddr_in *) malloc(sizeof(sa));
	Inet_pton(AF_INET, final_client_ip, &sa->sin_addr);
	sa->sin_family = AF_INET;
	sa->sin_port = final_client_port;
	Connect(connfd, (SA *) sa, sizeof(*sa));
	length = sizeof(peeraddr);
	getpeername(connfd,(SA*) &peeraddr, &length);
	printf("\nClient address and port number assigned to the socket using getpeername:%s\n\n",Sock_ntop((SA*) &peeraddr,length));
	getpeername(sockfd,(SA*) &peeraddr, &length);
	sprintf(sendline,"%d",ntohs(localaddr.sin_port));
	Sendto(sockfd,sendline,strlen(sendline),0,(SA*) &cliaddr,sizeof(cliaddr));
	printf("\nSending new port %s to client\n\n",sendline);
	length = sizeof(cliaddr);
	timer.tv_sec = 3;
	timer.tv_usec = 0;
	FD_ZERO(&rset);
	for(;;){
		FD_SET(connfd,&rset);
		maxfd = connfd+1;
		if(nready = select(maxfd,&rset,NULL,NULL,&timer)<0){
			if(errno == EINTR)
				continue;
			else{
				err_sys("Select error\n");
				}	
		}
		if(FD_ISSET(connfd,&rset)){
			length = sizeof(servaddr);
			if((recvfrom(connfd, recvline, MAXLINE, 0, NULL, NULL))<0){
					if(errno == EINTR)
					continue;
				else{
					err_sys("Select error\n");
					}
			}
			bzero(sendline,strlen(sendline));
			strcpy(sendline,"Connected");
			Sendto(connfd,sendline,strlen(sendline),0,NULL,NULL);
			break;
		}
		Sendto(connfd,sendline,strlen(sendline),0,NULL,NULL);
		Sendto(sockfd,sendline,strlen(sendline),0,NULL,NULL);
	}
	strcpy(rwnd,recvline);
	printf("\n Connected to client on new port number.\n\n");
	printf("\nReceiver Window size received: %s\n\n",rwnd);
	return connfd;
	
}

void recordipport(char* ip_temp,int port_temp, pid_t pid){
	if(ipcount<=99){
		strcpy(ipport[ipcount].ip_addr,ip_temp);
		ipport[ipcount].port = port_temp;
		ipport[ipcount].pid = pid;
		ipcount++;
	}
	
}

int isIpportserviced(char* ip_temp,int port_temp){
	int i;
	for(i=0;i<ipcount;i++){
		if((strcmp(ipport[i].ip_addr,ip_temp) == 0)&&(ipport[i].port == port_temp)){
			return 1;
		}
	}
	return 0;
	
}

int deleteport(pid_t pid){
	int i;
	for(i=0;i<ipcount;i++){
		if(pid == ipport[i].pid){
			strcpy(ipport[i].ip_addr,"EMPTY");
			ipport[i].port = -1;
		}
	}
}


