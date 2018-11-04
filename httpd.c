#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>
#include <sys/stat.h>
#include <ctype.h>
#include <sys/sendfile.h>
#include <unistd.h>
#include <string.h>
#define MAX 1024
#define HOME_PAGE "index.html"
#define PAGE_404  "404.html"
#define true 1
#define false 0
int DEBUG = false;
void usage(const char* proc)
{
    printf("Usage: %s [port] [DEBUG?]\n",proc);
}

int startp(int port)
{
    int listen_sock = socket(AF_INET,SOCK_STREAM,0);
    if(listen_sock < 0)
    {
        perror("socket");
        exit(2);
    }
    
    int opt = 1;
    setsockopt(listen_sock,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));

    struct sockaddr_in local;
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    local.sin_port = htons(port);

    if(bind(listen_sock,(struct sockaddr*)&local,sizeof(local)) < 0)
    {
        perror("bind");
        exit(3);
    }

    if(listen(listen_sock,5) < 0)
    {
        perror("listen");
        exit(4);
    }

    return listen_sock;
}

static int getLine(int sock,char line[],int size)
{
    //三种情况的换行
    //\r,\r\n,\n ->\n
    char c = ' ';
    int i = 0;
    while(c != '\n' && i < size-1)
    {
        recv(sock,&c,1,0);    
        if(c == '\r')
        {
            recv(sock, &c, 1, MSG_PEEK);    //MSG_PEEK 窥探下一个字符
            if(c == '\n')   //说明是\r\n
            {
                recv(sock, &c, 1, 0);
            }
            else
            {
                c = '\n';
            }
        }
        
        //\r\n \r -> \n
        line[i++] = c;
    }
    line[i] = '\0';

    return i;
}

static void clearHead(int sock)
{
    char line[MAX];
    do
    {
        getLine(sock, line, sizeof(line));
    }while(strcmp("\n",line) != 0);
}

int echo_www(int sock,char* path,int size)
{
    char line[MAX];
    clearHead(sock);

    int fd = open(path,O_RDONLY);
    if(fd < 0)
    {
        if(DEBUG) 
            printf("%s 打开失败\n",path);
        return 404;
    }

    sprintf(line,"HTTP/1.0 200 OK\r\n");
    send(sock,line,strlen(line),0);
    sprintf(line,"Content-type: text/html;charser=ISO-8859-1\r\n");
    send(sock,line,strlen(line),0);
    sprintf(line,"\r\n");
    send(sock,line,strlen(line),0);

    //发送文件
    sendfile(sock,fd,NULL,size);
    
    close(fd);
    return 200;
}

static void show_404(int sock)
{
    char line[MAX];
    char path[MAX];
    sprintf(line,"HTTP/1.0 404 NOT FOUND\r\n");
    send(sock,line,strlen(line),0);
    sprintf(line,"Content-type: text/html;charser=ISO-8859-1\r\n");
    send(sock,line,strlen(line),0);
    sprintf(line,"\r\n");
    send(sock,line,strlen(line),0);
    
    strcpy(path,"wwwroot/");
    strcat(path,PAGE_404);
    if(DEBUG)
        printf("404 path: %s\n",path);
    
    int fd = open(path,O_RDONLY);
    if(fd > 0)
    {
        if(DEBUG)
            printf("成功打开404文件\n");
    }
    else
    {
        if(DEBUG)
            printf("404文件打开失败\n");
    }
    
    struct stat st;
    stat(path, &st);
    if(DEBUG)
        printf("显示404页面\n");
    sendfile(sock,fd,NULL,st.st_size);

    close(fd);
}

static void echoErrMsg(int sock,int status_code)
{
    switch(status_code)
    {
        case 301:
            break;
        case 302:
            break;
        case 307:
            break;
        case 400:
            break;
        case 403:
            break;
        case 404:
            show_404(sock);
            break;
        case 500:
            break;
        case 503:
            break;
        default:
            break;
    }
}

int exe_cgi(int sock,char* method,char* path,char* query_string)
{
    char line[MAX];
    int content_length = -1;
   
    char method_env[MAX>>5];
    char query_string_env[MAX];
    char content_length_env[MAX>>3];
    if(strcasecmp(method,"GET") == 0)
    {
        clearHead(sock);
    }
    else    //post
    {
        do
        {
            getLine(sock, line, sizeof(line));
            //Content-Length: XXX
            if(strncasecmp(line,"Content-Length: ",16) == 0)
            {
                content_length = atoi(line + 16);
            }
        }while(strcmp(line,"\n"));

        if(content_length == -1)
        {
            return 400;    //客户端的错误
        }
    }
    
    int input[2];
    int output[2];
    pipe(input);
    pipe(output);

    pid_t id = fork();  //子进程处理
    if(id < 0)
    {
        return 500;
    }
    else if(id == 0)
    {
        close(input[1]);    //子进程关闭读管道的写端
        close(output[0]);   //关闭写管道的读端

        //让替换后的进程能与父进程通信
        dup2(input[0],0);   //标准输入从管道读端读
        dup2(output[1],1);  //标准输出往管道写端写
        
        sprintf(method_env,"METHOD=%s",method);
        putenv(method_env);
        
        //用环境变量传递相关参数避免通过管道需要处理的序列化、反序列化的问题

        if(strcasecmp(method,"GET") == 0)   //GET方法的参数在query中
        {
            sprintf(query_string_env,"QUERY_STRING=%s",query_string);
            putenv(query_string_env);
        }
        else    //POST方法的参数在报文的正文部分
        {
            sprintf(content_length_env,"CONTENT_LENGTH=%d",content_length);
            putenv(content_length_env);
        }
        //exex*
        execl(path,path,NULL);
        exit(1);
    }
    else    
    {
        close(input[0]);
        close(output[1]);

        //构建报文
        sprintf(line,"HTTP/1.0 200 OK\r\n");
        send(sock,line,strlen(line),0);
        sprintf(line,"Content-type: text/html;charser=ISO-8859-1\r\n");
        send(sock,line,strlen(line),0);
        sprintf(line,"\r\n");
        send(sock,line,strlen(line),0);
        
        int i = 0;
        char c;
        //POST方法则通过管道把参数交给exec后的进程
        if(strcasecmp(method,"POST") == 0)
        {
            for(; i < content_length;i++)
            {
                recv(sock,&c,1,0);
                write(input[1],&c,1);
            }
        }

        //把CGI处理的结果读取并发回浏览器
        while(read(output[0],&c,1) > 0)
        {
            send(sock, &c, 1, 0);
        }

        waitpid(id,NULL,0);
       
        close(input[1]);
        close(output[0]);
    }
    return 200;    //成功 200
}

