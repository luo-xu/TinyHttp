#include "httpConn.h"
#include <pthread.h>

#define MAX_FD 65536

void error_die(const char* sc);
int startup(u_short* port);
void* accept_request(void* from_client);

// 出错处理 (结束程序，打印错误)
void error_die(const char* sc) {
	perror(sc);
	exit(1);
}

// 启动服务端 (创建、绑定、监听)
int startup(u_short* port) {
	// 创建httpd监听套接字
	struct sockaddr_in name;
	int httpd = socket(PF_INET, SOCK_STREAM, 0);
	if (httpd == -1)
		error_die("socket");

	// 设置端口复用
	int option = 1;
	socklen_t optlen = sizeof(option);
	setsockopt(httpd, SOL_SOCKET, SO_REUSEADDR, (void*)&option, optlen);

	// 绑定地址
	memset(&name, 0, sizeof(name));
	name.sin_family = AF_INET;
	name.sin_port = htons(*port);
	name.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(httpd, (struct sockaddr*)&name, sizeof(name)) < 0)
		error_die("bind");

	// 动态分配一个端口 
	// if (*port == 0)  {
	// 	socklen_t  namelen = sizeof(name);
	// 	if (getsockname(httpd, (struct sockaddr*)&name, &namelen) == -1)
	// 		error_die("getsockname");
	// 	*port = ntohs(name.sin_port);
	// }

	// 监听
	if (listen(httpd, 5) < 0)
		error_die("listen");

	return(httpd);
}

//  处理监听到的 HTTP 请求 (线程函数)
void* accept_request(void* client_httpConn) {
	httpConn* client_conn = (httpConn*)client_httpConn;

	client_conn->do_request();   // 开始解析报文
	client_conn->close_conn();   // 关闭客户连接

	return NULL;
}


int main(int argc, char* argv[]) {
	// 预先为每个可能的客户连接 分配一个http_conn对象
	httpConn* users = new httpConn[MAX_FD];

	u_short port = 6379;
	int server_sock = startup(&port);
	printf("http server_sock is %d\n", server_sock);
	printf("http running on port %d\n", port);

	while (1) {
		sockaddr_in client_address;
		socklen_t client_address_len = sizeof(client_address);
		int client_sock = accept(server_sock, (struct sockaddr*)&client_address, &client_address_len);
		if (client_sock == -1)
			error_die("accept");
		printf("New connection....  ip: %s , port: %d\n", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));

		users[client_sock].init(client_sock, client_address);


		pthread_t newthread;
		if (pthread_create(&newthread, NULL, accept_request, (void*)&users[client_sock]) != 0)
			perror("pthread_create");

	}

	close(server_sock);
	return 0;
}