import cvxpy as cp
import numpy as np
import matplotlib.pyplot as plt
import datetime

def case_1(self, solar, loads, timesteps, num_homes, time_vec, initSOC): # this function assumes solar and loads are lists of lists
    SOCs        = cp.Variable((timesteps+1,num_homes)) 
    meter       = cp.Variable((timesteps, num_homes))
    batt_output = cp.Variable((timesteps, num_homes))
    costs     = np.zeros((num_homes,), dtype='float32')
    

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
    
    constraints = []
    constraints += [SOCs[0, :] == initSOC]
    constraints += [SOCs[-1, :] == initSOC]
    constraints += [SOCs <= 1]
    constraints += [SOCs >= 0]
     

    obj = 0
    for home in range(num_homes):
        constraints += [meter[:, home] == -solar[:, home] + loads[:, home] - batt_output[:, home]]
        #constraints += [meter[:, home] <= self.transformer_limit / self.num_homes]
        constraints += [batt_output[:, home] <= self.dischg_power[home]]
        constraints += [batt_output[:, home] >= -self.chg_power[home]]  

        for i in range(timesteps):
            constraints += [SOCs[i+1, home] == SOCs[i, home] - self.dt * batt_output[i, home] / self.batt_energy[home]]


        obj += cp.maximum(meter[:, home],0) * self.dt @ price_vector
        obj += self.lam * cp.sum(cp.square(cp.maximum(meter[:, home] - self.transformer_limit / self.num_homes, 0)))
    
    prob = cp.Problem(cp.Minimize(obj), constraints)
    total_cost = prob.solve(verbose=False, solver='MOSEK')
    for home in range(num_homes):
        costs[home] = np.maximum(meter.value[:, home],0) * self.dt @ price_vector
        
    # meters.append(meter.value)
    # battSOCs.append(SOCs.value)
    # batteries.append(batt_output.value)

    return meter.value, batt_output.value, costs, SOCs.value

def plot_case_1(self, meter_d_mpc, batt_output_d_mpc, soc_d_mpc, meter_d, batt_output_d, soc_d, start):
    num_homes = meter_d_mpc.shape[1]
    timesteps = meter_d_mpc.shape[0]
    fig, axes = plt.subplots(3,num_homes, figsize=(15,5.5))
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
