import sys
import copy
import datetime
import cvxpy as cp
import numpy as np
import matplotlib.pyplot as plt

class myBattery:
    def __init__(self, discharge_kW, charge_kW, capacity_kWh):
        self.charge    = charge_kW
        self.capacity  = capacity_kWh
        self.discharge = discharge_kW

    def getCharge(self):
        return self.charge

    def getDischarge(self):
        return self.discharge

    def getCapacity(self):
        return self.capacity

    def __repr__(self):
        return "charge = {}, discharge = {}, capacity = {}".format(self.charge, self.discharge, self.capacity)


def iOpt(solar, loads, timesteps, num_homes, time_vec, initSOC, price_vector, battList, dt):
    SOCs    = cp.Variable((timesteps+1, num_homes))
    meter   = cp.Variable((timesteps, num_homes))
    costs   = np.zeros((num_homes,), dtype='float32')
    battery = cp.Variable((timesteps, num_homes))

    constraints = []
    
    constraints += [SOCs[0, :] == initSOC]
    constraints += [SOCs[-1, :] == initSOC]    
    constraints += [SOCs <= 1]
    constraints += [SOCs >= 0]

    obj = 0
    for home in range(num_homes):
        constraints += [meter[:, home] == -solar[:, home] + loads[:, home] - battery[:, home]]
        constraints += [battery[:, home] <= battList[home].getDischarge()]
        constraints += [battery[:, home] >= -battList[home].getCharge()]  

        for i in range(timesteps):
            constraints += [SOCs[i+1, home] == SOCs[i, home] - dt * battery[i, home] / battList[home].getCapacity()]

        obj += cp.maximum(meter[:, home], 0) * dt @ price_vector

    prob = cp.Problem(cp.Minimize(obj), constraints)
    prob.solve(verbose=False, solver="MOSEK")

    for home in range(num_homes):
        costs[home] = np.maximum(meter.value[:, home], 0) * dt @ price_vector 

    return meter.value, battery.value, costs, SOCs.value 

def jOpt(solar, loads, timesteps, num_homes, time_vec, initSOC, price_vector, battList, lam, transformer_limit, dt):
    SOCs    = cp.Variable((timesteps+1, num_homes)) 
    meter   = cp.Variable((timesteps, num_homes))
    battery = cp.Variable((timesteps, num_homes))

    costs = np.zeros((num_homes,), dtype='float32')
    orig  = np.zeros((num_homes,), dtype='float32')

    # right now we assume that each of the constituent batteries have
    # the same maximum energy capacity and so the initSOC of the aggregate
    # is just the average of the initSOC of each of the constituents
    # avgInitSOC = np.average(initSOC)
    
    constraints = []

    constraints += [SOCs[0, :] == initSOC]
    constraints += [SOCs[-1, :] == initSOC]    
    constraints += [SOCs <= 1]
    constraints += [SOCs >= 0]

    # constraints += [SOCs[0] == avgInitSOC]
    # constraints += [SOCs[-1] == avgInitSOC]

    # constraints += [battery <= aggBatt.getDischarge()]
    # constraints += [battery >= -aggBatt.getCharge()] 

    obj = 0
    for home in range(num_homes):
        constraints += [meter[:, home] == -solar[:, home] + loads[:, home]]
        constraints += [battery[:, home] <= battList[home].getDischarge()]
        constraints += [battery[:, home] >= -battList[home].getCharge()]  

        for i in range(timesteps):
            constraints += [SOCs[i+1, home] == SOCs[i, home] - dt * battery[i, home] / battList[home].getCapacity()]

    # for i in range(timesteps):
    #     constraints += [SOCs[i+1] == SOCs[i] - dt * battery[i, 0] / aggBatt.getCapacity()]  
    
    # for home in range(num_homes):
    #     constraints += [meter[:, home] == -solar[:, home] + loads[:, home] ]

    combined_meter = cp.sum(meter, axis=1) - cp.sum(battery, axis=1)

    obj = cp.maximum(combined_meter,0) * dt @ price_vector 
    violation = lam * cp.sum(cp.square(cp.maximum(combined_meter - transformer_limit, 0)))

    prob = cp.Problem(cp.Minimize(obj + violation), constraints)
    total_cost = prob.solve(verbose=False, solver='MOSEK')

    # calculate the cost for each homeowner without the aggregate battery
    # this allows for a fair method for distributing the battery usage 
    # the homeowners with a higher costs (either from higher load usage or
    # from load usage occurring during peak times) pay a larger portion of
    # the overall costs when the battery is considered
    for home in range(num_homes):
        orig[home] = np.maximum(meter.value[:, home],0) * dt @ price_vector

    if np.sum(orig) != 0:
        for home in range(num_homes):
            costs[home] = orig[home] / np.sum(orig) * obj.value

    return meter.value, battery.value, costs, SOCs.value

