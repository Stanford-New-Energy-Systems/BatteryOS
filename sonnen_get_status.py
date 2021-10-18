import datetime
from xml.etree import ElementTree as ET
import requests
from requests.auth import HTTPDigestAuth
import numpy as np
import time
import json
import pandas as pd
import csv


class SonnenInterface():

    def __init__(self, serial=None, auth_token=None):
        self.serial = serial
        self.token = auth_token
        # the prefix of control urls
        self.url_ini = 'https://core-api.sonnenbatterie.de/proxy/'
        # request headers
        self.headers = {
            'Accept': 'application/vnd.sonnenbatterie.api.core.v1+json',
            'Authorization': 'Bearer ' + self.token, 
        }

    def get_status(self):
        status_endpoint = '/api/v1/status'
        try:
            resp = requests.get(self.url_ini + self.serial + status_endpoint, headers=self.headers)
            resp.raise_for_status()

        except requests.exceptions.HTTPError as err:
            print(err)

        return resp.json()

    # Backup:
    # Intended to maintain an energy reserve for situations where the Grid is no longer available. During the off-grid
    # period the energy would be dispensed to supply the demand of power from all the essential loads.
    # Load management can be enabled to further extend the life of the batteries by the Developers.

    def enable_backup_mode(self):
        backup_endpoint = '/api/setting?EM_OperatingMode=7'
        try:
            resp = requests.get(self.url_ini + self.serial + backup_endpoint, headers=self.headers)
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
            resp = requests.get(self.url_ini + self.serial + sc_endpoint, headers=self.headers)
            resp.raise_for_status()

        except requests.exceptions.HTTPError as err:
            print(err)

        return resp.json()

    # Time of Use (TOU):
    # This mode allows users to set time windows where it is preferred to employ the use of stored energy
    # (from PV) rather than consume from the grid.

    def enable_tou(self):
        tou_endpoint = '/api/setting?EM_OperatingMode=10'
        try:
            resp = requests.get(self.url_ini + self.serial + tou_endpoint, headers=self.headers)
            resp.raise_for_status()

        except requests.exceptions.HTTPError as err:
            print(err)

        return resp.json()

    def tou_grid_feedin(self, value=0):
        # value = 0 disable charging from grid
        # value = 1 enable charging from grid
        grid_feedin_endpoint = '/api/setting?EM_US_GRID_ENABLED=' + str(value)
        try:
            resp = requests.get(self.url_ini + self.serial + grid_feedin_endpoint, headers=self.headers)
            resp.raise_for_status()

        except requests.exceptions.HTTPError as err:
            print(err)

        return resp.json()

    def tou_window(self, pk_start='[16:00]', pk_end='[21:00]', opk_start='[21:01]'):
        # value format = [HH:00] in 24hrs format
        tou_pk_start_endpoint = '/api/setting?EM_US_PEAK_HOUR_START_TIME=' + pk_start
        tou_pk_end_endpoint = '/api/setting?EM_US_PEAK_HOUR_END_TIME=' + pk_end
        tou_opk_start_endpoint = '/api/setting?EM_US_LOW_TARIFF_CHARGE_TIME=' + opk_start
        try:
            resp1 = requests.get(self.url_ini + self.serial + tou_pk_start_endpoint, headers=self.headers)
            resp1.raise_for_status()
            resp2 = requests.get(self.url_ini + self.serial + tou_pk_end_endpoint, headers=self.headers)
            resp2.raise_for_status()
            resp3 = requests.get(self.url_ini + self.serial + tou_opk_start_endpoint, headers=self.headers)
            resp3.raise_for_status()

        except requests.exceptions.HTTPError as err:
            print(err)

        return [resp1.json(), resp2.json(), resp3.json()]

    # Manual Mode
    # This mode allows the user to manually charge or discharge the batteries. The user needs to provide the
    # value for charging or discharging and based on the value, the ecoLinx system will charge until it reaches
    # 100% or discharge until it reaches 0% User SOC (unless stopped by the user by changing the charge/discharge
    # value to 0).

    # Enabled by default
    def enable_manual_mode(self):
        manual_endpoint = '/api/setting?EM_OperatingMode=1'
        try:
            resp = requests.get(self.url_ini + self.serial + manual_endpoint, headers=self.headers)
            resp.raise_for_status()

        except requests.exceptions.HTTPError as err:
            print(err)

        return resp.json()

    def manual_mode_control(self, mode='charge', value='0'):
        control_endpoint = '/api/v1/setpoint/'
        # Checking if system is in off-grid mode
        voltage = SonnenInterface(serial=self.serial, auth_token=token).get_status()['Uac']

        if voltage == 0:
            print('Battery is in off-grid mode... Cannot execute the command')
            return {}

        else:
            try:
                resp = requests.get(self.url_ini + self.serial + control_endpoint + mode + '/' + value,
                                    headers=self.headers)
                resp.raise_for_status()

            except requests.exceptions.HTTPError as err:
                print(err)

            return resp.json()



if __name__ == "__main__": 
    token = '5db92cf858eebce34af146974f49f4d40ec699b99372546c0af628fb48133f61'
    gismo_batt_serial = '66352'
    batt = SonnenInterface(serial = gismo_batt_serial, auth_token = token)



    ### Sonia's code below
    print('batt status:', batt.get_status())
    # for i in range(2):
    #     with open('test1.csv', 'a') as csv_file:
    #         csv_writer = csv.writer(csv_file, delimiter=',')
    #         csv_writer.writerow(batt.get_status().values())

    # batt.enable_manual_mode()


    # Discharging 1kW
    # batt.manual_mode_control(mode='charge', value='3000')
    # batt.manual_mode_control(mode='charge', value='200')
    # batt.manual_mode_control(mode='discharge', value='6000')

    vals = list(batt.get_status().values())
    soc = float(vals[19])
    voltage = float(vals[21])
    print('soc:', soc)
    print('V =', voltage)
    # print(batt.get_status())
    batt.enable_self_consumption()
    print(batt.get_status())






