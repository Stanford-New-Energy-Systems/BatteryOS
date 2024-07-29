import cvxpy as cp
import numpy as np
import matplotlib.pyplot as plt
import datetime

#### defunct case 
def case_2(self, solar, loads, timesteps, num_homes, time_vec, initSOC): # this function assumes solar and loads are lists of lists
    SOCs        = cp.Variable((timesteps+1, 1)) 
    meter       = cp.Variable((timesteps, num_homes))
    batt_output = cp.Variable((timesteps, 1))
    costs     = np.array((num_homes,), dtype='float32')
    

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
    constraints += [SOCs[0] == initSOC]
    constraints += [SOCs[-1] == initSOC]
    constraints += [SOCs <= 1]
    constraints += [SOCs >= 0]
    constraints += [batt_output <= self.dischg_power[0]]
    constraints += [batt_output >= -self.chg_power[0]] 
    for i in range(timesteps):
        constraints += [SOCs[i+1] == SOCs[i] - self.dt * batt_output[i] / self.batt_energy[0]]  
    

    obj = 0
    for home in range(num_homes):
        constraints += [meter[:, home] == -solar[:, home] + loads[:, home] - self.gamma_vec[home] * batt_output[:, 0]]
        constraints += [meter[:, home] <= self.transformer_limit / self.num_homes]
       

        obj += cp.maximum(meter[:, home],0) * self.dt @ price_vector

    
    prob = cp.Problem(cp.Minimize(obj), constraints)
    total_cost = prob.solve(verbose=False)
    for home in range(num_homes):
        costs[home] = np.maximum(meter.value[:, home],0) * self.dt @ price_vector
        
    # meters.append(meter.value)
    # battSOCs.append(SOCs.value)
    # batteries.append(batt_output.value)

    return meter.value, batt_output.value, costs, SOCs.value


def plot_case_2(self, meter_d_mpc, batt_output_d_mpc, soc_d_mpc, meter_d, batt_output_d, soc_d, start):
    num_homes = self.num_homes
    timesteps = meter_d_mpc.shape[0]
    fig, axes = plt.subplots(1,num_homes, figsize=(8,5.5))
    idxs = []
    for j in range(self.num_timesteps*2):
        idxs.append(np.where(self.data.loc[:, 'datetime']==start + datetime.timedelta(hours = j * self.dt))[0][0])
    
    if num_homes>1:
        for home in range(num_homes):
            axes[home].plot(np.arange(timesteps), meter_d[:, home], label = 'Meter - Determ', color=self.colorset[3])
            axes[home].plot(np.arange(timesteps), meter_d_mpc[:, home], label = 'Meter - MPC', color=self.colorset[5])
            axes[home].plot(np.arange(len(idxs)), self.data.loc[idxs, 'loads'] -self.data.loc[idxs, 'solar'], color = self.colorset[4], label = 'no BESS')
            axes[home].grid(visible=True, axis='y')
            axes[home].set_ylabel('Power [kW]')
            axes[home].legend()

    else:
        home = 0
        axes.plot(np.arange(timesteps), meter_d[:, home], label = 'Meter - Determ', color=self.colorset[3])
        axes.plot(np.arange(timesteps), meter_d_mpc[:, home], label = 'Meter - MPC', color=self.colorset[5])
        axes.plot(np.arange(len(idxs)), self.data.loc[idxs, 'loads'] -self.data.loc[idxs, 'solar'], color = self.colorset[4], label = 'no BESS')
        axes.set_ylabel('Power [kW]')
        axes.grid(visible=True, axis='y')
        axes.legend()
        
    plt.show()
    plt.close()

    fig, axes = plt.subplots(2,1, figsize=(8,5.5))
    axes[0].plot(soc_d[:, home], label = 'SOC - Determ', color=self.colorset[3])
    axes[0].plot(soc_d_mpc[:, home], label = 'SOC - MPC', color=self.colorset[5])
    axes[0].set_ylabel('SOC')
    axes[0].legend()
    axes[1].plot(batt_output_d[:, home], label = 'Batt Power - Determ', color=self.colorset[3])
    axes[1].plot(batt_output_d_mpc[:, home], label = 'Batt Power - MPC', color=self.colorset[5])
    axes[1].set_ylabel('Power [kW]')
    axes[1].set_xlabel('Timestep')
    axes[1].legend()
    plt.show()
    return


