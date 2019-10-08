/*server端*/

#include<stdio.h>
#include<stdlib.h>
#include<netinet/in.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<string.h>
#include<unistd.h>
#include <sys/select.h>
#include <assert.h>

#define BACKLOG 5     //完成三次握手但没有accept的队列的长度
#define CONCURRENT_MAX 8   //应用层同时可以处理的连接
#define SERVER_PORT 11332
#define BUFFER_SIZE 1024
#define QUIT_CMD ".quit"
int client_fds[CONCURRENT_MAX];
char *lines[CONCURRENT_MAX];
void split(char *src,const char *separator,char **dest,int *num);
char *mysubstring(char *srcstr, int start, int end);
char* toString(int iVal);
int main(int argc, const char * argv[])
{
    char input_msg[BUFFER_SIZE];
    char recv_msg[BUFFER_SIZE];
    //本地地址
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr("0.0.0.0");
    bzero(&(server_addr.sin_zero), 8);
    //创建socket
    int server_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_sock_fd == -1)
    {
        perror("socket error");
        return 1;
    }
    //绑定socket
    int bind_result = bind(server_sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if(bind_result == -1)
    {
        perror("bind error");
        return 1;
    }
    //listen
    if(listen(server_sock_fd, BACKLOG) == -1)
    {
        perror("listen error");
        return 1;
    }
    //fd_set
    fd_set server_fd_set;
    int max_fd = -1;
    struct timeval tv;  //超时时间设置
    while(1)
    {
        tv.tv_sec = 20;
        tv.tv_usec = 0;
        FD_ZERO(&server_fd_set);
        FD_SET(STDIN_FILENO, &server_fd_set);
        if(max_fd <STDIN_FILENO)
        {
            max_fd = STDIN_FILENO;
        }
        //printf("STDIN_FILENO=%d\n", STDIN_FILENO);
        //服务器端socket
        FD_SET(server_sock_fd, &server_fd_set);
        // printf("server_sock_fd=%d\n", server_sock_fd);
        if(max_fd < server_sock_fd)
        {
            max_fd = server_sock_fd;
        }
        //客户端连接
        for(int i =0; i < CONCURRENT_MAX; i++)
        {
            //printf("client_fds[%d]=%d\n", i, client_fds[i]);
            if(client_fds[i] != 0)
            {
                FD_SET(client_fds[i], &server_fd_set);
                if(max_fd < client_fds[i])
                {
                    max_fd = client_fds[i];
                }
            }
        }
        int ret = select(max_fd + 1, &server_fd_set, NULL, NULL, &tv);
        if(ret < 0)
        {
            perror("select 出错\n");
            continue;
        }
        else if(ret == 0)
        {
            printf("select 超时\n");
            continue;
        }
        else
        {
            //ret 为未状态发生变化的文件描述符的个数
            if(FD_ISSET(STDIN_FILENO, &server_fd_set))
            {
                printf("发送消息：\n");
                bzero(input_msg, BUFFER_SIZE);
                fgets(input_msg, BUFFER_SIZE, stdin);
                //输入“.quit"则退出服务器
                if(strcmp(input_msg, QUIT_CMD) == 0)
                {
                    exit(0);
                }
                for(int i = 0; i < CONCURRENT_MAX; i++)
                {
                    if(client_fds[i] != 0)
                    {
                        printf("client_fds[%d]=%d\n", i, client_fds[i]);
                        send(client_fds[i], input_msg, BUFFER_SIZE, 0);
                    }
                }
            }
            if(FD_ISSET(server_sock_fd, &server_fd_set))
            {
                //有新的连接请求
                struct sockaddr_in client_address;
                socklen_t address_len;
                int client_sock_fd = accept(server_sock_fd, (struct sockaddr *)&client_address, &address_len);
                printf("new connection client_sock_fd = %d\n", client_sock_fd);
                if(client_sock_fd > 0)
                {
                    int index = -1;
                    for(int i = 0; i < CONCURRENT_MAX; i++)
                    {
                        if(client_fds[i] == 0)
                        {
                            index = i;
                            client_fds[i] = client_sock_fd;
                            break;
                        }
                    }
                    if(index >= 0)
                    {
                        printf("新客户端(%d)加入成功 %s:%d\n", index, inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));
                    }
                    else
                    {
                        bzero(input_msg, BUFFER_SIZE);
                        strcpy(input_msg, "服务器加入的客户端数达到最大值,无法加入!\n");
                        send(client_sock_fd, input_msg, BUFFER_SIZE, 0);
                        printf("客户端连接数达到最大值，新客户端加入失败 %s:%d\n", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));
                    }
                }
            }
            for(int i =0; i < CONCURRENT_MAX; i++)
            {
                if(client_fds[i] !=0)
                {
                    if(FD_ISSET(client_fds[i], &server_fd_set))
                    {
                        //处理某个客户端过来的消息
                        bzero(recv_msg, BUFFER_SIZE);
                        long byte_num = recv(client_fds[i], recv_msg, BUFFER_SIZE, 0);

                        if (byte_num > 0)
                        {
                            if(byte_num > BUFFER_SIZE)
                            {
                                byte_num = BUFFER_SIZE;
                            }
                            //recv_msg[byte_num] = '\0';
                            printf("客户端(%d):%s", i, recv_msg);
                            //用来接收返回数据的数组。这里的数组元素只要设置的比分割后的子字符串个数大就好了。
                            char *revbuf[BUFFER_SIZE] = {0};
                            //分割后子字符串的个数
                            int num = 0;
                            split(recv_msg, " ", revbuf, &num);
                            char *tmp = mysubstring(revbuf[0], 0, 1);
                            char *tmp2 = mysubstring(revbuf[0], 1, 1024);
                            char *tmp_fd = toString(client_fds[i]);
                            //printf("%s:%s\n", tmp, tmp2);
                            /*转发数据给其他的客户端*/
                            for(int j = 0; j < CONCURRENT_MAX; j++)
                            {
                                if(client_fds[j] != 0)
                                {

                                    if(strcmp(tmp, "@") == 0)
                                    {
                                        send(client_fds[atoi(tmp2)], revbuf[1], sizeof(recv_msg), 0);
                                        break;
                                    }
                                    else
                                    {
                                        send(client_fds[j], recv_msg, sizeof(recv_msg), 0);
                                    }

                                }
                            }
                            /*结束转发内容*/
                        }
                        else if(byte_num < 0)
                        {
                            printf("从客户端(%d)接受消息出错.\n", i);
                        }
                        else
                        {
                            FD_CLR(client_fds[i], &server_fd_set);
                            client_fds[i] = 0;
                            printf("客户端(%d)退出了\n", i);
                        }
                    }
                }
            }
        }
    }
    return 0;
}