void* handlerRequest(void* arg)
{
    int sock = (int)arg;
    char line[MAX];
    char method[MAX>>4];
    char url[MAX];
    char* query_string = NULL;
    char path[MAX];
    int status_code;
    int cgi = 0;
    getLine(sock,line,sizeof(line));    //请求行

    if(DEBUG)
        printf("request line:%s",line);
    //GET /a/b/c%20x=1&&x!=2 HTTP/1.1

    int i = 0;
    int j = 0;
    while(i < sizeof(method)-1 && j < sizeof(line) && !isspace(line[j]))
    {
        method[i] = line[j];
        i++;
        j++;
    }
    method[i] = '\0';
    if(DEBUG) 
        printf("method:%s\n",method);
    while(j < sizeof(line) && isspace(line[j]))
    {
        j++;
    }

    i = 0;

    while(i < sizeof(url) - 1 && j < sizeof(line) && !isspace(line[j]))
    {
        url[i] = line[j];
        i++;
        j++;
    }
    //!!!!!!!!!!!!!!!!!!!!!!!!!!!1
    //这个地方！！！！！！下标一定不要写错！！！！！！
    //不然调死你！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！
    //我当时把i写成了j
    //这也是提醒了我们
    //！！！！尽量不要用i,j之类的做下标
    url[i] = '\0';
    if(DEBUG)
        printf("url:%s\n",url);

    if(strcasecmp("GET",method) == 0)
    {
        ;
    }
    else if(strcasecmp("POST",method) == 0)
    {
        cgi = 1;
    }
    else
    {
        if(DEBUG)
            printf("其他方法\n");
        status_code = 400;
        clearHead(sock);
        goto end;
    }

    //方法已经提取出来
    
    //提取GET方法的可能存在的参数
    if(strcasecmp(method,"GET") == 0)
    {
        query_string = url;
        while(*query_string)
        {
            if(*query_string == '?')
            {
                cgi = 1;
                *query_string = '\0';
                query_string++;
                break;
            }
            query_string++;
        }
    }
    
    // [wwwroot] /a/b/c/    带斜杠的情况都默认返回当前目录下的主页
    sprintf(path,"wwwroot%s",url);
    if(path[strlen(path)-1] == '/')     //定位到最后一个，判断是否为/
    {
        strcat(path,HOME_PAGE);
    }
    if(DEBUG)
       printf("path: %s\n",path);

    struct stat st;
    if(stat(path,&st) < 0)
    {
        if(DEBUG)
            printf("%s 不存在\n",path);
        status_code = 404;
        clearHead(sock);
        goto end;
    }
    else    //如果存在，那么判断是否为文件夹
    {
        if(DEBUG)
            printf("%s 存在\n",path);
        if(S_ISDIR(st.st_mode))    //是目录
        {
            if(DEBUG)
                printf("%s 是目录\n",path);
            strcat(path,"/");
            strcat(path,HOME_PAGE);
        }
        else if((st.st_mode & S_IXUSR) ||\
                (st.st_mode & S_IXGRP) ||\
                (st.st_mode & S_IXOTH))
        {
            cgi = 1;
        }
        else
        {
            ; //do nothing
        }

        //method path cgi get->query_string
        if(cgi)
        {
            if(DEBUG)
                printf("执行cgi\n");
            status_code = exe_cgi(sock,method,path,query_string);
        }
        else
        {
            if(DEBUG)
                printf("显示内容\n");
            status_code = echo_www(sock,path,st.st_size);
        }
    }

end:
    if(status_code != 200)
    {
       echoErrMsg(sock,status_code);
    }
    close(sock);
}



int main(int argc,char* argv[])
{
    if(argc == 1 ||  argc > 3)
    {
        usage(argv[0]);
        return 1;
    }

    if(argc == 3)
        DEBUG = true;
    int listen_sock = startp(atoi(argv[1]));

    for( ; ; )
    {
        struct sockaddr_in client;
        socklen_t len = sizeof(client);
        int sock = accept(listen_sock,(struct sockaddr*)&client,&len);
        if(sock < 0)
        {
            perror("accept");
            continue;
        }

        pthread_t tid;
        pthread_create(&tid,NULL,handlerRequest,(void*)sock);
        pthread_detach(tid);
    }
        
}
