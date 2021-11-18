#ifndef SONNEN_HPP
#define SONNEN_HPP

#include "BatteryInterface.hpp"
#include <curl/curl.h>
#include <rapidjson/document.h>

class Sonnen : public PhysicalBattery {
    int64_t max_capacity_Wh;
    int64_t max_discharging_current_mA; 
    int64_t max_charging_current_mA; 
    int serial;
public: 
    static size_t curl_write_function(void *ptr, size_t size, size_t nmemb, std::string* data) {
        data->append((char*) ptr, size * nmemb);
        return size * nmemb;
    }
public: 
    Sonnen(
        const std::string &name, 
        int serial, 
        int64_t max_capacity_Wh, 
        int64_t max_discharging_current_mA, 
        int64_t max_charging_current_mA, 
        std::chrono::milliseconds max_staleness
    ) : PhysicalBattery(name, max_staleness),
        max_capacity_Wh(max_capacity_Wh), 
        max_discharging_current_mA(max_discharging_current_mA), 
        max_charging_current_mA(max_charging_current_mA), 
        serial(serial) 
    {
        this->refresh();
    }

    std::string get_type_string() override {
        return "SonnenBattery";
    }

    std::chrono::milliseconds get_delay(int64_t from_current, int64_t to_current) override {
        if (from_current == to_current) { return std::chrono::milliseconds(0); }
        else if (from_current == 0 && to_current != 0) { return std::chrono::milliseconds(30000); }
        else { return std::chrono::milliseconds(5000); }
    }

    std::string send_request(const std::string &endpoint) {
        CURL* curl = curl_easy_init();
        if (!curl) {
            WARNING() << "curl initialization failure" << std::endl;
            return "";
        }
        CURLcode res;
        std::string url = std::string("https://core-api.sonnenbatterie.de/proxy/")+std::to_string(serial)+endpoint;

        // the actual code has the actual url string in place of <my_url>
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        struct curl_slist* headers = NULL;
        /*
            self.headers = {
                'Accept': 'application/vnd.sonnenbatterie.api.core.v1+json',
                'Authorization': 'Bearer ' + self.token, 
            }
        */
        // headers = curl_slist_append(headers, "Content-Type: application/json");
        std::string header_accept = "Accept: application/vnd.sonnenbatterie.api.core.v1+json";
        headers = curl_slist_append(headers, header_accept.c_str());
        std::string header_bearer = std::string("Authorization: Bearer ") + std::getenv("SONNEN_TOKEN"); 

        // the actual code has the actual token in place of <my_token>
        headers = curl_slist_append(headers, header_bearer.c_str());

        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers); 

        std::string response_string;
        std::string header_string;
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Sonnen::curl_write_function);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, &header_string);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            WARNING() << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        }
        curl_easy_cleanup(curl);
        return response_string;
    }

    BatteryStatus refresh() override {
        std::string response_string = this->send_request("/api/v1/status");
        // LOG() << response_string;

        rapidjson::Document json_resp; 
        json_resp.Parse(response_string.c_str());
        int64_t uac = json_resp["Uac"].GetInt64();
        int64_t pac_total_w = json_resp["Pac_total_W"].GetInt64();
        int64_t usoc = json_resp["USOC"].GetInt64();
        // std::string sonnen_time(json_resp["Timestamp"].GetString());
        std::tm ts{};
        strptime(json_resp["Timestamp"].GetString(), "%Y-%m-%d %H:%M:%S", &ts);
        ts.tm_hour -= 3;  // EST --> PST (localtime)
        timepoint_t tp = std::chrono::system_clock::from_time_t(std::mktime(&ts));

        this->status.voltage_mV = uac * 1000;
        this->status.current_mA = round((double)pac_total_w/(double)uac*1000);
        this->status.capacity_mAh = round((double)max_capacity_Wh * (double)usoc/100 / (double)uac * 1000); 
        this->status.max_capacity_mAh = round((double)max_capacity_Wh / (double)uac * 1000);
        this->status.max_charging_current_mA = this->max_charging_current_mA;
        this->status.max_discharging_current_mA = this->max_discharging_current_mA;
        this->status.timestamp = timepoint_to_c_time(tp);
        
        return this->status;
    }

    uint32_t set_current(int64_t target_current_mA, bool is_greater_than_target, void *other_data) override {
        LOG() << "set_current: " << target_current_mA << std::endl;

        LOG() << "now is (secs since epoch)" << 
            std::chrono::duration_cast<std::chrono::seconds>(get_system_time().time_since_epoch()).count() << std::endl;

        std::string response_string = this->send_request(
            "/api/v1/setpoint/"+
            ((target_current_mA > 0) ? std::string("discharge/") : std::string("charge/"))+
            std::to_string(std::abs((target_current_mA/1000) * (this->status.voltage_mV/1000)))
        );
        std::cout << response_string << std::endl;
        // rapidjson::Document json_resp; 
        // json_resp.Parse(response_string.c_str()); 
        int64_t retcode = 1;
        // LOG() << retcode << std::endl;
        return (uint32_t)retcode;
    }
};

#endif // ! SONNEN_HPP 

