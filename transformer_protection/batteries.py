class Battery:
    def __init__(self, energy_kWh, charge_kW, discharge_kW, SOC):
        self.energy       = energy_kWh 
        self.initSOC      = SOC
        self.charge_kW    = charge_kW
        self.discharge_kW = discharge_kW 

class TeslaBattery(Battery):
    def __init__(self, SOC):
        self.energy       = 13.5 # should be 13.5
        self.initSOC      = SOC
        self.charge_kW    = 7
        self.discharge_kW = 7
