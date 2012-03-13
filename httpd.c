
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <lthread.h>

// https://github.com/halayli/lthread

#define PORTNO 5002

typedef struct client_t {
    struct sockaddr_in addr;
    int fd;
} client_t;

char reply[] = "HTTP/1.0 200 OK\r\nContent-length: 5\r\n\r\nPong!\r\n";

void serve(lthread_t* lt, void* arg)
{
    lthread_detach();
    client_t* client = (client_t*)arg;
    char buf[1024];
    int ret = 0;

    char ipstr[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET, &client->addr.sin_addr, ipstr, INET_ADDRSTRLEN);

    ret = lthread_recv(client->fd, buf, 1024, 0, 5000);

    if (ret != -2)
        lthread_send(client->fd, reply, strlen(reply), 0);

    lthread_close(client->fd);
    free(arg);
}

void listener(lthread_t* lt, void* arg)
{
    lthread_detach();
    DEFINE_LTHREAD;

    int listenfd = lthread_socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenfd < 0)
        return;

    int opt = 1;
    
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int)) == -1)
        perror("failed to set SOREUSEADDR on socket");

    struct sockaddr_in sin;
    sin.sin_family = PF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(PORTNO);

    int ret = bind(listenfd, (struct sockaddr*)&sin, sizeof(sin));
    if (ret == -1)
    {
        printf("bind to port %d failed\n", PORTNO);
        return;
    }

    listen(listenfd, 128);

    printf("listening on port %d\n", PORTNO);

    lthread_t* c_thread = NULL;

    while (1)
    {
        client_t* client = malloc(sizeof(client_t));
        client->fd = 0;
        socklen_t addrlen = sizeof(client->addr);
        
        client->fd = lthread_accept(listenfd, (struct sockaddr*)&client->addr, &addrlen);
        if (client->fd == -1)
        {
            perror("failed to accept client");
            return;
        }
        ret = lthread_create(&c_thread, serve, client);
    }
}

int main(int argc, char** argv)
{
    lthread_t* lt;
    lthread_create(&lt, listener, NULL);

    lthread_run();

    lthread_destroy(lt);

    return 0;
}

