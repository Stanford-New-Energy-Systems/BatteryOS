#include "Sonnen.hpp"

/************************
Constructors/Destructors 
*************************/

SonnenBattery::~SonnenBattery() {
    // check to make sure the battery is back in self_consumption mode
    while (this->manual_mode) {
        this->manual_mode_control("charge", "0");
        this->manual_mode = !(this->enable_self_consumption());
    }

    curl_slist_free_all(this->headers);
}

SonnenBattery::SonnenBattery(const std::string& serial, const std::string& auth_token, double max_charge,
                             double max_discharge, double max_capacity, const std::string& url) {
    this->serial                     = serial;
    this->sonnen_url                 = url;
    this->manual_mode                = false;
    this->auth_token                 = auth_token;
    this->max_capacity_Wh            = max_capacity;
    this->max_charging_current_mA    = max_charge;
    this->max_discharging_current_mA = max_discharge;
    this->set_headers();
}

/*****************
Private Functions
******************/

void SonnenBattery::set_headers() {
    this->headers = curl_slist_append(this->headers, "Accept: application/vnd.sonnenbatterie.api.core.v1+json"); 
    this->headers = curl_slist_append(this->headers, ("Authorization: Bearer " + this->auth_token).c_str());

    return;
}

std::string SonnenBattery::send_request(const std::string& endpoint) {
    std::string curl_response;
    CURL* curl = curl_easy_init(); 

    if (!curl) {
        WARNING() << "curl initializaiton failure" << std::endl;
        return "";
    }
    
    std::string url = this->sonnen_url + this->serial + endpoint;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, SonnenBattery::writeCallback); 
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, this->headers); 
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,  &curl_response); 

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        WARNING() << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        return "";
    }
    
    curl_easy_cleanup(curl); 
    return curl_response;
}

size_t SonnenBattery::writeCallback(char* ptr, size_t size, size_t nmemb, std::string* data) {
    size_t length = size * nmemb;
    data->append(ptr, length);
    return length;
}

bool SonnenBattery::enable_self_consumption() {
    std::string curl_response = this->send_request("/api/setting?EM_OperatingMode=2");

    if (curl_response == "") // error occurred
        return false;
    return true;
}

bool SonnenBattery::enable_manual_mode() {
    std::string curl_response = this->send_request("/api/setting?EM_OperatingMode=1");

    if (curl_response == "") // error occurred
        return false;
    return true;
}

bool SonnenBattery::manual_mode_control(const std::string& mode, const std::string& power_W) {
    std::string endpoint      = "/api/v1/setpoint/" + mode + '/' + power_W; 
    std::string curl_response = this->send_request(endpoint);
    
    if (curl_response == "") // error occurred
        return false;
    return true; 
}

/****************
Public Functions
*****************/

BatteryStatus SonnenBattery::refresh() {
    BatteryStatus status{};
    rapidjson::Document jsonResponse;

    std::string curl_response = this->send_request("/api/v1/status");
    
    if (curl_response == "") // error occurred
        return status;
    
    jsonResponse.Parse(curl_response.c_str());

    auto capacity_mAh = [this] (double usoc, double voltage_V) -> double {
        double SOC = usoc / 100;
        double voltage_mV = voltage_V * 1000;
        return (this->max_capacity_Wh * SOC) / voltage_mV;
    };

    int64_t voltage_V  = jsonResponse["Uac"].GetInt64();
    int64_t usoc       = jsonResponse["USOC"].GetInt64();
    int64_t power_W    = jsonResponse["Pac_total_W"].GetInt64(); 

    status.voltage_mV                   = (double)voltage_V * 1000;
    status.current_mA                   = 1000 * double(power_W) / double(voltage_V);
    status.capacity_mAh                 = capacity_mAh((double)usoc, (double)voltage_V);
    status.max_capacity_mAh             = this->max_capacity_Wh / status.voltage_mV;
    status.max_charging_current_mA      = this->max_charging_current_mA; 
    status.max_discharging_current_mA   = this->max_discharging_current_mA;
    status.time                         = convertToMilliseconds(getTimeNow()); 

    return status;
} 

bool SonnenBattery::set_current(double current_mA) {
    BatteryStatus status = this->refresh();
    if (status.voltage_mV == 0) {
        WARNING() << "Battery is in off grid mode... Cannot execute the command" << std::endl;
        return false;
    }

    if (!this->manual_mode) {
        this->manual_mode = this->enable_manual_mode();
        if (!this->manual_mode)
            return false; 
    }

    std::string mode = current_mA > 0 ? "discharge" : "charge"; 
    double power_W = (status.voltage_mV/1000) * (current_mA/1000);
    
    return this->manual_mode_control(mode, std::to_string(power_W));
}

/**********
C Functions
***********/

void* CreateSonnen(void* args) {
    const char** initArgs = (const char**) args;

    std::string serial   = initArgs[0];
    std::string token    = initArgs[1];
    double max_charge    = atof(initArgs[2]);
    double max_discharge = atof(initArgs[3]);
    double max_capacity  = atof(initArgs[4]);
    std::string url      = initArgs[5];

    return (void *) new SonnenBattery(serial, token, max_charge, max_discharge, max_capacity, url);
}

void DestroySonnen(void* battery) {
    SonnenBattery* bat = (SonnenBattery*) battery;
    delete bat;
}

BatteryStatus SonnenRefresh(void* battery) {
    SonnenBattery* bat = (SonnenBattery*) battery;
    return bat->refresh();
}

bool SonnenSetCurrent(void* battery, double current_mA) {
    SonnenBattery* bat = (SonnenBattery*) battery;
    return bat->set_current(current_mA);
}