def case_4(self, solar, loads, timesteps, num_homes, time_vec, initSOC): # this function assumes solar and loads are lists of lists
    
    #create price vector
    price_vector = np.zeros((timesteps,))
    for time in range(timesteps):
        if time_vec[time].month >=6 and time_vec[time].month<10: #summer
            if time_vec[time].hour < 15: #off peak
                price_vector[time] = self.off_peak_summer
            elif time_vec[time].hour == 15 or time_vec[time].hour >=21: #peak
                price_vector[time] = self.peak_summer
            else: #off peak
                price_vector[time] = self.partial_peak_summer
        else: #winter
            if time_vec[time].hour < 15: #off peak
                price_vector[time] = self.off_peak_winter
            elif time_vec[time].hour == 15 or time_vec[time].hour >=21: #peak
                price_vector[time] = self.peak_winter
            else: #off peak
                price_vector[time] = self.partial_peak_winter

#    fraction = [0.5] * num_homes
#    fraction = [0.9, 0.9, 0.69, 0.9, 0.84, 0.9, 0.9, 0.74, 0.1]
#    fraction = [0.9, 0.9, 0.67, 0.9, 0.81, 0.9, 0.9, 0.71, 0.3]
#    fraction = [0.9, 0.9, 0.65, 0.9, 0.79, 0.9, 0.9, 0.69, 0.4]
#    fraction = [0.9, 0.9, 0.81, 0.9, 0.75, 0.9, 0.81, 0.78, 0.56] # from month of June

    fraction = self.fraction

    battList = []
    for home in range(num_homes): 
        battList.append(myBattery(self.dischg_power[home] * fraction[home],
                                  self.chg_power[home] * fraction[home],
                                  self.batt_energy[home] * fraction[home]))

    meter_0, battery_0, costs_0, SOCs_0 = iOpt(solar, loads, timesteps,
                                               num_homes, time_vec, initSOC[0, :],
                                               price_vector, battList, self.dt)

    solar_1 = copy.deepcopy(meter_0)
    solar_1[solar_1 > 0] = 0
    solar_1[solar_1 < 0] *= -1

    loads_1 = copy.deepcopy(meter_0)
    loads_1[loads_1 < 0] = 0

    for home in range(num_homes): 
        battList[home] = myBattery(self.dischg_power[home] * (1 - fraction[home]),
                                   self.chg_power[home] * (1 - fraction[home]),
                                   self.batt_energy[home] * (1 - fraction[home]))

    meter_1, battery_1, costs_1, SOCs_1 = jOpt(solar_1, loads_1, timesteps,
                                               num_homes, time_vec, initSOC[1, :],
                                               price_vector, battList, self.lam,
                                               self.transformer_limit, self.dt) 

    energy_0  = np.zeros((timesteps+1, num_homes), dtype="float32")
    energy_1  = np.zeros((timesteps+1, num_homes), dtype="float32")
    final_SOC = np.zeros((timesteps+1, num_homes), dtype="float32")
        
    for home in range(num_homes):
        energy_0[:, home] = SOCs_0[:, home] * fraction[home] * self.batt_energy[home]
        energy_1[:, home] = SOCs_1[:, home] * (1 - fraction[home]) * self.batt_energy[home]

        final_SOC[:, home] = (energy_0[:, home] + energy_1[:, home]) / self.batt_energy[home]

    r = lambda x : np.round(x, 4)

    # the individual optimization allows for each homeowner to use their battery to reduce their meter value,
    # while the second battery reduces the battery while protecting the transformer. The cost each homeowner pays
    # is calculated after the individual optimization is complete using the updates meter values

    final_battery = battery_0 + battery_1

    ## append the battery output from each partition so that 
    ## accurate calculations are done for the SOC of each battery
    ## for the beginning of the next time step

    final_battery = np.row_stack((final_battery, battery_0[0, :]))
    final_battery = np.row_stack((final_battery, battery_1[0, :]))

    return meter_1, final_battery, costs_1, final_SOC

