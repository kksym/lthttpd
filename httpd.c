
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <lthread.h>
#include <unistd.h>
#include <arpa/inet.h>

typedef struct client_t {
    struct sockaddr_in addr;
    int fd;
} client_t;

static int port = 80;
static int verbose = 0;
static char fdir[127] = "./files";

char reply[] = "HTTP/1.0 200 OK\r\nContent-length: 5\r\n\r\nPong!\r\n";

void buildresponse(char* dst, char* src, size_t len)
{
    sprintf(dst, "HTTP/1.0 200 OK\r\nContent-length: %u\r\n\r\n%s\r\n", len, src);
}

int parse_request(char* req, char* type, char* path, char* httpvers, char* host)
{
    // TODO: de-uglify this
    return sscanf(req, "%15[^ ] %255[^ ] %31[^\r\n] Host: %255[^\r\n]", type, path, httpvers, host) == 4;
}

void serve(lthread_t* lt, void* arg)
{
    lthread_detach();
    client_t* client = (client_t*)arg;
    char ibuf[1024];
    char obuf[1024];
    size_t flen;

    char type[16];
    char path[128];
    char httpvers[32];
    char host[256];

    int ret = 0;

    char ipstr[INET6_ADDRSTRLEN];

    inet_ntop(AF_INET, &client->addr.sin_addr, ipstr, INET_ADDRSTRLEN);

    ret = lthread_recv(client->fd, ibuf, 1024, 0, 5000);


    if (parse_request(ibuf, type, path, httpvers, host))
    {
        printf("%s | %s %s\n", ipstr, type, path);
        char fullp[256];
        sprintf(fullp, "%s%s", fdir, path);
        FILE* file = fopen(fullp, "r");
        if (!file)
        {
            printf("problem serving %s\n", path);
        }
        else
        {
            fseek(file, 0, SEEK_END);
            flen = ftell(file);
            fseek(file, 0, SEEK_SET);
            char* recvbuf = calloc(flen, 1);
            char* ptr = recvbuf;
            while (!feof(file))
            {
                *ptr = fgetc(file);
                ptr++;

            }
            buildresponse(obuf, recvbuf, flen);
            puts(obuf);
            printf("obuf[%d] [%d]\n", strlen(obuf), flen);
        }
    }

    if (ret != -2)
        lthread_send(client->fd, obuf, strlen(obuf) - 1, 0);


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
        perror("failed to set SO_REUSEADDR on socket");

    struct sockaddr_in sin;
    sin.sin_family = PF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(port);

    int ret = bind(listenfd, (struct sockaddr*)&sin, sizeof(sin));
    if (ret == -1)
    {
        printf("bind to port %d failed\n", port);
        return;
    }

    listen(listenfd, 128);

    printf("listening on port %d\n", port);

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
    int c = 0;
    opterr = 0;

    while ((c = getopt(argc, argv, "vp:d:")) != -1)
    {
        switch (c)
        {
            case 'v':
                verbose = 1;
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 'd':
                strncpy(fdir, optarg, 127);
                break;
        }
    }

    lthread_t* lt;
    lthread_create(&lt, listener, NULL);

    lthread_run();

    return 0;
}

