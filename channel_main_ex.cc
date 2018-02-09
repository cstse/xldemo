#include "base/clock.h"
#include "base/context.h"
#include "common/settings.h"
#include "channel_manager.h"
#include "channel_observer.h"
#include <inttypes.h>
#include <iostream>
#include <atomic>
#include <chrono>
#include <thread>
#include <map>

// configuration
static std::string g_local_peerid; // -pid
static std::string g_dst_peerid; // -dst
static bool g_server_mode = false; // -s
static int g_channel_count = 1; // -c
static int g_block_size = 16*1024; // -b
static int g_direction = 1; // -push = 1, -pull = 2, -bidir = 3
static int g_duration = 60; // -t second(s)

static int64_t g_start_tick;

static std::atomic_bool g_stop_flag;

static std::map<uint64_t, std::shared_ptr<xcloud::Channel>> g_channel_map;

static std::shared_ptr<xcloud::ChannelManager> g_channel_mgr;
static std::shared_ptr<xcloud::ChannelAcceptor> g_channel_acceptor;
static std::shared_ptr<xcloud::Router> g_router;
static std::shared_ptr<xcloud::Context> g_worker;

static void ChannelSendData(std::shared_ptr<xcloud::Channel> channel)
{
    auto resp = std::make_shared<std::string>("dududu...");
    resp->append(std::to_string(channel->Id()));
    resp->resize(g_block_size);
    while(1)
    {
        if(channel->Send(resp) != 0) break;
    }
}

static void ShowStats()
{
    if (g_channel_map.size() == 0)
        return;

    uint64_t total_speed_in_data = 0;
    uint64_t total_speed_out_data = 0;
    uint64_t total_speed_in_proto = 0;
    uint64_t total_speed_out_proto = 0;

    uint32_t min_rtt = -1;
    uint32_t max_rtt = 0;
    uint32_t sum_rtt = 0;

    uint32_t min_srtt = -1;
    uint32_t max_srtt = 0;
    uint32_t sum_srtt = 0;

    for (auto& c : g_channel_map)
    {
        auto ch = c.second;

        auto rtt = ch->Rtt();
        sum_rtt += rtt;
        if (rtt < min_rtt)
            min_rtt = rtt;
        if (rtt > max_rtt)
            max_rtt = rtt;

        auto srtt = ch->Srtt();
        sum_srtt += srtt;
        if (srtt < min_srtt)
            min_srtt = srtt;
        if (srtt > max_srtt)
            max_srtt = srtt;

        printf(
            "channel: %" PRIu64 ", "
            "in_data: %" PRIu64 ", out_data: %" PRIu64 ", "
            "in_proto: %" PRIu64 ", out_proto: %" PRIu64 ", "
            "rtt: %u, srtt: %u"
            "\n"
            , ch->Id()
            , ch->DataInSpeed(), ch->DataOutSpeed()
            , ch->ProtoInSpeed(), ch->ProtoOutSpeed()
            , rtt, srtt
        );

        total_speed_in_data += ch->DataInSpeed();
        total_speed_out_data += ch->DataOutSpeed();
        total_speed_in_proto += ch->ProtoInSpeed();
        total_speed_out_proto += ch->ProtoOutSpeed();

    }

    printf("-----------------------------\n");
    printf("total_speed_in_data: %" PRIu64 "\n", total_speed_in_data);
    printf("total_speed_out_data: %" PRIu64 "\n", total_speed_out_data);
    printf("total_speed_in_proto: %" PRIu64 "\n", total_speed_in_proto);
    printf("total_speed_out_proto: %" PRIu64 "\n", total_speed_out_proto);
    printf("avg_rtt: %u, avg_srtt: %u\n"
        , sum_rtt / g_channel_map.size(), sum_srtt / g_channel_map.size());
    printf("-----------------------------\n");
}

static void CheckTermination()
{
    auto now = xcloud::Clock::NowTicks();
    if (now - g_start_tick >= g_duration * 1000)
    {

        if (g_channel_acceptor)
        {
            g_channel_acceptor->Close();
            g_channel_acceptor = nullptr;
        }

        for (auto& c : g_channel_map)
            c.second->Close();

        g_channel_map.clear();

        if (g_channel_mgr)
        {
            std::string stats = g_channel_mgr->GetStats();
            printf("%s\n", stats.c_str());
            g_channel_mgr->UnInit();
            g_channel_mgr = nullptr;
        }

        if (g_router)
        {
            g_router->UnInit();
            g_router = nullptr;
        }

        g_stop_flag = true;
    }
}

struct DemoChannelObserver : public xcloud::ChannelObserver
{
    virtual void OnError(std::shared_ptr<xcloud::Channel> channel, int32_t errcode) override
    {
        printf("channel %" PRIu64 " error: %d\n", channel->Id(), errcode);
        std::string stats = g_channel_mgr->GetStats();
        printf("%s\n", stats.c_str());
        channel->Close();
        g_channel_map.erase(channel->Id());
    }

