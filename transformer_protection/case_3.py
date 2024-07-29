import cvxpy as cp
import numpy as np
import matplotlib.pyplot as plt
import datetime
import pandas as pd
import matplotlib.dates as mdates


def case_3(self, solar, loads, timesteps, num_homes, time_vec, initSOC): # this function assumes solar and loads are lists of lists
    SOCs        = cp.Variable((timesteps+1, 1)) 
    meter       = cp.Variable((timesteps, num_homes))
    batt_output = cp.Variable((timesteps, 1))
    costs     = np.zeros((num_homes,), dtype='float32')
    orig_costs = np.zeros((num_homes,), dtype='float32')
    

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
        constraints += [SOCs[i+1] == SOCs[i] - self.dt * batt_output[i, 0] / self.batt_energy[0]]  
    #constraints += [cp.sum(meter, axis=1) <= self.transformer_limit]

    
    for home in range(num_homes):
        constraints += [meter[:, home] == -solar[:, home] + loads[:, home] ]

    combined_meter = cp.sum(meter, axis=1) -  batt_output[:, 0]

    obj = cp.maximum(combined_meter,0) * self.dt @ price_vector 
    violation = self.lam * cp.sum(cp.square(cp.maximum(combined_meter - self.transformer_limit, 0)))

    
    prob = cp.Problem(cp.Minimize(obj + violation), constraints)
    total_cost = prob.solve(verbose=False, solver='MOSEK')

    
    for home in range(num_homes):
        orig_costs[home] = np.maximum(meter.value[:, home],0) * self.dt @ price_vector
        
    for home in range(num_homes):
        costs[home] = orig_costs[home] / np.sum(orig_costs) * obj.value

    final_battery = np.zeros((timesteps, num_homes))

    for i in range(num_homes):
        final_battery[:, i] = batt_output.value[:, 0] / num_homes

#    return meter.value, batt_output.value, costs, SOCs.value
    return meter.value, final_battery, costs, SOCs.value

def plot_case_3(self, meter_d_mpc, batt_output_d_mpc, soc_d_mpc, meter_d, batt_output_d, soc_d, start):
#    batt_output_d = batt_output_d.reshape(-1,1)
    soc_d = soc_d.reshape(-1,1)
    timesteps = meter_d_mpc.shape[0]
    fig, axes = plt.subplots(2, 1, figsize=(10,8))
    idxs = []
    for j in range(meter_d.shape[0]):
        idxs.append(np.where(self.data_list[0].loc[:, 'datetime']==start + datetime.timedelta(hours = j * self.dt))[0][0])
    

    label_list = []
    stack_list = [np.sum(meter_d, axis=1)]

    #label_list.append('Home Loads and Solar')

    stack_list.append(-batt_output_d[:,0])
    label_list.append('Battery Output - PF')

    #axes[0].stackplot(np.arange(len(batt_output_d)), stack_list_pos,  colors=self.colorset[:self.num_homes+1],  alpha=.7, labels = label_list)
    #axes[0].stackplot(np.arange(len(batt_output_d)), stack_list_neg,  colors=self.colorset[:self.num_homes+1],  alpha=.7, )
    #stacks = axes[0].stackplot(np.arange(len(batt_output_d)), stack_list,  colors=['1', self.colorset[4]],  alpha=.6,labels = label_list )
   
    axes[0].axhline(0, color='k', linewidth=.8)
    

    no_bess = np.zeros((len(self.data_list[0].loc[idxs, 'loads']),))
    for home in range(self.num_homes):
        no_bess += self.data_list[home].loc[idxs, 'loads']
        no_bess -= self.data_list[home].loc[idxs, 'solar']

#    dates = [(start + datetime.timedelta(days=day)).strftime('%m/%d') for day in range(int(self.total_horizon/self.mpc_horizon)+1)]
    dates = [(start + datetime.timedelta(days=day*4)).strftime('%m/%d') for day in range(int(self.total_horizon/(self.mpc_horizon * 4))+1)]

    #hatches=["", "//"]
    #for stack, hatch in zip(stacks, hatches):
    #    stack.set_hatch(hatch)
    

    
    # axes[0].plot(np.arange(len(idxs)), np.ma.masked_where(np.maximum(-batt_output_d[:,0], 0)>0, no_bess), color = self.colorset[1], label = 'No Battery', linewidth=2)
    # axes[0].plot(np.arange(len(idxs)), np.ma.masked_where(np.minimum(-batt_output_d[:,0], 0)<0, no_bess), color = self.colorset[2], linewidth=2)
    axes[0].plot(np.arange(len(idxs)), no_bess, color = '0.7', label = 'Aggregate Meter Reading - No Battery', linewidth=2.1)
    print(np.sum(meter_d, axis = 1).shape, batt_output_d[:, 0].shape)
