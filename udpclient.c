/*
* Authors: Harishkumar Katagal(109915793) & Gagan Nagaraju (109889036)
* FileName: udpclient.c
* 
*/

#include	"unpifiplus.h"
#include "clientsupport.h"

/*
* Structure to store the address.
*/
struct sock_info{
	int sockfd;
	struct sockaddr_in ipaddr;
	struct sockaddr_in netaddr;
	struct sockaddr_in subnetaddr;
};

void reconnecttoserver(int sockfd,struct sockaddr_in servaddr,char* recvline);

char fileread[7][50];

int
main(int argc, char **argv)
{
	int					sockfd;
	int i=0, length=0;
	int nrecv;
	const int			on = 1;
	pid_t				pid;
	struct ifi_info		*ifi, *ifihead;
	struct sockaddr_in	*sa, cliaddr, wildaddr, servaddr;
	struct sockaddr_in *printaddr, *netaddr, *subnet;
	struct sockaddr_in sa2;
	struct sock_info head[20];
	char buff[MAXLINE];
	int count =0;
	FILE *fp;
	int port;
	char line[50];
	int window;
	struct timeval timer;
	char server_ip[50];
	char final_server_ip[50];
	char final_client_ip[50];
	int cmpFlag,subnetFlag,portFlag=0;
	int localservFlag=0,subnetservFlag=0;
	char subnetStr[50];
	char prevsubnet[50];
	struct sockaddr_in localaddr;
	struct sockaddr_in peeraddr;
	fd_set rset, allset;
	int maxfd=-1;
	int nready;
	
	int		n;
	char	sendline[MAXLINE], recvline[MAXLINE + 1];
	
	/*
	*Read port number and sending sliding window size from file.
	*/
	if ((fp = fopen("client.in","r")) == NULL) {
	  printf("Error: cannot open file server.in for reading port number.\n");
	  exit(0);
	}
	for(i=0;i<4;i++){
		if(fgets(line,50,(FILE*)fp)!=NULL){
			length = strlen(line);
			strncpy(fileread[i],line,length);
			if(fileread[i][length-1]=='\n'){
				fileread[i][length-1] = '\0';
			}
		}
	}
	strncpy(server_ip,fileread[0],strlen(fileread[0])-1);
	port = atoi(fileread[1]);
	if(port == 0){
		printf("\nInvalid Port Number Specified in \"client.in\" file.");
		exit(0);
	}
	window = atoi(fileread[3]);
	if(window == 0){
		printf("\nInvalid sliding window size specified in client.in file.");
		exit(0);
	}	
	i=0;
	for (ifihead = ifi = Get_ifi_info_plus(AF_INET, 1);
		 ifi != NULL; ifi = ifi->ifi_next) {
		printaddr = (struct sockaddr_in *)ifi->ifi_addr;
		head[i].ipaddr = *printaddr;
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
		printf("\n%d Address found for client,\n",count);
		sockfd = Socket(AF_INET, SOCK_DGRAM, 0);
	}
	else{
		printf("\nCannot find IP address of server.\n");
		exit(0);
	}
	printf("\nFollowing are the address found for client\n");
	for(i=0;i<count;i++){
		printf("******************%d********************\n",i);
		Inet_ntop(AF_INET, &head[i].ipaddr.sin_addr,buff,sizeof(buff));
		printf("IP Address: %-25s\n",buff);
		Inet_ntop(AF_INET, &head[i].netaddr.sin_addr,buff,sizeof(buff));
		printf("Network Address: %-25s\n",buff);
		Inet_ntop(AF_INET, &head[i].subnetaddr.sin_addr,buff,sizeof(buff));
		printf("Subnet Address: %-25s\n",buff);
		printf("***************************************\n\n");
	}
	
	/* 
	* To check if the server is on the same host by comparing the IP address.
	*/
	for(i=0;i<count;i++){
		Inet_ntop(AF_INET, &head[i].ipaddr.sin_addr,buff,sizeof(buff));
		cmpFlag = (strcmp(server_ip,buff));
		if(cmpFlag == 0){
			printf("\n\nServer is local to client.\n\n");
			strcpy(final_server_ip,"127.0.0.1");
			strcpy(final_client_ip,"127.0.0.1");
			Setsockopt(sockfd, SOL_SOCKET, SO_DONTROUTE, &on, sizeof(on));
			localservFlag = 1;
			break;
		}
	}
	/*
	* To check if the server is on the same subnet.
	*/
	if(localservFlag == 0){
		strcpy(final_server_ip,server_ip);
		for(i=0;i<count;i++){
			Inet_pton(AF_INET,server_ip,&printaddr->sin_addr);
			subnet = (struct sockaddr_in *) malloc(sizeof(printaddr));
			subnet->sin_addr.s_addr = printaddr->sin_addr.s_addr & head[i].netaddr.sin_addr.s_addr;
			Inet_ntop(AF_INET, &subnet->sin_addr,subnetStr,sizeof(subnetStr));
			Inet_ntop(AF_INET, &head[i].subnetaddr.sin_addr,buff,sizeof(buff));
			cmpFlag = (strcmp(buff,subnetStr));
			if(cmpFlag==0){
				if(strlen(final_client_ip)==0){
					Inet_ntop(AF_INET, &head[i].ipaddr.sin_addr,final_client_ip,sizeof(final_client_ip));
					strcpy(prevsubnet,buff);
					subnetservFlag = 1;
					printf("\n\nServer is on the same subnet\n\n");
					Setsockopt(sockfd, SOL_SOCKET, SO_DONTROUTE, &on, sizeof(on));
				}
				else{
					subnetFlag = strcmp(buff,prevsubnet);
					if(subnetFlag>0){
						Inet_ntop(AF_INET, &head[i].ipaddr.sin_addr,final_client_ip,sizeof(final_client_ip));
					}
				}
			}
		}
		if(subnetservFlag == 1){
			printf("\nFinal Client IP: %s\n\n", final_client_ip);
		}
	}
	
	/*If not on the same subnet then choose any ip address for client*/
	if(localservFlag == 0 && subnetservFlag==0){
		strcpy(final_server_ip,server_ip);
		for(i=0;i<count;i++){
			Inet_ntop(AF_INET, &head[i].ipaddr.sin_addr,buff,sizeof(buff));
			cmpFlag = strcmp(buff,"127.0.0.1");
			if(cmpFlag == 0){
				continue;
			}
			else{
				strcpy(final_client_ip,buff);
				break;
			}
		}
		if(strlen(final_client_ip)==0){
			printf("\n\nThe server is not local or in subnet and client does not have a subnet.\n\n");
			exit(0);
		}
		printf("\nClient is not on same host and not in same subnet. Hence choosing any one IP address for client from above.\n\n");
		printf("\nFinal Client IP: %s\n\n", final_client_ip);
	}
	
	/*
	* Creating socket
	*/
	Setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = port;
	Inet_pton(AF_INET, final_server_ip, &servaddr.sin_addr);
	sa = (struct sockaddr_in *) malloc(sizeof(sa));
	Inet_pton(AF_INET, final_client_ip, &sa->sin_addr);
	sa->sin_family = AF_INET;
	sa->sin_port = 0;
	Bind(sockfd, (SA *) sa, sizeof(*sa));
	length = sizeof(localaddr);
	Getsockname(sockfd,(SA *) &localaddr,&length);
	printf("\n\nClient address and port number assigned to the socket using getsockname:%s\n\n",Sock_ntop((SA*) &localaddr,length));
	//dg_cli(stdin, sockfd, (SA *) &servaddr, sizeof(servaddr));
	printf("Connecting to server.\n");
	Connect(sockfd, (SA *) &servaddr, sizeof(servaddr));
	length = sizeof(peeraddr);
	Getpeername(sockfd,(SA*) &peeraddr, &length);
	printf("\n\nServer address and port number assigned to the socket using getpeername:%s\n\n",Sock_ntop((SA*) &peeraddr,length));
	strncpy(sendline,fileread[2],strlen(fileread[2])-1);
	Write(sockfd,sendline,strlen(sendline));
	printf("Sent %s file name to server for FTP.\n",sendline);
	printf("");
	timer.tv_sec = 3;
	timer.tv_usec = 0;
	FD_ZERO(&rset);
	for(;;){
		FD_SET(sockfd,&rset);
		maxfd = sockfd+1;
		if(nready = select(maxfd,&rset,NULL,NULL,&timer)<0){
			if(errno == EINTR)
				continue;
			else{
				perror("Select error\n");
				break;
			}	
		}
		if(FD_ISSET(sockfd,&rset)){
			if(portFlag==0){
				length = sizeof(servaddr);
				Recvfrom(sockfd, recvline, MAXLINE, 0, (SA *) &servaddr, &length);
				printf("\nNew Port number received from server: %s\n\n",recvline);
				servaddr.sin_port = htons(atoi(recvline));
				printf("Connecting to server on new port number.\n");
				Connect(sockfd, (SA *) &servaddr, sizeof(servaddr));
				length = sizeof(peeraddr);
				Getpeername(sockfd,(SA*) &peeraddr, &length);
				printf("\n\nServer address and port number assigned to the socket using getpeername after reconnecting:%s\n\n",Sock_ntop((SA*) &peeraddr,length));
				reconnecttoserver(sockfd,servaddr,recvline);
				portFlag =1;
				continue;
			}
			else if(portFlag == 1){
				length = sizeof(servaddr);
				Recvfrom(sockfd, recvline, MAXLINE, 0, (SA *) &servaddr, &length);
				if(strcmp(recvline,"Connected")==0){
					printf("\nConnected to Server on new port for reliable communication.\n\n");
					break;
				}
			}
		}
		Sendto(sockfd,sendline,strlen(sendline),0,NULL,NULL);
	}
	/*
	* Calling the function to receive data from client. 
	*/
	printf("\n Ready to receive file from the server\n\n.");
	client_receive_data(sockfd);
	printf("\n");
}

/*
* Function to retransmit the packet in case of packet loss.
*/

void reconnecttoserver(int sockfd,struct sockaddr_in servaddr,char* recvline){
	char	sendline[MAXLINE];
	struct sockaddr_in localaddr;
	struct sockaddr_in peeraddr;
	int length;
	length = sizeof(localaddr);
	strcpy(sendline,fileread[3]);
	printf("\nSending receiver window to server: %s\n",sendline);
	Sendto(sockfd,sendline,strlen(sendline),0,NULL,NULL);
}

