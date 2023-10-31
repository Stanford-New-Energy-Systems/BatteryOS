#ifndef AGG_HPP
#define AGG_HPP

#include "Acceptor.hpp"
#include "NetService.hpp"
#include "BatteryConnection.hpp"
#include "ProtoParameters.hpp"
#include "util.hpp"

#include <stdint.h>

extern "C" {
    void* new_aggregator(
        uint32_t id,
        uint32_t schedule_length,
        char* verify_key_ptr,
        uint32_t n_clients,
        uint32_t max_v,
        uint32_t max_e
    );

    void aggregate(
        void* agg_ptr,
        const char* input_share,
        uint32_t input_share_len,
        uint32_t client_num,
        void* user,
        void (*write_callback)(char*, uint32_t, uint32_t, void*)
    );

    void agg_finish_round(
        void* agg,
        const char* input_share,
        uint32_t input_share_len,
        void* user,
        void (*write_callback)(char*, uint32_t, void*)
    );

    void send_agg_share(
        void* agg_ptr,
        void* user,
        void (*write_callback)(char*, uint32_t, void*)
    );


}

class Aggregator {
    void* rust_agg;
    std::shared_ptr<BatteryConnection> connection;
    std::shared_ptr<BatteryConnection> client_connection;
    NetService* servicer;
    uint32_t client_num = 0;
    

    public:
        int client_port, agg_port;
        std::shared_ptr<Acceptor> aggListener;
        std::shared_ptr<Acceptor> clientListener;
        Aggregator(int addr, int client_port, int agg_port, char* verify_key, NetService* servicer);

    private:
};

#endif