    virtual void OnChannelRecvData(std::shared_ptr<xcloud::Channel> channel
        , std::shared_ptr<std::string> data) override
    {
        if (*data == "pull")
        {
            printf("channel %" PRIu64 " requesting data...\n", channel->Id());
            ChannelSendData(channel);
        }
    }

    virtual void Writable(std::shared_ptr<xcloud::Channel> channel) override
    {
        ChannelSendData(channel);
    }
};

struct DemoAcceptorObserver : public xcloud::AcceptorObserver
{
public:

    virtual void OnError(std::shared_ptr<xcloud::ChannelAcceptor> acceptor, int32_t errcode) override
    {
        printf("acceptor %hu error: %d\n", acceptor->Vport(), errcode);
    }

    virtual void OnAcceptChannel(std::shared_ptr<xcloud::ChannelAcceptor> acceptor
                          , std::shared_ptr<xcloud::Channel> channel) override
    {
        printf("new channel %" PRIu64 "\n", channel->Id());
        g_channel_map.insert(std::make_pair(channel->Id(), channel));
        auto channel_observer = std::make_shared<DemoChannelObserver>();
        channel->SetObserver(channel_observer);
    }
};

static void Run()
{
    printf("start running ...\n");
    
    g_router = std::make_shared<xcloud::Router>();
    g_router->Init(g_local_peerid);

    g_channel_mgr = std::make_shared<xcloud::ChannelManager>(g_router, g_worker);
    g_channel_mgr->Init();

    auto acceptor_observer = std::make_shared<DemoAcceptorObserver>();
    g_channel_acceptor = g_channel_mgr->NewAcceptor();
    g_channel_acceptor->SetObserver(acceptor_observer);
    g_channel_acceptor->Open();

    g_start_tick = xcloud::Clock::NowTicks();

    for(int c = 0; c < g_channel_count; ++c)
    {
        auto channel = g_channel_mgr->NewChannel(g_dst_peerid, 0);
        g_channel_map.insert(std::make_pair(channel->Id(), channel));
        auto channel_observer = std::make_shared<DemoChannelObserver>();
        channel->SetObserver(channel_observer);
        channel->Open();
        if (g_direction == 1)
        {
            ChannelSendData(channel);
        }
        else if(g_direction == 2)
        {
            auto req = std::make_shared<std::string>("pull");
            channel->Send(req);
        }
        else if(g_direction == 3)
        {
            auto req = std::make_shared<std::string>("pull");
            channel->Send(req);
            ChannelSendData(channel);
        }
    }
}

static void ShowUsage(const char* exec)
{
    printf("%s", exec);
    printf(" [-s]");
    printf(" -pid <local peerid>");
    printf(" -dst <dest peerid>");
    printf(" [-c <channel count>]");
    printf(" [-b <block size>]");
    printf(" [-d <direction>]");
    printf(" [-t <duration(seconds)>]");
    printf("\n");
}

int main(int argc, char** argv)
{
    int i = 0;
    while(i < argc)
    {
        if (strcmp(argv[i], "-pid") == 0)
        {
            if (i == argc -  1) break;
            g_local_peerid = argv[++i];
            printf("parsed local peerid: %s\n", g_local_peerid.c_str());
        }
        else if (strcmp(argv[i], "-s") == 0)
        {
            g_server_mode = true;
            printf("server mode\n");
        }
        else if (strcmp(argv[i], "-dst") == 0)
        {
            if (i == argc -  1) break;
            g_dst_peerid = argv[++i];
            printf("destination: %s\n", g_dst_peerid.c_str());
        }
        else if (strcmp(argv[i], "-c") == 0)
        {
            if (i == argc -  1) break;
            g_channel_count = atoi(argv[++i]);
            printf("channel count: %d\n", g_channel_count);
        }
        else if (strcmp(argv[i], "-b") == 0)
        {
            if (i == argc -  1) break;
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

    if (g_local_peerid.size() == 0)
    {
        ShowUsage(argv[0]);
        return 1;
    }

    if (g_server_mode)
    {
        g_channel_count = 0;
    }
    else
    {
        if (g_dst_peerid.size() == 0)
        {
            ShowUsage(argv[0]);
            return 1;
        }

        if (g_channel_count <= 0)
        {
            ShowUsage(argv[0]);
            return 1;
        }

        if (g_block_size <= 0)
        {
            ShowUsage(argv[0]);
            return 1;
        }

        if (g_direction < 1 || g_direction > 3)
        {
            ShowUsage(argv[0]);
            return 1;
        }
    }

    xcloud::Settings::GetInstance().Init("./");
    Json::Value cfg = xcloud::Settings::GetInstance().Load(
        "logger", "config", Json::Value("log4cplus.cfg"));
    auto& root = xcloud::Settings::GetInstance().GetRootDir();
    xcloud::XLogger::Init(root + cfg.asString());

    g_worker = std::make_shared<xcloud::Context>();
    g_worker->Start();
    g_worker->Post(Run);

    g_stop_flag = false;

    while(!g_stop_flag)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        g_worker->Post(ShowStats);
        if (!g_server_mode)
            g_worker->Post(CheckTermination);
    }

    g_worker->Stop();
    g_worker = nullptr;

    return 0;
}