#include "xsdn_channel_capi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>
#ifdef _WIN32
#include <Windows.h>
#define xc_sleep(ms) Sleep(ms)
#else
#include <unistd.h>
#define xc_sleep(ms) usleep((ms)*1000)
#endif

// configuration
static char g_local_peerid[20]; // -pid
static char g_dst_peerid[20]; // -dst
static int g_server_mode = 0; // -s
static int g_block_size = 16 * 1024; // -b
static int g_direction = 1; // -push = 1, -pull = 2, -bidir = 3
static int g_duration = 30; // -t second(s)

static int g_channel_error = 0;

static xc_channel_t g_channel_handle = XSDN_CHANNEL_INVALID_HANDLE;
static xc_acceptor_t g_acceptor_handle = XSDN_CHANNEL_INVALID_HANDLE;

static void show_usage(const char* exec)
{
    printf("%s", exec);
    printf(" [-s]");
    printf(" -pid <local peerid>");
    printf(" -dst <dest peerid>");
    printf(" [-b <block size>]");
    printf(" [-d <direction>]");
    printf(" [-t <duration(seconds)>]");
    printf("\n");
}

static void parse_args(int argc, char** argv)
{
    int i = 0;
    while (i < argc)
    {
        if (strcmp(argv[i], "-pid") == 0)
        {
            if (i == argc - 1) break;
            ++i;
            strcpy(g_local_peerid, argv[i]);
            printf("parsed local peerid: %s\n", g_local_peerid);
        }
        else if (strcmp(argv[i], "-s") == 0)
        {
            g_server_mode = 1;
            printf("server mode\n");
        }
        else if (strcmp(argv[i], "-dst") == 0)
        {
            if (i == argc - 1) break;
            ++i;
            strcpy(g_dst_peerid, argv[i]);
            printf("destination: %s\n", g_dst_peerid);
        }
        else if (strcmp(argv[i], "-b") == 0)
        {
            if (i == argc - 1) break;
            g_block_size = atoi(argv[++i]);
            printf("block size: %d\n", g_block_size);
        }
        else if (strcmp(argv[i], "-push") == 0)
        {
            g_direction = 1;
            printf("push direction ->\n");
        }
        else if (strcmp(argv[i], "-pull") == 0)
        {
            g_direction = 2;
            printf("pull direction <-\n");
        }
        else if (strcmp(argv[i], "-bidir") == 0)
        {
            g_direction = 3;
            printf("bidirection <->\n");
        }
        else if (strcmp(argv[i], "-t") == 0)
        {
            g_duration = atoi(argv[++i]);
            printf("duration: %d(s)\n", g_duration);
        }
        ++i;
    }
}

static int check_args(const char* prog)
{
    if (strlen(g_local_peerid) == 0)
    {
        show_usage(prog);
        return 1;
    }
    if(!g_server_mode)
    {
        if (strlen(g_dst_peerid) == 0)
        {
            show_usage(prog);
            return 1;
        }
        if (g_block_size <= 0)
        {
            show_usage(prog);
            return 1;
        }

        if (g_direction < 1 || g_direction > 3)
        {
            show_usage(prog);
            return 1;
        }
    }
    return 0;
}

static void send_to_channel(xc_channel_t c)
{
    int ret;
    char* data = (char*)malloc(16 * 1024);
    memcpy(data, "dududu....", strlen("dududu...."));
    for (;;)
    {
        ret = xc_channel_send(c, data, 16 * 1024);
        if (ret != 0)
            break;
    }
    free(data);
}

static void acceptor_error_cb(xc_acceptor_t self, int errcode)
{
    fprintf(stderr, "acceptor_error_cb: %d\n", errcode);
    xc_close_acceptor(self);
    exit(1);
}

static void acceptor_on_channel_cb(xc_acceptor_t self, xc_channel_t new_channel)
{
    if (xc_handle_is_valid(g_channel_handle))
    {
        fprintf(stderr, "server busy...\n");
        xc_channel_send(new_channel, "server busy...", strlen("server busy..."));
        xc_close_channel(new_channel);
        return;
    }
    char pid[20];
    int len = 20;
    xc_channel_dst_peerid(new_channel, pid, &len);
    pid[len] = 0;
    printf("new channel %" PRIu64 " from %s ...\n", xc_channel_id(new_channel), pid);
    g_channel_handle = new_channel;
}

