#include "httpConn.h"

// 初始化客户连接
void httpConn::init(int client_sock, const sockaddr_in& client_address) {
    m_client_sock = client_sock;
    m_client_address = client_address;

    init();
}
void httpConn::init() {
    m_cgi = 0;

    // memset(&m_rbuf, 0, sizeof(m_rbuf));
    memset(&m_method, 0, sizeof(m_method));
    memset(&m_url, 0, sizeof(m_url));
    memset(&m_path, 0, sizeof(m_path));
    // memset(&m_wbuf, 0, sizeof(m_wbuf));
}

// 关闭客户连接
void httpConn::close_conn() {
    printf("connection close....client: %d \n", m_client_sock);

    close(m_client_sock);
    m_client_sock = -1;
    init();
}

// 线程主体部分
void httpConn::do_request() {

    // 读取一行http报文 (把回车换行等情况都统一为换行符结束)
    char buf[1024] = { '\0' };
    int numchars = get_line(buf, sizeof(buf));

    // 提取请求方法
    size_t i = 0, j = 0;
    while (!ISspace(buf[j]) && (i < sizeof(m_method) - 1)) {

        m_method[i] = buf[j];
        i++;
        j++;
    }
    m_method[i] = '\0';

    // 只支持GET或POST请求
    if (strcasecmp(m_method, "GET") && strcasecmp(m_method, "POST")) {
        unimplemented();   // 501: 返回给浏览器表明收到的 HTTP 请求所用的 method 不被支持
        return;
    }

    // 如果是POST请求，则开启cgi
    if (strcasecmp(m_method, "POST") == 0)  m_cgi = 1;

    // 跳过空格
    i = 0;
    while (ISspace(buf[j]) && (j < sizeof(buf))) {
        j++;
    }

    // 提取url
    while (!ISspace(buf[j]) && (i < sizeof(m_url) - 1) && (j < sizeof(buf))) {
        m_url[i] = buf[j];
        i++;
        j++;
    }
    m_url[i] = '\0';

    // GET请求url可能会带有?,有查询参数
    char* query_string = NULL;
    if (strcasecmp(m_method, "GET") == 0)
    {
        query_string = m_url;
        while ((*query_string != '?') && (*query_string != '\0'))
            query_string++;

        /* 如果有?表明是动态请求, 开启cgi */
        if (*query_string == '?')
        {
            m_cgi = 1;
            *query_string = '\0';
            query_string++;
        }
    }

    // 合成资源文件路径
    sprintf(m_path, "httpdocs%s", m_url);

    // 主页
    if (m_path[strlen(m_path) - 1] == '/')
    {
        strcat(m_path, "test.html");
    }

    // 检测客户请求资源是否存在
    struct stat st;
    if (stat(m_path, &st) == -1) {
        printf("[do_request]: 文件不存在\n");
        while ((numchars > 0) && strcmp("\n", buf)) {
            numchars = get_line(buf, sizeof(buf));
        }
        not_found();  // 404: 主要处理找不到请求文件时的情况
    }
    else
    {
        printf("[do_request]: 文件存在\n");
        if ((st.st_mode & S_IFMT) == S_IFDIR) { // S_IFDIR代表目录 (如果请求参数为目录, 自动打开test.html)
            strcat(m_path, "/test.html");
        }

        // 检测文件是否可执行
        if ((st.st_mode & S_IXUSR) || (st.st_mode & S_IXGRP) || (st.st_mode & S_IXOTH)) {// S_IXUSR:文件所有者具可执行权限 S_IXGRP:用户组具可执行权限 S_IXOTH:其他用户具可读取权限  
            m_cgi = 1;
        }

        if (!m_cgi) {
            printf("[do_request]: 执行server_file\n");
            serve_file();   // 如果不是CGI文件，也就是静态文件，直接读取文件返回给请求的http客户端即可
        }
        else {
            execute_cgi(query_string);
        }
    }

    printf("[do_request]: 线程结束\n");
}