char *mysubstring(char *srcstr, int start, int end)
{
    assert(start >= 0);
    assert(srcstr != NULL);
    assert(end >= start);

    int total_length = strlen(srcstr);//首先获取srcstr的长度
    //判断srcstr的长度减去需要截取的substr开始位置之后，剩下的长度
    //是否大于指定的长度length，如果大于，就可以取长度为length的子串
    //否则就把从开始位置剩下的字符串全部返回
    int real_length = (end > total_length)?total_length : end;
    char  *tmp;
    if(NULL == (tmp=(char *)malloc(real_length * sizeof(char))))
    {
        printf("Memory overflow.\n");
        exit(0);
    }
    strncpy(tmp, srcstr+start, real_length );

    return tmp;
}

void split(char *src,const char *separator,char **dest,int *num) {
    char *pNext;
    int count = 0;
    if (src == NULL || strlen(src) == 0)
        return;
    if (separator == NULL || strlen(separator) == 0)
        return;
    pNext = strtok(src,separator);
    while(pNext != NULL) {
        *dest++ = pNext;
        ++count;
        pNext = strtok(NULL,separator);
    }
    *num = count;
}

char* toString(int iVal)
{
    /* 优化点:
      *  1,不通过堆栈的方式,无需进出栈,或者是对字符串进行逆序,直接通过指针运算,内存拷贝方式或者最终结果
    */
    char str[1024] = {'\0',};
    char *pos = NULL;
    int sign = 0;   //正数 或者是 0

    int abs = iVal;

    pos = str + 1023; //移动指针,指向堆栈底部
    *pos-- = '\0';  //end

    if(iVal < 0)
    {
        sign = 1;
        abs = -abs;
    }

    int dit = 0;
    while(abs > 0)
    {
        dit = abs % 10;
        abs = abs / 10;

        *pos-- = (char)('0' + dit);
    }

    if(sign)
        *pos-- = '-';

    char *ret = (char*)malloc(1024 - (pos - str));

    if(iVal == 0)               //0的一个处理
        strcpy(ret, "0");
    else                        //iVal非0的拷贝
        strcpy(ret, pos+1);

    return(ret);
}

int toInt(char *str)
{
    int  iTemp = atoi(str);

    return iTemp;
}