static void channel_on_error(xc_channel_t self, int errcode)
{
    fprintf(stderr, "channel error: %d\n", errcode);
    assert(self == g_channel_handle);
    g_channel_error = errcode;
}

static void channel_on_recv(xc_channel_t self, const char* data, int data_len)
{
    if (memcmp(data, "pull", 4) == 0)
    {
        send_to_channel(self);
    }
}

static void channel_on_writable(xc_channel_t self)
{
    send_to_channel(self);
}


static void print_status(int c)
{
    if (!xc_handle_is_valid(g_channel_handle))
        return;

    printf("%d:"
        "channel: %" PRIu64 ", "
        "in_data: %" PRIu64 ", out_data: %" PRIu64 ", "
        "in_proto: %" PRIu64 ", out_proto: %" PRIu64 ", "
        "rtt: %u, srtt: %u"
        "\n"
        , c
        , xc_channel_id(g_channel_handle)
        , xc_channel_data_in_speed(g_channel_handle)
        , xc_channel_data_out_speed(g_channel_handle)
        , xc_channel_proto_in_speed(g_channel_handle)
        , xc_channel_proto_out_speed(g_channel_handle)
        , xc_channel_rtt(g_channel_handle)
        , xc_channel_srtt(g_channel_handle)
    );
}

static void close_channel()
{
    if (!xc_handle_is_valid(g_channel_handle))
        return;
    xc_close_channel(g_channel_handle);
    g_channel_handle = XSDN_CHANNEL_INVALID_HANDLE;
}

static void close_acceptor()
{
    if (!xc_handle_is_valid(g_acceptor_handle))
        return;
    xc_close_acceptor(g_acceptor_handle);
    g_acceptor_handle = XSDN_CHANNEL_INVALID_HANDLE;
}

static void run_server()
{
    g_acceptor_handle = xc_open_acceptor(0, acceptor_error_cb, acceptor_on_channel_cb,
        channel_on_error, channel_on_recv, channel_on_writable);
    if (!xc_handle_is_valid(g_acceptor_handle))
    {
        fprintf(stderr, "xc_new_acceptor error!\n");
        exit(1);
    }
    int c = 0;
    for (;;)
    {
        xc_sleep(1000);
        if (g_channel_error)
        {
            close_channel();
            g_channel_error = 0;
        }
        print_status(++c);
    }
}

static void run_client()
{
    g_channel_handle = xc_open_channel(g_dst_peerid, strlen(g_dst_peerid), 0,
        channel_on_error, channel_on_recv, channel_on_writable);
    if (!xc_handle_is_valid(g_channel_handle))
    {
        fprintf(stderr, "xc_new_channel error!\n");
        exit(1);
    }
    switch (g_direction)
    {
    case 1: // push
        send_to_channel(g_channel_handle);
        break;
    case 2: // pull
        xc_channel_send(g_channel_handle, "pull", 4);
        break;
    case 3: // pull & push
        xc_channel_send(g_channel_handle, "pull", 4);
        send_to_channel(g_channel_handle);
        break;
    default:
        fprintf(stderr, "flow direction error\n");
        exit(1);
    }
    int c = 0;
    for (;;)
    {
        if (++c > g_duration)
        {
            close_channel();
            return;
        }
        xc_sleep(1000);
        if (g_channel_error)
        {
            close_channel();
            g_channel_error = 0;
        }
        print_status(c);
    }
}

int main(int argc, char** argv)
{
    parse_args(argc, argv);

    if (check_args(argv[0]))
    {
        return 1;
    }

    if (xc_init("./", g_local_peerid, (int)strlen(g_local_peerid)) != 0)
    {
        fprintf(stderr, "xc_init error\n");
        return 1;
    }

    if (g_server_mode)
    {
        run_server();
    }
    else
    {
        run_client();
    }

    close_acceptor();

    xc_uninit();
}
