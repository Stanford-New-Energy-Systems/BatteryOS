#ifndef SONNEN_HPP
#define SONNEN_HPP

#include <curl/curl.h>
#include <rapidjson/document.h>

#include <vector>
#include <thread>
#include <stdlib.h>
#include "util.hpp"
#include "BatteryStatus.hpp"

class SonnenBattery {
    private:
        bool manual_mode;
        std::string serial; 
        std::string auth_token;
        std::string sonnen_url;
        double max_capacity_Wh;
        struct curl_slist* headers;
        double max_charging_current_mA;
        double max_discharging_current_mA;

    public:
        ~SonnenBattery();
        SonnenBattery(const std::string& serial, const std::string& auth_token, double max_charge,
                      double max_discharge, double max_capcity, const std::string& url = "https://core-api.sonnenbatterie.de/proxy/");
         
    private:
        void set_headers();
        bool enable_manual_mode();
        bool enable_self_consumption();
        bool manual_mode_control(const std::string& mode, const std::string& value);
        std::string send_request(const std::string& endpoint);
        static size_t writeCallback(char *ptr, size_t size, size_t nmemb, std::string* data);

    public:
        BatteryStatus refresh(); 
        bool set_current(double current_mA);
};

extern "C" void* CreateSonnen(void* args);
extern "C" void DestroySonnen(void* battery);
extern "C" BatteryStatus SonnenRefresh(void* battery);
extern "C" bool SonnenSetCurrent(void* battery, double current_mA);

#endif
