#ifndef HTTP_CONN
#define HTTP_CONN


#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <ctype.h>
#include <sys/wait.h>


#define ISspace(x) isspace((int)(x))
#define SERVER_STRING "Server: SongHao's http/0.1.0\r\n"  //定义个人server名称

class httpConn {
public:
    httpConn() {}
    ~httpConn() {}
public:
    void init(int cilent_sock, const sockaddr_in& cilent_address);  // 初始化客户连接
    void init();
    void do_request();         // 开始解析报文
    void close_conn();         // 关闭客户连接

    int get_line(char* buf, int size);    // 读取一行http报文 (把回车换行等情况都统一为换行符结束)
    void unimplemented();      // 501: 返回给浏览器表明收到的 HTTP 请求所用的 method 不被支持
    void not_found();          // 404: 主要处理找不到请求文件时的情况
    void serve_file();         // 如果不是CGI文件，也就是静态文件，直接读取文件返回给请求的http客户端即可
    void execute_cgi(const char* query_string);  // 执行cgi动态解析
    void bad_request();        // 400: 返回给客户端这是个错误请求
    void cannot_execute();     // 500: 主要处理执行 cgi 程序时出现的错误
private:
    void headers();            // 把HTTP响应的头部写到套接字
    void cat(FILE* resource);  // 读取服务器上某个文件，并将其写到套接字

private:
    int m_client_sock;
    sockaddr_in m_client_address;
private:
    int m_cgi;

    // char m_rbuf[1024];   // 读缓冲区
    char m_method[255];  // 请求方法
    char m_url[255];     // url
    char m_path[512];    // 资源文件路径
    // char m_wbuf[1024];   // 写缓冲区
};


#endif // !HTTP_CONN 