// 读取一行http报文 (把回车换行等情况都统一为换行符结束)
int httpConn::get_line(char* buf, int size)
{
    printf("pre [get_line]: %s\n", buf);
    int i = 0;
    char c = '\0';
    printf("[size] %d\n", size);

    while ((i < size - 1) && (c != '\n'))
    {
        int n = recv(m_client_sock, &c, 1, 0);

        if (n > 0)
        {
            if (c == '\r')
            {
                n = recv(m_client_sock, &c, 1, MSG_PEEK);
                if ((n > 0) && (c == '\n'))
                    recv(m_client_sock, &c, 1, 0);
                else
                    c = '\n';
            }
            buf[i] = c;
            i++;

        }
        else
            c = '\n';
    }
    buf[i] = '\0';

    printf("[get_line]: %s\n", buf);
    return(i);
}

// 501: 返回给浏览器表明收到的 HTTP 请求所用的 method 不被支持
void httpConn::unimplemented()
{
    char buf[1024];
    //发送501说明相应方法没有实现
    sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
    send(m_client_sock, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(m_client_sock, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(m_client_sock, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(m_client_sock, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
    send(m_client_sock, buf, strlen(buf), 0);
    sprintf(buf, "</TITLE></HEAD>\r\n");
    send(m_client_sock, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
    send(m_client_sock, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(m_client_sock, buf, strlen(buf), 0);
}

// 404: 主要处理找不到请求文件时的情况
void httpConn::not_found()
{
    char buf[1024];
    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
    send(m_client_sock, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(m_client_sock, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(m_client_sock, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(m_client_sock, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    send(m_client_sock, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
    send(m_client_sock, buf, strlen(buf), 0);
    sprintf(buf, "your request because the resource specified\r\n");
    send(m_client_sock, buf, strlen(buf), 0);
    sprintf(buf, "is unavailable or nonexistent.\r\n");
    send(m_client_sock, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(m_client_sock, buf, strlen(buf), 0);
}


// 如果不是CGI文件，也就是静态文件，直接读取文件返回给请求的http客户端即可
void httpConn::serve_file() {

    int numchars = 1;
    char buf[1024] = { '\0' };
    buf[0] = 'A';
    buf[1] = '\0';
    while ((numchars > 0) && strcmp("\n", buf))
    {
        numchars = get_line(buf, sizeof(buf));
    }

    // 打开文件
    FILE* resource = fopen(m_path, "r");
    if (resource == NULL) {
        printf("[server_file]: 执行 not_found\n");
        not_found();
    }
    else {
        printf("[server_file]: 执行 headers cat\n");
        headers();   // 把HTTP响应的头部写到套接字
        cat(resource);
    }

    fclose(resource);//关闭文件句柄
}

// 把HTTP响应的头部写到套接字
void httpConn::headers() {
    char buf[1024] = { '\0' };
    (void)m_path;  /* could use filename to determine file type */
    // 发送HTTP头
    strcpy(buf, "HTTP/1.0 200 OK\r\n");
    send(m_client_sock, buf, strlen(buf), 0);
    printf("[head]: %s\n", buf);
    strcpy(buf, SERVER_STRING);
    send(m_client_sock, buf, strlen(buf), 0);
    printf("[head]: %s\n", buf);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(m_client_sock, buf, strlen(buf), 0);
    printf("[head]: %s\n", buf);
    strcpy(buf, "\r\n");
    send(m_client_sock, buf, strlen(buf), 0);
    printf("[head]: %s\n", buf);

}

// 读取服务器上某个文件，并将其写到套接字
void httpConn::cat(FILE* resource) {
    // 发送文件的内容
    char buf[1024] = { '\0' };
    fgets(buf, sizeof(buf), resource);
    while (!feof(resource))
    {
        send(m_client_sock, buf, strlen(buf), 0);
        printf("[cat]: %s\n", buf);
        fgets(buf, sizeof(buf), resource);
    }
}

// 执行cgi动态解析
void httpConn::execute_cgi(const char* query_string) {
    char buf[1024] = { '\0' };
    // cgi子进程与父进程通信的管道
    int cgi_output[2];
    int cgi_input[2];

    pid_t pid;
    int status;

    int i;
    char c;

    int numchars = 1;
    int content_length = -1;
    // 默认字符
    buf[0] = 'A';
    buf[1] = '\0';
    if (strcasecmp(m_method, "GET") == 0) {
        while ((numchars > 0) && strcmp("\n", buf)) {
            numchars = get_line(buf, sizeof(buf));
        }
    }
    else {
        numchars = get_line(buf, sizeof(buf));
        while ((numchars > 0) && strcmp("\n", buf))
        {
            buf[15] = '\0';
            if (strcasecmp(buf, "Content-Length:") == 0)
                content_length = atoi(&(buf[16]));

            numchars = get_line(buf, sizeof(buf));
        }

        if (content_length == -1) {
            bad_request();   // 400: 返回给客户端这是个错误请求
            return;
        }
    }

    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    send(m_client_sock, buf, strlen(buf), 0);

    // 创建用于父子进程通信的管道(半双工，所以要创建两个)
    if (pipe(cgi_output) < 0) {
        cannot_execute();
        return;
    }
    if (pipe(cgi_input) < 0) {
        cannot_execute();
        return;
    }

    // 在父进程中，关闭 cgi_input读端 和 cgi_output写端，如果是 POST请求，则将数据写入 cgi_input写端，子进程直接在STDIN读 (cgi_input读端被重定向到 STDIN)
    // 子进程将处理的应答 写入STDOUT (cgi_output写端被重定向到 STDOUT)，主进程读取 cgi_output读端获取应答数据。 并将数据输出到客户端，接着关闭所有管道，等待子进程结束。
    if ((pid = fork()) < 0) {
        cannot_execute();
        return;
    }
    // 子进程: 运行CGI 脚本 
    if (pid == 0)
    {
        char meth_env[1024] = { '\0' };
        char query_env[1024] = { '\0' };
        char length_env[1024] = { '\0' };

        // 重定向(cgi_output[1],cgi_input[0]被关闭，但由STDIN、STDOUT指向相同位置取代)
        // 因此对于子进程而言：STDIN从cgi_input[1]读，STDOUT写到cig_output[0]
        dup2(cgi_output[1], 1);
        dup2(cgi_input[0], 0);


        close(cgi_output[0]);  // 关闭cgi_output中的读端 (cgi_output读端用于父进程)
        close(cgi_input[1]);   // 关闭cgi_input中的写端  (cgi_input写端用于父进程)


        sprintf(meth_env, "REQUEST_METHOD=%s", m_method);
        putenv(meth_env);

        // 设置环境变量
        if (strcasecmp(m_method, "GET") == 0) {
            // 存储QUERY_STRING
            sprintf(query_env, "QUERY_STRING=%s", query_string);
            putenv(query_env);
        }
        else {   /* POST */
            // 存储CONTENT_LENGTH
            sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
            putenv(length_env);
        }

        //执行CGI脚本
        execl(m_path, m_path, NULL);

        exit(0);
    }
    // 父进程
    else {
        close(cgi_output[1]);   // 关闭cgi_output中的写端 (cgi_output写端用于子进程)
        close(cgi_input[0]);    // 关闭cgi_input中的读端  (cgi_input读端用于子进程)

        if (strcasecmp(m_method, "POST") == 0) {
            for (i = 0; i < content_length; i++) {
                recv(m_client_sock, &c, 1, 0);
                write(cgi_input[1], &c, 1);   // 父进程写管道cgi_input写端
            }
        }

        while (read(cgi_output[0], &c, 1) > 0) {   // 父进程读管道cgi_output读端 (读取cgi脚本返回数据)
            send(m_client_sock, &c, 1, 0);         // 发送给浏览器
        }

        // 运行结束关闭
        close(cgi_output[0]);
        close(cgi_input[1]);


        waitpid(pid, &status, 0); // 等待子进程结束
    }
}

// 400: 返回给客户端这是个错误请求
void httpConn::bad_request()
{
    char buf[1024];
    //发送400
    sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
    send(m_client_sock, buf, sizeof(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(m_client_sock, buf, sizeof(buf), 0);
    sprintf(buf, "\r\n");
    send(m_client_sock, buf, sizeof(buf), 0);
    sprintf(buf, "<P>Your browser sent a bad request, ");
    send(m_client_sock, buf, sizeof(buf), 0);
    sprintf(buf, "such as a POST without a Content-Length.\r\n");
    send(m_client_sock, buf, sizeof(buf), 0);
}

// 500: 主要处理执行 cgi 程序时出现的错误
void httpConn::cannot_execute()
{
    char buf[1024];
    // 发送500
    sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
    send(m_client_sock, buf, strlen(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(m_client_sock, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(m_client_sock, buf, strlen(buf), 0);
    sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
    send(m_client_sock, buf, strlen(buf), 0);
}