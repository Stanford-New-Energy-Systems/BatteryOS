"""
A BAL implementation for the Sonnen Battery
"""
import datetime
from xml.etree import ElementTree as ET
import requests
from requests import auth
from requests.auth import HTTPDigestAuth
import numpy as np
import time
import json
import pandas as pd
import csv
import os
import typing as T
import BatteryInterface
# import Interface
import BOSErr
class SonnenBattery(BatteryInterface.Battery):
    def __init__(self, name: str, serial: str, auth_token: str = os.environ.get("SONNEN_TOKEN"), staleness: int=1000): 
        super().__init__(name, staleness)
        self.serial = serial
        self.token = auth_token
        # the prefix of the url for accessing the services
        self.url_initial = "https://core-api.sonnenbatterie.de/proxy/"
        # the request header
        self.headers = {
            'Accept': 'application/vnd.sonnenbatterie.api.core.v1+json',
            'Authorization': 'Bearer ' + self.token, 
        }
        self.status = None
        self.refresh()  # initialize the status 
        self.last_refresh = (time.time() * 1000)  # in ms
        self.MDC = 28  # max discharging current, delibrately set it smaller
        self.MCC = 28  # max charging current, delibrately set it smaller

    def refresh(self):
        status_endpoint = '/api/v1/status'
        try:
            resp = requests.get(self.url_initial + self.serial + status_endpoint, headers=self.headers)
            resp.raise_for_status()

            self.status = resp.json()
            self.last_refresh = (time.time() * 1000)

            # self.update_meter(newcurrent, newcurrent)

        except requests.exceptions.HTTPError as err:
            print(err)
            # if error, no change on status 
        
        return self.status

    def check_staleness(self):
        now = (time.time() * 1000)  # it's in milliseconds
        if now - self.last_refresh > self._sample_period: 
            self.refresh()
        return

    # Backup:
    # Intended to maintain an energy reserve for situations where the Grid is no longer available. During the off-grid
    # period the energy would be dispensed to supply the demand of power from all the essential loads.
    # Load management can be enabled to further extend the life of the batteries by the Developers.

    def enable_backup_mode(self):
        backup_endpoint = '/api/setting?EM_OperatingMode=7'
        try:
            resp = requests.get(self.url_initial + self.serial + backup_endpoint, headers=self.headers)
            resp.raise_for_status()
        except requests.exceptions.HTTPError as err:
            print(err)

        return resp.json()

    # Self-Consumption:
    # The ecoLinx monitors all energy sources (Grid, PV, Generator), loads, and Energy Reserve Percentage
    # in order to minimize purchase of energy from the Grid.

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
        # voltage = SonnenBattery(serial=self.serial, auth_token=self.token).refresh()['Uac']
        voltage = self.refresh()['Uac']
        if voltage == 0:
            print('Battery is in off-grid mode... Cannot execute the command')
            return {}
        else:
            try:
                resp = requests.get(self.url_initial + self.serial + control_endpoint + mode + '/' + value,
                                    headers=self.headers)
                resp.raise_for_status()
            except requests.exceptions.HTTPError as err:
                print(err)

            return resp.json()


    def get_status(self):
        self.check_staleness()

        return BatteryInterface.BatteryStatus(
            self.status['Uac'],   # Voltage 
            self.status['Pac_total_W'] / self.status['Uac'],  # AC current, need some conversion!!!
            (self.status['USOC']/100) * 10000 / self.status['Uac'],  # SOC, percentage??? 10kWh * USOC, in Ah
            10000 / self.status['Uac'],  # 10k Wh ==> convert it to Ah
            self.MDC,  # max discharging current, delibrately set it smaller
            self.MCC   # max charging current, delibrately set it smaller
        )

    def set_current(self, target_current):
        resp = None
        if target_current < 0:
            # charging
            if (-target_current) > self.MCC:
                return False
            resp = self.manual_mode_control(mode="charge", value=str(round((-target_current)*self.status['Uac'])))
        else:
            #discharging
            if target_current > self.MDC:
                return False
            resp = self.manual_mode_control(mode="discharge", value=str(round(target_current*self.status['Uac'])))
        return resp

    @staticmethod
    def type() -> str: 
        return "SonnenBattery"


    # not used below
    # def serialize(self):
    #     return super().serialize()

    # @staticmethod
    # def _deserialize_derived(d: dict):
    #     return super()._deserialize_derived(d)

    # @staticmethod
    # def deserialize(d: dict, types: dict):
    #     return super().deserialize(d, types)

    # # Time of Use (TOU):
    # # This mode allows users to set time windows where it is preferred to employ the use of stored energy
    # # (from PV) rather than consume from the grid.

    # def enable_tou(self):
    #     tou_endpoint = '/api/setting?EM_OperatingMode=10'
    #     try:
    #         resp = requests.get(self.url_initial + self.serial + tou_endpoint, headers=self.headers)
    #         resp.raise_for_status()
    #     except requests.exceptions.HTTPError as err:
    #         print(err)

    #     return resp.json()

    # def tou_grid_feedin(self, value=0):
    #     # value = 0 disable charging from grid
    #     # value = 1 enable charging from grid
    #     grid_feedin_endpoint = '/api/setting?EM_US_GRID_ENABLED=' + str(value)
    #     try:
    #         resp = requests.get(self.url_initial + self.serial + grid_feedin_endpoint, headers=self.headers)
    #         resp.raise_for_status()
    #     except requests.exceptions.HTTPError as err:
    #         print(err)

    #     return resp.json()

    # def tou_window(self, pk_start='[16:00]', pk_end='[21:00]', opk_start='[21:01]'):
    #     # value format = [HH:00] in 24hrs format
    #     tou_pk_start_endpoint = '/api/setting?EM_US_PEAK_HOUR_START_TIME=' + pk_start
    #     tou_pk_end_endpoint = '/api/setting?EM_US_PEAK_HOUR_END_TIME=' + pk_end
    #     tou_opk_start_endpoint = '/api/setting?EM_US_LOW_TARIFF_CHARGE_TIME=' + opk_start
    #     try:
    #         resp1 = requests.get(self.url_initial + self.serial + tou_pk_start_endpoint, headers=self.headers)
    #         resp1.raise_for_status()
    #         resp2 = requests.get(self.url_initial + self.serial + tou_pk_end_endpoint, headers=self.headers)
    #         resp2.raise_for_status()
    #         resp3 = requests.get(self.url_initial + self.serial + tou_opk_start_endpoint, headers=self.headers)
    #         resp3.raise_for_status()

    #     except requests.exceptions.HTTPError as err:
    #         print(err)

    #     return [resp1.json(), resp2.json(), resp3.json()]


if __name__ == "__main__": 
    # os.environ['SONNEN_TOKEN'] = 'Your token here'
    # code here
    sb = SonnenBattery("sonnen", serial='your serial', auth_token='your token', staleness=0)
    sb.enable_manual_mode()
    print(sb.get_status())
    print(sb.refresh())
    print(sb.set_current(28))

    sb.enable_self_consumption()

    pass



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

"""


"""
batt status: {
    'BackupBuffer': '5', 
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
    'OperatingMode': '8', 
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