#    axes[0].plot(np.arange(len(idxs)),np.sum(meter_d, axis=1) -batt_output_d[:,0], color = 'k', label = 'Aggregate Meter Reading- PF', linewidth=2)
    axes[0].plot(np.arange(len(idxs)),np.sum(meter_d, axis=1) - batt_output_d.sum(axis = 1), color = 'k', label = 'Aggregate Meter Reading- PF', linewidth=2)


    axes[0].fill_between(np.arange(len(batt_output_d)), np.sum(meter_d, axis=1), np.sum(meter_d, axis=1) + np.maximum(-batt_output_d.sum(axis = 1), 0),   color=self.colorset[2],  alpha=.6,label = 'Battery Charge - PF' )
    axes[0].fill_between(np.arange(len(batt_output_d)), np.sum(meter_d, axis=1), np.sum(meter_d, axis=1) + np.minimum(-batt_output_d.sum(axis = 1), 0),  color=self.colorset[1],  alpha=.6,label = 'Battery Discharge - PF'  )

#    axes[0].fill_between(np.arange(len(batt_output_d)), np.sum(meter_d, axis=1), np.sum(meter_d, axis=1)+ np.maximum(-batt_output_d[:,0], 0),   color=self.colorset[2],  alpha=.6,label = 'Battery Charge - PF' )
#    axes[0].fill_between(np.arange(len(batt_output_d)), np.sum(meter_d, axis=1), np.sum(meter_d, axis=1) + np.minimum(-batt_output_d[:,0], 0),  color=self.colorset[1],  alpha=.6,label = 'Battery Discharge - PF'  )
    axes[0].axhline(25, linewidth=1.7, color=self.colorset[4])
    axes[0].annotate('Transformer Limit', (270,27), color=self.colorset[4])

    #axes[0].grid(visible=True, axis='y')
    axes[0].set_ylabel('Power [kW]', fontsize=14)
    axes[0].set_ylim([min(no_bess)*1.1, max(no_bess)*1.9])
    axes[0].legend(ncols=2,fontsize=10, loc=1)
    print(self.total_horizon)
    print(self.mpc_horizon)
#    axes[0].set_xticks(np.linspace(0,len(idxs), int(self.total_horizon/self.mpc_horizon + 1)))
    axes[0].set_xticks(np.linspace(0,len(idxs),8))
    axes[0].set_xticklabels(dates)
    axes[0].tick_params(axis='both', which='major', labelsize=11)
    

    stack_list = []
    

    label_list = []
    #label_list.append('Home Loads and Solar')
    label_list.append('Battery Power - MPC')

    ##note: meter_d_mpc isn't a helpful value to plot here - it's the forecasted meter values
    # so, we only plot with the true meter, meter_d plus the batt mpc output
    stack_list.append(np.sum(meter_d, axis=1))
    stack_list.append(- batt_output_d_mpc[:, 0])
    
    #stacks = axes[1].stackplot(np.arange(timesteps), stack_list, colors=['1', self.colorset[4]],  alpha=.6, labels = label_list)
    # for stack, hatch in zip(stacks, hatches):
    #     stack.set_hatch(hatch)
    
    axes[1].axhline(0, color='k', linewidth=.8)

    #axes[1].plot(np.arange(len(idxs)), np.ma.masked_where(np.maximum(-batt_output_d_mpc[:,0], 0)>0, no_bess), color = self.colorset[1], label = 'No Battery', linewidth=2)
    #axes[1].plot(np.arange(len(idxs)), np.ma.masked_where(np.minimum(-batt_output_d_mpc[:,0], 0)<0, no_bess), color = self.colorset[2], label = 'No Battery', linewidth=2)
    
    axes[1].plot(np.arange(len(idxs)), no_bess, color = '0.7', label = 'Aggregate Meter Reading - No Battery', linewidth=2.1)
    axes[1].plot(np.arange(len(idxs)), np.sum(meter_d, axis=1) - batt_output_d_mpc.sum(axis = 1), color = 'k', label = 'Aggregate Meter Reading - MPC', linewidth=2)