def plot_case_4(self, meter_d_mpc, batt_output_d_mpc, soc_d_mpc, meter_d, batt_output_d, soc_d, start):
    num_homes = meter_d_mpc.shape[1]
    timesteps = meter_d_mpc.shape[0]
    fig, axes = plt.subplots(3,num_homes, figsize=(8,5.5))
    idxs = []
    for j in range(self.num_timesteps*2):
        idxs.append(np.where(self.data_list[0].loc[:, 'datetime']==start + datetime.timedelta(hours = j * self.dt))[0][0])
    
    if num_homes>1:
        for home in range(num_homes):
            axes[0, home].plot(np.arange(timesteps), meter_d[:, home], label = 'Meter - Determ', color=self.colorset[3])
            axes[0, home].plot(np.arange(timesteps), meter_d_mpc[:, home], label = 'Meter - MPC', color=self.colorset[5])
            axes[0, home].plot(np.arange(len(idxs)), self.data_list[home].loc[idxs, 'loads'] -self.data_list[home].loc[idxs, 'solar'], color = self.colorset[4], label = 'no BESS')
            axes[0, home].grid(visible=True, axis='y')
            axes[0, home].set_ylabel('Power [kW]')
            axes[0, home].legend()
            axes[1, home].plot(soc_d[:, home], label = 'SOC - Determ', color=self.colorset[1])
            axes[1, home].plot(soc_d_mpc[:, home], label = 'SOC - MPC', color=self.colorset[2])
            axes[1, home].set_ylabel('SOC')
            axes[1, home].legend()
            axes[2, home].plot(batt_output_d[:, home], label = 'Batt Power - Determ', color=self.colorset[1])
            axes[2, home].plot(batt_output_d_mpc[:, home], label = 'Batt Power - MPC', color=self.colorset[2])
            axes[2, home].set_ylabel('Power [kW]')
            axes[2, home].set_xlabel('Timestep')
            axes[2, home].legend()
    else:
        home=0
        axes[0].plot(np.arange(timesteps), meter_d[:, home], label = 'Meter - Determ', color=self.colorset[1])
        axes[0].plot(np.arange(timesteps), meter_d_mpc[:, home], label = 'Meter - MPC', color=self.colorset[2])
        axes[0].plot(np.arange(len(idxs)), self.data_list[0].loc[idxs, 'loads'] -self.data_list[0].loc[idxs, 'solar'], color = self.colorset[4], label = 'no BESS')
        axes[0].set_ylabel('Power [kW]')
        axes[0].grid(visible=True, axis='y')
        axes[0].legend()
        axes[1].plot(soc_d[:, home], label = 'SOC - Determ', color=self.colorset[1])
        axes[1].plot(soc_d_mpc[:, home], label = 'SOC - MPC', color=self.colorset[2])
        axes[1].set_ylabel('SOC')
        axes[1].legend()
        axes[2].plot(batt_output_d[:, home], label = 'Batt Power - Determ', color=self.colorset[1])
        axes[2].plot(batt_output_d_mpc[:, home], label = 'Batt Power - MPC', color=self.colorset[2])
        axes[2].set_ylabel('Power [kW]')
        axes[2].set_xlabel('Timestep')
        axes[2].legend()
    plt.show()
    return
