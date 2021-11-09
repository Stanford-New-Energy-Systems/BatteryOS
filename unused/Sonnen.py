import datetime
import requests
import time
import os
import typing as T

sonnen_token =  os.environ.get("SONNEN_TOKEN")
url_initial = "https://core-api.sonnenbatterie.de/proxy/"
headers = {
    'Accept': 'application/vnd.sonnenbatterie.api.core.v1+json',
    'Authorization': 'Bearer ' + sonnen_token, 
}
class Sonnen:
    def __init__(self, serial: str, auth_token: str = sonnen_token): 
        if sonnen_token is None: 
            print("please define your SONNEN_TOKEN in the environment variable", file=os.sys.stderr)
        self.serial = serial
        self.token = auth_token
        self.url_initial = "https://core-api.sonnenbatterie.de/proxy/"
        self.headers = {
            'Accept': 'application/vnd.sonnenbatterie.api.core.v1+json',
            'Authorization': 'Bearer ' + self.token, 
        }
        self.MDC_mA = 30000
        self.MCC_mA = 30000
        self.status = None
        self.get_internal_status()

    def get_internal_status(self): 
        status_endpoint = '/api/v1/status'
        try:
            resp = requests.get(self.url_initial + self.serial + status_endpoint, headers=self.headers)
            resp.raise_for_status()
            status = resp.json()
            self.status = status
        except requests.exceptions.HTTPError as err:
            print(err)
        return self.status
    
    def enable_backup_mode(self):
        backup_endpoint = '/api/setting?EM_OperatingMode=7'
        try:
            resp = requests.get(self.url_initial + self.serial + backup_endpoint, headers=self.headers)
            resp.raise_for_status()
        except requests.exceptions.HTTPError as err:
            print(err)
        return resp.json()
    
    def enable_self_consumption(self):
        sc_endpoint = '/api/setting?EM_OperatingMode=8'
        try:
            resp = requests.get(self.url_initial + self.serial + sc_endpoint, headers=self.headers)
            resp.raise_for_status()
        except requests.exceptions.HTTPError as err:
            print(err)
        return resp.json()

    # Manual Mode
    # This mode allows the user to manually charge or discharge the batteries. The user needs to provide the
    # value for charging or discharging and based on the value, the ecoLinx system will charge until it reaches
    # 100% or discharge until it reaches 0% User SOC (unless stopped by the user by changing the charge/discharge
    # value to 0).
    # Enabled by default
    def enable_manual_mode(self):
        manual_endpoint = '/api/setting?EM_OperatingMode=1'
        try:
            resp = requests.get(self.url_initial + self.serial + manual_endpoint, headers=self.headers)
            resp.raise_for_status()
        except requests.exceptions.HTTPError as err:
            print(err)
        return resp.json()

    def manual_mode_control(self, mode='charge', value='0'):
        control_endpoint = '/api/v1/setpoint/'
        # Checking if system is in off-grid mode
        voltage = self.status['Uac']
        if voltage == 0:
            print('Battery is in off-grid mode... Cannot execute the command')
            return {}
        else:
            try:
                # value='300'
                print("try1", value)
                print(self.url_initial + self.serial + control_endpoint + mode + '/' + value)
                print(self.headers)
                # resp = requests.get(self.url_initial + self.serial + control_endpoint + mode + '/' + value, headers=self.headers)
                print("try2", value)
                # resp.raise_for_status()
                print("try3", value)
            except requests.exceptions.HTTPError as err:
                print(err)
            # return resp.json()


def new_sonnen(serial: int) -> Sonnen: 
    sonnen = Sonnen(str(serial), sonnen_token)
    sonnen.enable_manual_mode()
    return sonnen

def quit_sonnen(sonnen: Sonnen) -> bool: 
    # and resume the backup buffer 
    sonnen.enable_self_consumption()
    return True

def sonnen_get_status(sonnen: Sonnen, Wh: int, MCC_mA: int, MDC_mA: int) -> T.Tuple[int, int, int, int, int, int, int, int]: 
    # Wh = 10000
    # MCC_mA = 30000
    # MDC_mA = 30000
    status = sonnen.get_internal_status()
    print(status)
    # note: voltage_mV, current_mA, charge_mAh, max_capacity_mAh, max_charge_current_mA, max_discharge_current_mA 
    timestamp = status['Timestamp'] # example: '2021-10-11 00:27:37' 
    dt = datetime.datetime.strptime(timestamp, "%Y-%m-%d %H:%M:%S")
    seconds_since_epoch_float = dt.timestamp()
    seconds_since_epoch_int = int(seconds_since_epoch_float)
    milliseconds = 0 # int((seconds_since_epoch_float - seconds_since_epoch_int) * 1000)
    return \
        status['Uac'] * 1000, \
        round(status['Pac_total_W']/status['Uac']*1000), \
        round(Wh * status['USOC']/100 / status['Uac'] * 1000), \
        round(Wh / status['Uac'] * 1000), \
        MCC_mA, \
        MDC_mA, \
        seconds_since_epoch_int, \
        milliseconds

def sonnen_set_current(sonnen: Sonnen, current_mA: int) -> int: 
    resp = None
    if current_mA >= 0:
        print("set current python {}".format(current_mA))
        # resp = sonnen.manual_mode_control('discharge', str(round(sonnen.status['Uac'] * current_mA/1000)))
        resp = requests.get(
            url_initial + os.environ.get("SONNEN_SERIAL1") + '/api/v1/setpoint/' + 'discharge/' + str(0), headers=headers)
        # time.sleep(3)
        print("set current python {}".format(current_mA))
        print(resp.json())
    else: 
        resp = sonnen.manual_mode_control('charge', str(round(sonnen.status['Uac'] * (-current_mA)/1000)))
    return 1
    if resp is None: return 0
    code = int(resp['ReturnCode'])
    return int(code == 0)



"""
Name Description
Consumption_W 
    House comsumption in watts
Production_W 
    PV Production in watts
Pac_total_W 
    Inverter AC Power greater than ZERO is discharging
    Inverter AC Power less than ZERO is charging RSOC Relative state of charge
USOC 
    User state of charge
Fac 
    AC frequency in hertz. 
Uac 
    AC voltage in volts
Ubat 
    Battery volatge in volts 
Timestamp 
    System time 
IsSystemInstalled 
    System is installed or not

batt status: {
    'BackupBuffer': '5',  // --- important, save 5% SOC then stop discharging 
    'BatteryCharging': False, 
    'BatteryDischarging': False, 
    'Consumption_W': 19, 
    'Fac': 60,   // frequency
    'FlowConsumptionBattery': False, 
    'FlowConsumptionGrid': True, 
    'FlowConsumptionProduction': True, 
    'FlowGridBattery': False, 
    'FlowProductionBattery': False, 
    'FlowProductionGrid': False, 
    'GridFeedIn_W': -4, 
    'IsSystemInstalled': 1, 
    'OperatingMode': '8',  // --- important
    'Pac_total_W': 0,      // output power in AC 
    'Production_W': 15,    // ???
    'RSOC': 98,   // Relative SOC
    'SystemStatus': 'OnGrid', 
    'Timestamp': '2021-10-11 00:27:37', 
    'USOC': 98,   // SOC in percentage??? 
    'Uac': 237,   // AC voltage
    'Ubat': 54    // Battery voltage
}

AC current: Pac_total_W / Uac 

10kWh * USOC 

5 modules 

and 2kWh each 



"""