#    axes[1].plot(np.arange(len(idxs)), np.sum(meter_d_mpc, axis=1) - batt_output_d_mpc.sum(axis = 1), color = 'k', label = 'Aggregate Meter Reading - MPC', linewidth=2)
#    axes[1].plot(np.arange(len(idxs)), np.sum(meter_d, axis=1) - batt_output_d_mpc[:,0], color = 'k', label = 'Aggregate Meter Reading - MPC', linewidth=2)
    
    axes[1].fill_between(np.arange(len(batt_output_d)), np.sum(meter_d, axis=1), np.sum(meter_d, axis=1)+ np.maximum(-batt_output_d_mpc.sum(axis = 1), 0),   color=self.colorset[2],  alpha=.6,label = 'Battery Charge - MPC')
    axes[1].fill_between(np.arange(len(batt_output_d)), np.sum(meter_d, axis=1), np.sum(meter_d, axis=1) + np.minimum(-batt_output_d_mpc.sum(axis = 1), 0),  color=self.colorset[1],  alpha=.6,label = 'Battery Discharge - MPC' )
#    axes[1].fill_between(np.arange(len(batt_output_d)), np.sum(meter_d, axis=1), np.sum(meter_d, axis=1)+ np.maximum(-batt_output_d_mpc[:,0], 0),   color=self.colorset[2],  alpha=.6,label = 'Battery Charge - MPC')
#    axes[1].fill_between(np.arange(len(batt_output_d)), np.sum(meter_d, axis=1), np.sum(meter_d, axis=1) + np.minimum(-batt_output_d_mpc[:,0], 0),  color=self.colorset[1],  alpha=.6,label = 'Battery Discharge - MPC' )
    axes[1].axhline(25, linewidth=1.7, color=self.colorset[4])
    axes[1].annotate('Transformer Limit', (270,27), color=self.colorset[4])
    #axes[1].grid(visible=True, axis='y')
    axes[1].set_ylabel('Power [kW]', fontsize=14)
    axes[1].set_ylim([min(no_bess)*1.1, max(no_bess)*1.9])
    axes[1].set_xlabel('Date', fontsize=14)
    axes[1].legend(ncols=2,fontsize=10, loc=1)
#    axes[1].set_xticks(np.linspace(0,len(idxs), int(self.total_horizon/self.mpc_horizon+1)))
    axes[1].set_xticks(np.linspace(0,len(idxs),8))
    axes[1].set_xticklabels(dates)
    axes[1].tick_params(axis='both', which='major', labelsize=11)
    
    
    #xfmt = mdates.DateFormatter('%m/%d')
    #axes[1].xaxis.set_major_formatter(xfmt)
    

    #axes[1].set_xticklabels(range(start, start+datetime.timedelta(hours = self.total_horizon)))

    plt.savefig('Figures/meter_comparison.pdf', bbox_inches='tight')
    plt.savefig('Figures/meter_comparison.png', bbox_inches='tight')
    plt.show()

    # fig, axes = plt.subplots(2,1, figsize=(8,5.5))
    # axes[0].plot(soc_d[:, 0], label = 'SOC - PF', linestyle = 'dashed', color=self.colorset[3], linewidth = 2.2)
    # axes[0].plot(soc_d_mpc[:, 0], label = 'SOC - MPC', color=self.colorset[4], linewidth = 2)
    # axes[0].set_ylabel('SOC', fontsize=14)
    # axes[0].set_ylim([-.1,1.2])
    # axes[0].set_xticks(np.linspace(0,len(idxs), int(self.total_horizon/self.mpc_horizon+1)))
    # axes[0].set_xticklabels(dates)
    # axes[0].tick_params(axis='both', which='major', labelsize=10)
    # axes[0].legend(fontsize=10.5,ncols=2, loc=1)

    # axes[1].plot(batt_output_d[:, 0], label = 'Battery Power - PF',linestyle = 'dashed', color=self.colorset[3], linewidth = 2.2)
    # axes[1].plot(batt_output_d_mpc[:, 0], label = 'Battery Power - MPC', color=self.colorset[4], linewidth = 2)
    # axes[1].set_ylabel('Power [kW]', fontsize=14)
    # axes[1].set_xlabel('Date', fontsize=14)
    # axes[1].set_ylim([-self.chg_power[0]*.5, self.chg_power[0]*.7])
    # axes[1].legend(fontsize=10.5,ncols=2, loc=1)
    # axes[1].set_xticks(np.linspace(0,len(idxs), int(self.total_horizon/self.mpc_horizon+1)))
    # axes[1].set_xticklabels(dates)
    # axes[1].tick_params(axis='both', which='major', labelsize=10)
    # plt.savefig('Figures/soc_comparison.pdf')
    # plt.savefig('Figures/soc_comparison.png')
    # plt.show()
    return
