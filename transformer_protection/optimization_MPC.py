#!/usr/bin/python3
#usr/local/bin/python3

import os
import sys
import random
import numpy as np
import cvxpy as cp
import pandas as pd
import matplotlib.pyplot as plt
import datetime
from statsmodels.tsa.arima.model import ARIMA
from sklearn.metrics import mean_squared_error
import pmdarima
from dateutil import parser
import json

from case_1 import case_1
from case_2 import case_2
from case_3 import case_3
from case_4 import case_4

from case_1 import plot_case_1
from case_2 import plot_case_2
from case_3 import plot_case_3
from case_4 import plot_case_4

from plot_case_1_3 import plot_case_1_3
from pStar import getFraction


class MPC(object):
    def __init__(self, data, filename):
        self.data_list = data

        params = json.load(open(filename))

        self.dt   = params['dt']
        self.lam  = params['lambda']
        self.eta  = params['eta']
        self.case = params['case']

        self.chg_power    = params['chg_power']    #kW
        self.batt_energy  = params['batt_energy']  # kWh
        self.dischg_power = params['dischg_power'] # kW
        self.transformer_limit = params['transformer_limit'] # kW
        
        self.soc_init  = params['soc_init']
        self.num_homes = params['num_homes']

        self.mpc_horizon      = params['mpc_horizon']
        self.total_horizon    = params['total_horizon']
        self.forecast_days    = params['forecast_days']
        self.num_timesteps    = int(self.mpc_horizon / self.dt)
        self.num_mpc_iters    = int(self.total_horizon  / self.dt)
        self.ev_forecast_days = params['ev_forecast_days']
        self.fraction         = None

        #PG&E TOU - effective April 1, 2014
        self.peak_summer = 0.66 # $/kWh
        self.partial_peak_summer= 0.55 # $/kWh
        self.off_peak_summer = 0.35 # $/kWh
        self.peak_winter = 0.54 # $/kWh
        self.partial_peak_winter = 0.52 # $/kWh
        self.off_peak_winter = 0.35 # $/kWh

        #Austin TOU
        # self.peak         = 0.08961
        # self.off_peak     = 0.02841
        # self.partial_peak = 0.04371

        self.colorset = ['#4477AA', '#EE6677', '#228833', '#CCBB44', '#66CCEE', '#AA3377', '#BBBBBB']
    
    def size_batteries(self):
        print(self.chg_power)
        print("sizing batteries ...")

        nBatt = [np.ceil(self.data_list[i].loads.max() / self.chg_power[0]) for i in range(self.num_homes)]
        
        self.chg_power    = [nBatt[i] * self.chg_power[0] for i in range(self.num_homes)]
        self.dischg_power = [nBatt[i] * self.dischg_power[0] for i in range(self.num_homes)]
        self.batt_energy  = [nBatt[i] * self.batt_energy[0] for i in range(self.num_homes)]

        print(self.chg_power)

    def reshape_data(self):
        ## downsample the data so that each index moves at the 
        ## the timestep of interest ( for example take a dataset
        ## that steps every 15 minutes and change it to every 30 
        ## minutes instead )
        ## average out data to fit MPC - downsampling only!

        for home in range(self.num_homes):
            temp = pd.DataFrame(columns = ['datetime', 'solar', 'loads', 'ev'])
            self.data_temp = self.data_list[home]
            current_dt = (self.data_temp.datetime[1].minute - self.data_temp.datetime[0].minute) / 60
            sampling_rate = int(self.dt/current_dt)
            temp.datetime = self.data_temp.datetime[1::sampling_rate]
            
            temp.solar = np.average(np.array(self.data_temp.solar).reshape(-1, sampling_rate), axis=1)
            temp.loads = np.average(np.array(self.data_temp.loads).reshape(-1, sampling_rate), axis=1)
            temp.ev    = np.average(np.array(self.data_temp.loads).reshape(-1, sampling_rate), axis=1)
            
            self.data_temp = temp
            self.data_temp = self.data_temp.reset_index(drop=True)
            self.data_list[home] = self.data_temp

    def run_mpc(self, solar, loads, timesteps, num_homes, time_vec, soc_init = .5):

        if self.case ==1:
            meter, batt_output, costs, SOCs = case_1(self, solar, loads, timesteps, num_homes, time_vec, soc_init)
        if self.case ==2:
            meter, batt_output, costs, SOCs = case_2(self, solar, loads, timesteps, num_homes, time_vec, soc_init)
        if self.case ==3:
            meter, batt_output, costs, SOCs = case_3(self, solar, loads, timesteps, num_homes, time_vec, soc_init)
        if self.case ==4:
            meter, batt_output, costs, SOCs = case_4(self, solar, loads, timesteps, num_homes, time_vec, soc_init)

        return meter, batt_output, costs, SOCs
    
    def take_mpc_step(self, soc_old, batt_output, time):
        # add ev loads to home loads (not combined together anymore)

        if self.case == 3:
            soc_new = soc_old - self.dt * batt_output / (self.batt_energy[0] / self.num_homes)
        else:
            soc_new = soc_old - self.dt * batt_output / self.batt_energy
        
        solar_actual = np.zeros((self.num_homes,))
        loads_actual = np.zeros((self.num_homes,))
        for home in range(self.num_homes):
            solar_actual[home] = self.data_list[home].loc[np.where(self.data_list[home].loc[:, 'datetime']==time)[0][0], 'solar']
            loads_actual[home] = self.data_list[home].loc[np.where(self.data_list[home].loc[:, 'datetime']==time)[0][0], 'loads'] + self.data_list[home].loc[np.where(self.data_list[home].loc[:, 'datetime']==time)[0][0], 'ev']

        # check if there are cases where the battery is set to charge when the loads are high
        # if so just set those values equal to 0
        # loc = np.where((loads_actual - solar_actual) >= 5)[0]
        # for ix in loc:
        #     if batt_output[ix] < 0:
        #         batt_output[ix] = 0

        meter = -solar_actual + loads_actual - batt_output
        
        return soc_new, meter
    
    def calc_mpc_cost(self, meter, num_homes, time_vec, batt_output=None, no_bess_val=False):
        timesteps = len(time_vec)
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

        if no_bess_val:
            cost_vec = np.zeros((num_homes,))
            for home in range(num_homes):
                cost_vec[home]= np.maximum(meter[:, home],0) * self.dt @ price_vector
        elif self.case ==1 or self.case == 4:
            cost_vec = np.zeros((num_homes,))
            for home in range(num_homes):
                cost_vec[home]= np.maximum(meter[:, home] -  batt_output[:, home], 0) * self.dt @ price_vector

        # elif self.case==3 or self.case==4:
        elif self.case == 3:
            combined_cost =  np.maximum(np.sum(meter, axis=1) -  batt_output[:, 0],0 ) * self.dt @ price_vector
            cost_vec = np.zeros((num_homes,))
            orig_costs = np.zeros((num_homes,), dtype='float32')
            for home in range(num_homes):
                orig_costs[home] = np.maximum(meter[:, home],0) * self.dt @ price_vector
        
            for home in range(num_homes):
                cost_vec[home] = orig_costs[home] / np.sum(orig_costs) * combined_cost
        

        return cost_vec

    def getSolarLoads(self, start_date):
        solar = np.zeros((self.num_mpc_iters, self.num_homes))
        loads = np.zeros((self.num_mpc_iters, self.num_homes))
        soc_init = np.zeros((self.num_homes,))

        soc_init[:] = self.soc_init

        for home in range(self.num_homes):
            mask = self.data_list[home][(self.data_list[home].datetime >= start_date) & (self.data_list[home].datetime <  start_date + datetime.timedelta(hours = self.total_horizon))].index
            solar[:, home] = self.data_list[home].loc[mask, 'solar']
            loads[:, home] = self.data_list[home].loc[mask, 'loads'] + self.data_list[home].loc[mask, 'ev']
            time_vec       = self.data_list[home].loc[mask].datetime.values

        return solar, loads, time_vec

    def getDates(self, startDate):
        for timestep in range(self.num_timesteps):
            yield [startDate - datetime.timedelta(days  = self.forecast_days) + \
                               datetime.timedelta(hours = timestep * self.dt) + \
                               datetime.timedelta(days  = day) for day in range(self.forecast_days)]
    
    def forecastPlots(self, forecast_df, f, f2, start):
             
        dates = list(self.getDates(start))
        if self.num_homes>1:
            fig, axes = plt.subplots(self.num_homes,1)
            for home in range(self.num_homes):
                count=0
                for currentDay in np.array(dates).transpose():
                    
                    solar = self.data_list[home].loc[np.isin(self.data_list[home].datetime, currentDay), 'solar']
                    # axes[home].plot(forecast_df.datetime, solar, color=self.colorset[count+2], label='Day '+ str(count))
                    axes[home].plot(forecast_df.datetime, solar, color=self.colorset[count], label='Day '+ str(count))
                    count+=1

                times  = [d[0] + datetime.timedelta(days = self.forecast_days) for d in dates]
                currDF = self.data_list[home][np.isin(self.data_list[home].datetime, times)]
                axes[home].plot(forecast_df.datetime, f[home]["solar_mean"],color=self.colorset[1],  label = 'avg')
                axes[home].plot(currDF.datetime, currDF.solar, color=self.colorset[0], label = 'actual')
                axes[home].set_ylabel('Power [kw]')
                axes[home].legend()

            # Plot Solar
            fig, axes = plt.subplots(self.num_homes,1)

            times  = [d[0] + datetime.timedelta(days = self.forecast_days) for d in dates]
            currDF = self.data_list[0][np.isin(self.data_list[0].datetime, times)]

            for home in range(self.num_homes):
                currDF = self.data_list[home][np.isin(self.data_list[home].datetime, times)]
                axes[home].plot(currDF.datetime, currDF.solar, color=self.colorset[0], label = 'actual')
                axes[home].plot(forecast_df.datetime, f[home]["solar_mean"], color=self.colorset[1], label = "avg")
                axes[home].set_ylabel('Power [kw]')
                axes[home].legend()
            plt.title('Solar')
            plt.show()

            # Plot Loads 
            fig, axes = plt.subplots(self.num_homes,1)
            for home in range(self.num_homes):
                currDF = self.data_list[home][np.isin(self.data_list[0].datetime, times)]
                axes[home].plot(currDF.datetime, currDF.loads,color=self.colorset[0], label = 'Actual')
                axes[home].plot(forecast_df.datetime, f[home]["loads_mean"],color=self.colorset[1], label = "Prediction")
                axes[home].set_ylabel('Power [kw]')
                axes[home].legend()
            plt.title('Loads')
            plt.show()

            fig, axes = plt.subplots(2,1, figsize=(6,6))
            currDF = self.data_list[0][np.isin(self.data_list[0].datetime, times)]
            axes[0].plot(np.arange(len(currDF.datetime)), currDF.solar,color='0', label = 'Actual Solar', linewidth=2)
            axes[0].plot(np.arange(len(forecast_df.datetime)), f[0]["solar_mean"],color=self.colorset[1], label = 'Forecast Solar', linewidth=2)
            #axes[0].plot(np.arange(len(forecast_df.datetime)), f2[0]["solar_mean"],color=self.colorset[2], label = 'Conservative Forecast Solar', linewidth=2)

            axes[0].set_ylabel('Power [kW]', fontsize=14)

            axes[0].legend(fontsize=11, loc=1)
            axes[0].set_xticks(range(0,len(forecast_df.datetime)+1,8))
            axes[0].set_xticklabels(range(0,24+1,4))
            axes[0].tick_params(axis='both', which='major', labelsize=11)
            #axes[0].xaxis.set_tick_params(labelbottom=False)
            currDF = self.data_list[0][np.isin(self.data_list[0].datetime, times)]
            axes[1].plot(np.arange(len(currDF.datetime)), currDF.loads,color='0', label = 'Actual Loads', linewidth=2)
            axes[1].plot(np.arange(len(forecast_df.datetime)), f[0]["loads_mean"], color=self.colorset[1], label = "Forecast Loads", linewidth=2)
            #axes[1].plot(np.arange(len(forecast_df.datetime)), f2[0]["loads_mean"], color=self.colorset[2], label = "Conservative Forecast Loads", linewidth=2)

            axes[1].set_ylabel('Power [kW]', fontsize=14)
            axes[1].legend(fontsize=11, loc=1)
            axes[1].set_xlabel('Time [hr]', fontsize=14)
            axes[1].set_xticks(range(0,len(forecast_df.datetime)+1,8))
            axes[1].set_xticklabels(range(0,24+1,4))
            axes[1].set_ylim([0,max(currDF.loads)*1.1])
            axes[1].tick_params(axis='both', which='major', labelsize=11)
            #plt.setp(axes, xticks=range(0,len(forecast_df.datetime),8), xticklabels=range(0,24,4))
            plt.savefig('Figures/forecast.pdf', bbox_inches='tight')
            plt.savefig('Figures/forecast.png', bbox_inches='tight')
            plt.show()

            print('Day = ' + str(start))
            solar_mape= []
            loads_mape = []
            for home in range(self.num_homes):
                solar_mape.append(abs((sum(self.data_list[home][np.isin(self.data_list[0].datetime, times)].solar)-sum(f[home]["solar_mean"]))/sum(self.data_list[0][np.isin(self.data_list[0].datetime, times)].solar)))
                loads_mape.append(abs((sum(self.data_list[home][np.isin(self.data_list[0].datetime, times)].loads)-sum(f[home]["loads_mean"]))/sum(self.data_list[0][np.isin(self.data_list[0].datetime, times)].loads)))
            solar_mape_final = (1/self.num_homes) * sum(solar_mape)
            loads_mape_final = (1/self.num_homes) * sum(loads_mape)
            print('Solar MAPE = ' + str(solar_mape_final))
            print('Loads MAPE = ' + str(loads_mape_final))


        else:
            fig, axes = plt.subplots(self.num_homes,1)
            for currentDay in np.array(dates).transpose():
                solar = self.data_list[0][np.isin(self.data_list[0].datetime, currentDay)].solar
                axes.plot(forecast_df.datetime, solar)

            axes.plot(forecast_df.datetime, f[0]["solar_mean"], color=self.colorset[1], label = 'avg')
            axes.set_ylabel('Power [kw]')
            axes.legend()
            
            plt.show()

            fig, axes = plt.subplots(self.num_homes,1)

            times  = [d[0] + datetime.timedelta(days = self.forecast_days) for d in dates]
            currDF = self.data_list[0][np.isin(self.data_list[0].datetime, times)]

            
            axes.plot(currDF.datetime, currDF.solar, color=self.colorset[0], label = 'actual')
            axes.plot(forecast_df.datetime, f[0]["solar_mean"], color=self.colorset[1], label = "avg")
            axes.set_ylabel('Power [kw]')
            axes.legend()
            plt.title('Solar')
            plt.show()

            fig, axes = plt.subplots(self.num_homes,1)
            
            axes.plot(currDF.datetime, currDF.loads, color=self.colorset[0], label = 'Actual')
            axes.plot(forecast_df.datetime, f[0]["loads_mean"], color=self.colorset[1], label = "Prediction")
            axes.set_ylabel('Power [kw]')
            axes.legend()
            plt.title('Loads')
            plt.show()
        
        return
    
    def return_forecast(self, start, plotting=False, forecast_type=1):
        forecast_df = pd.DataFrame(columns = ['datetime'])
        forecast_df.loc[0, 'datetime'] = start 

        for timestep in range(1, self.num_timesteps):
            forecast_df.loc[timestep, 'datetime'] = forecast_df.loc[0, 'datetime'] + timestep * datetime.timedelta(hours=self.dt)

        f = [{"solar_mean" : [], 
              "loads_mean" : [],  
              "ev_mean"    : []} for home in range(self.num_homes)]
    
        # forecast = 1 refers to the average of the past X days where 
        # X refers to the value specified in self.forecast_days
        # forecast = 2 takes the average of the past X days as well, 
        # but multiplies this average solar value by 0.8 and the loads
        # by 1.2 ultimately scaling down the solar and scaling up the loads

        if forecast_type==1:
            for timestep in range(self.num_timesteps):
                avg_idxs    = []
                ev_avg_idxs = []

                for day in range(self.forecast_days):
                    avg_idxs.append(np.where(self.data_list[0].loc[:, 'datetime'] == start - datetime.timedelta(days=self.forecast_days) + datetime.timedelta(hours = timestep * self.dt) + datetime.timedelta(days=day))[0][0])

                if self.ev_forecast_days != 0:
                    for day in range(self.ev_forecast_days):
                        ev_avg_idxs.append(np.where(self.data_list[0].loc[:, 'datetime'] == start - datetime.timedelta(days=self.ev_forecast_days) + datetime.timedelta(hours = timestep * self.dt) + datetime.timedelta(days=day))[0][0])
                else:
                    ev_avg_idxs.append(np.where(self.data_list[0].loc[:, 'datetime'] == start + datetime.timedelta(hours = timestep * self.dt))[0][0])
                    #print("ev_avg_idxs = {}".format(ev_avg_idxs))
                    #assert(True == False)
                
                for home in range(self.num_homes):

                    f[home]["solar_mean"].append(np.average(self.data_list[home].loc[avg_idxs, 'solar']))
                    f[home]["loads_mean"].append(np.average(self.data_list[home].loc[avg_idxs, 'loads']))
                    f[home]["ev_mean"].append(np.average(self.data_list[home].loc[ev_avg_idxs, 'ev']))
                
        if forecast_type==2:
            for timestep in range(self.num_timesteps):
                avg_idxs = []
                for day in range(self.forecast_days):
                    avg_idxs.append(np.where(self.data_list[0].loc[:, 'datetime'] == start - datetime.timedelta(days=self.forecast_days) + datetime.timedelta(hours = timestep * self.dt) + datetime.timedelta(days=day))[0][0])
                
                for home in range(self.num_homes):

                    f[home]["solar_mean"].append(np.average(self.data_list[home].loc[avg_idxs, 'solar']) * 0.8)
                    f[home]["loads_mean"].append(np.average(self.data_list[home].loc[avg_idxs, 'loads']) * 1.2)
            
        
        solar = np.zeros((len(forecast_df.datetime), self.num_homes))
        loads = np.zeros((len(forecast_df.datetime), self.num_homes))

        # print(len(forecast_df.datetime))

        for home in range(self.num_homes):
            # print("home = {} len of loads = {} len of ev = {} len of solar = {}".format(home, len(f[home]["loads_mean"]), len(f[home]["ev_mean"]), len(f[home]["solar_mean"])))
            solar[:, home] = f[home]["solar_mean"] 
            loads[:, home] = np.array(f[home]["loads_mean"]) + np.array(f[home]["ev_mean"])

        if plotting:
            # a bit of plotting
            f2 = [{"solar_mean" : [], 
              "loads_mean" : []} for home in range(self.num_homes)]
            for timestep in range(self.num_timesteps):
                avg_idxs = []
                for day in range(self.forecast_days):
                    avg_idxs.append(np.where(self.data_list[0].loc[:, 'datetime'] == start - datetime.timedelta(days=self.forecast_days) + datetime.timedelta(hours = timestep * self.dt) + datetime.timedelta(days=day))[0][0])
                
                for home in range(self.num_homes):

                    f2[home]["solar_mean"].append(np.average(self.data_list[home].loc[avg_idxs, 'solar']) * 0.8)
                    f2[home]["loads_mean"].append(np.average(self.data_list[home].loc[avg_idxs, 'loads']) * 1.2)
            self.forecastPlots(forecast_df, f, f2, start)
            
        return forecast_df.datetime, solar, loads


    def deterministic_mpc(self, start_date, plotting = False, forecast = 1, filename = None):

        meter       = np.zeros((self.num_mpc_iters, self.num_homes))
        batt_output = np.zeros((self.num_mpc_iters, self.num_homes))
        iopt_batt   = np.zeros((self.num_mpc_iters, self.num_homes))
        soc_init    = np.zeros((self.num_mpc_iters+1, self.num_homes))

        # for case 4 we are running both the individual and joint optimization cases together
        # this means that we need to pass in two different initSOCs ... one that represents
        # the initSOC for each of the inidivdual batteries and another that represents the
        # initSOCs of the batteries used for joint optimization

        if self.case == 4:
            soc_double = np.zeros((2, self.num_homes))
            soc_double[0, :] = self.soc_init
            soc_double[1, :] = self.soc_init

        soc_init[0, :] = self.soc_init
        batt_vec_list  = []

        for mpc_iter in range(self.num_mpc_iters):
            print(str(mpc_iter+1) + ' iteration out of ' + str(self.num_mpc_iters) + ' MPC iterations')
            time = start_date + datetime.timedelta(hours = mpc_iter * self.dt)

            dates, solar, loads = self.return_forecast(time, plotting = False, forecast_type = forecast)
            
            if self.case == 4:
                m, b, cost_vec, soc = self.run_mpc(solar, loads, self.num_timesteps, self.num_homes, dates, soc_init = soc_double)
                soc_double[0, :], iMeter = self.take_mpc_step(soc_double[0, :], b[-2, :], time)
                soc_double[1, :], jMeter = self.take_mpc_step(soc_double[1, :], b[-1, :], time)
                iopt_batt[mpc_iter, :]   = b[-2, :]
            else:
                m, b, cost_vec, soc = self.run_mpc(solar, loads, self.num_timesteps, self.num_homes, dates, soc_init = soc_init[mpc_iter, :])

            batt_output[mpc_iter, :] = b[0, :]
            soc_init[mpc_iter+1, :], meter[mpc_iter, :] = self.take_mpc_step(soc_init[mpc_iter, :], batt_output[mpc_iter, :], time)

            batt_vec_list.append(b)

        if plotting:
            start_idx = np.where(self.data_list[0].loc[:, 'datetime']==start_date)[0][0]
            end_idx = np.where(self.data_list[0].loc[:, 'datetime']==start_date + datetime.timedelta(hours = self.total_horizon-self.dt) )[0][0] 
            fig, axes = plt.subplots(int(b.shape[1]),1)
            
            if b.shape[1]>1:
                for home in range(b.shape[1]):
                    for iter in range(0,len(batt_vec_list),4):
                        date = self.data_list[0].loc[start_idx+iter:np.minimum(end_idx,start_idx+iter+self.num_timesteps-1), 'datetime'].values
                        axes[home].plot(date, batt_vec_list[iter][:len(date), home], color = self.colorset[1])
                    axes[home].plot(self.data_list[0].loc[start_idx:end_idx, 'datetime'].values, batt_output[:, home], label = 'Actual Output', color = self.colorset[0])
                    axes[home].set_ylabel('Power [kW]')
            else:
                for iter in range(0,len(batt_vec_list),4):
                    date = self.data_list[0].loc[start_idx+iter:np.minimum(end_idx,start_idx+iter+self.num_timesteps-1), 'datetime'].values
                    axes.plot(date, batt_vec_list[iter][:len(date)], color = self.colorset[1])
                axes.plot(self.data_list[0].loc[start_idx:end_idx, 'datetime'].values, batt_output, label = 'Actual Output', color = self.colorset[0])
                axes.set_ylabel('Power [kW]')

            plt.xlabel('Date')
            
            plt.legend()
            plt.show()

        start_idx = np.where(self.data_list[0].loc[:, 'datetime']==start_date)[0][0]
        end_idx = np.where(self.data_list[0].loc[:, 'datetime']==start_date + datetime.timedelta(hours = self.total_horizon - self.dt))[0][0] 
        time_vec = self.data_list[0].loc[start_idx:end_idx, 'datetime'].values
        
        # add EV values to this data
        no_bess = np.zeros((len(time_vec), self.num_homes))
        for home in range(self.num_homes):
            no_bess[:, home] = self.data_list[home].loc[start_idx:end_idx, 'loads'].values + self.data_list[home].loc[start_idx:end_idx, 'ev'].values - self.data_list[home].loc[start_idx:end_idx, 'solar'].values

        cost_vec = self.calc_mpc_cost(no_bess, self.num_homes, time_vec, batt_output)

        print('Total MPC cost is: ', np.sum(cost_vec))
        print('cost of each home is: ', cost_vec)
        
        if self.num_homes >1:
            cost_vec_no_bess = self.calc_mpc_cost(no_bess, self.num_homes, time_vec, batt_output=None, no_bess_val=True)
        else:
            cost_vec_no_bess = self.calc_mpc_cost(no_bess.reshape(-1,1), self.num_homes, time_vec, batt_output=None, no_bess_val=True)

        print('Total cost without BESS: ', np.sum(cost_vec_no_bess))
        print('cost of each home is: ', cost_vec_no_bess)

        if self.case == 4:
            path = 'Code/Results/case_4/f1'
            np.savetxt("{}/iopt_batt_output_mpc_{}_ev_forecast_{}.csv".format(path, 4, filename), iopt_batt)

        return meter, batt_output, soc_init, cost_vec, cost_vec_no_bess

    def full_deterministic(self, start_date, plotting=False):
        solar     = np.zeros((self.num_mpc_iters, self.num_homes))
        loads     = np.zeros((self.num_mpc_iters, self.num_homes))
        soc_init  = np.zeros((self.num_homes,))

        if self.case == 4:
            soc_init = np.zeros((2, self.num_homes))
            soc_init[0, :] = self.soc_init
            soc_init[1, :] = self.soc_init
        else:
            soc_init[:] = self.soc_init

        for home in range(self.num_homes):
            mask = self.data_list[home][(self.data_list[home].datetime >= start_date) & (self.data_list[home].datetime <  start_date + datetime.timedelta(hours = self.total_horizon))].index
            solar[:, home] = self.data_list[home].loc[mask, 'solar']
            loads[:, home] = self.data_list[home].loc[mask, 'loads'] + self.data_list[home].loc[mask, 'ev']
            time_vec       = self.data_list[home].loc[mask].datetime.values
 
        meter_amount, batt_output, cost_vec, soc_output = self.run_mpc(solar, loads, self.num_mpc_iters, self.num_homes, time_vec, soc_init)
        
        print('Total full deterministic cost is: ', np.sum(cost_vec))
        print('cost of each home is: ', cost_vec)

        return meter_amount, batt_output, soc_output, cost_vec

    
    
def process_data(filename, add_EV_to_loads = True, dt=0.25):
    data = pd.read_csv(filename)
    #data = data.rename(columns={'Date & Time':'datetime', 'Solar [kW]':'solar', 'Grid [kW]':'loads', 'EV Loads [kW]':'ev'})
    data = data.rename(columns={'loads [kW]':'loads', 'solar [kW]':'solar', 'ev (kW)':'ev'}) # new files
    #data = data.rename(columns={'Date & Time':'datetime', 'Solar [kW]':'solar', 'EV Loads[kW]':'ev'}) # old files
    #data.loads += data.solar
    
    #data.datetime = pd.to_datetime(data.datetime)
    #data cleaning:
    data.loc[np.where(data.solar < 0)[0][:], 'solar'] = 0
    data.loc[np.where(data.ev < 0.01)[0][:], 'ev'] = 0

    #data.solar[data.solar < 0] = 0
    #data.ev[data.ev < 0.01] = 0

    # solar and loads are currently reported in kWh every 15 minutes
    # divide all the data in these columns by 15/60 hours so that you
    # convert the energy consumed to the corresponding kW during that 
    # 15 minute time window
    data.loads = data.loads / dt
    data.solar = data.solar / dt

    # add EV data to loads - if not already pre-processed
    if add_EV_to_loads:
        data.loads += data.ev

    for i in range(len(data.datetime)):
        data.loc[i, 'datetime'] = parser.parse(str(data.loc[i,'datetime'])) # datetime.datetime.strptime(str(data.loc[i,'datetime']), '%Y-%m-%d %H:%M:%S')
        data.loc[i, 'datetime'] = datetime.datetime.replace(data.loc[i, 'datetime'], tzinfo=None)
    return data

def cost_savings(no_bess, indiv, joint):
    indiv_save = []
    joint_save = []
    for home in range(len(no_bess)):
        indiv_save.append((no_bess[home] - indiv[home])/no_bess[home])
        joint_save.append((no_bess[home] - joint[home])/no_bess[home])

    print('indiv savings: ', np.average(indiv_save))
    print('joint savings: ', np.average(joint_save))
    return np.average(indiv_save), np.average(joint_save)

def run_case(mpc, start, path, case, forecast):
    
    meter0, battery0, soc0, cost_bess, cost_no_bess = mpc.deterministic_mpc(start, plotting = False, forecast=1, filename = forecast)
    meter1, battery1, soc1, costD = mpc.full_deterministic(start)

    np.savetxt("{}/soc_d_{}_ev_forecast_{}.csv".format(path, case, forecast), soc1)
    np.savetxt("{}/soc_d_{}_mpc_ev_forecast_{}.csv".format(path, case, forecast), soc0)
    np.savetxt("{}/meter_d_{}_ev_forecast_{}.csv".format(path, case, forecast), meter1)
    np.savetxt("{}/meter_d_{}_mpc_ev_forecast_{}.csv".format(path, case, forecast), meter0)
    np.savetxt("{}/batt_output_d_{}_ev_forecast_{}.csv".format(path, case, forecast), battery1)
    np.savetxt("{}/batt_output_d_{}_mpc_forecast_{}.csv".format(path, case, forecast), battery0)
    np.savetxt("{}/cost_list_determ_{}_ev_forecast_{}.csv".format(path, case, forecast), costD)
    np.savetxt("{}/cost_list_mpc_{}_ev_forecast_{}.csv".format(path, case, forecast), cost_bess)
    np.savetxt("{}/cost_list_{}_no_bess_ev_forecast_{}.csv".format(path, case, forecast), cost_no_bess)

def sizeSolar(data, average_peak_hours = 5.3, inefficiency_factor = 1.2):
    # process for solar sizing is outlined as below
    # 1. determine the average daily kWh consumption from the home
    # 2. determine the average daily peak hours in the city in which panels are located
    # 3. divide (1) by (2) to determine the power output of the panels
    # 4. account for any inefficiencies in the system (multiplying (3) by 1.2 is a good choice)
    # 5. reference : https://www.gogreensolar.com/pages/sizing-solar-systems

    ev_kWh    = sum(data.ev) / 4
    loads_kWh = sum(data.loads) / 4
    daily_kWh = (ev_kWh + loads_kWh)/365
    
    return round(daily_kWh / average_peak_hours * inefficiency_factor, 2)

def countViolations(meter, limit = 25, violation_time_steps = 2):
    new_meter = meter.reshape(-1, violation_time_steps)
    iix = np.where((new_meter > [limit] * violation_time_steps).sum(axis = 1) == violation_time_steps)[0]

    return len(iix)

def setupHomes(files, iters = 1488, start_date = None, jan = False):
    solar = np.zeros((iters, 1))
    loads = np.zeros((iters, 1))

    names = []
    count = 0
    violations = 0

    while violations == 0:
        count += 1

        file = random.sample(files, 1)
#        file = random.sample(files, 1)[0]
        print("opening file = {}".format(file))
#        names.append(process_data(file, add_EV_to_loads = False))
#
#        maxSolar      = max(names[-1].solar)
#        currSolarSize = sizeSolar(names[-1])
#
#        if currSolarSize > maxSolar:
#            ratio = currSolarSize / maxSolar
#            names[-1].solar *= ratio
#
#        mask = (names[-1].datetime >= start_date) & (names[-1].datetime < start_date + datetime.timedelta(hours = iters / 2))
#
#        solar[:, 0] += np.average(np.array(names[-1].loc[mask, "solar"]).reshape(-1, 2))
#        loads[:, 0] += np.average(np.array(names[-1].loc[mask, "loads"]).reshape(-1, 2))
        
        names.append(file)

        data = pd.read_csv(file[0])

        solar[:, 0] += data.solar
        loads[:, 0] += data.loads

        violations = countViolations(loads - solar)
#        print("home {} violations = {}".format(count, violations))

    print("{} homes under this configuration".format(count))

    if jan:
        return [x[0].split("jan/")[0] + x[0].split("jan/")[1] for x in names], count
    return [x[0].split("july/")[0] + x[0].split("july/")[1] for x in names], count
#    return names, count
    

def main():
    #solar, loads, data = process_data(sys.argv[1])
    # temp_data = process_temp_data(5/60)
#    homeIDs   = [661, 4373, 4767, 6139, 7719, 8156, 1642, 8156, 1642]
#    createFileName = lambda start,end,ID: "Data_L2/data_{}_jan_{}_{}_l2.csv".format(ID, start, end)
#    start_day = 1
#   filenames = [createFileName(start_day, start_day + 10, ID) for ID in homeIDs]
    #filenames = [createFileName(start_day, start_day + 17, ID) for ID in homeIDs]

    # df = pd.read_csv('Data_L2/csv/102808-0.csv')

    # df["loads [kW]"] = df["loads [kW]"] / (15/60)
    # df["solar [kW]"] = df["solar [kW]"] / (15/60)

    # def convert_datetime(data):
    #     for i in range(len(data.datetime)):
    #         data.loc[i, 'datetime'] = parser.parse(str(data.loc[i,'datetime'])) # datetime.datetime.strptime(str(data.loc[i,'datetime']), '%Y-%m-%d %H:%M:%S')
    #         data.loc[i, 'datetime'] = datetime.datetime.replace(data.loc[i, 'datetime'], tzinfo=None)
    #     return data

    # IDs = [102808, 108199, 122340, 131172, 138546, 146132, 153948, 165637, 180477]

    # colors = ["xkcd:chocolate", "#04D8B2", "xkcd:azure", "xkcd:beige", "#000000", "xkcd:cyan", 
    #           "#653700", "#C1F80A", "#FC5A50", "#8C000F", "#054907", "#FF00FF", 
    #           "#FAC205", "#808080", "#380282", "#C79FEF", "#6E750E", "orangered", 
    #           "#580F41", "#FF0000", "#A9561E", "#008080", "#FFFF00", "#9ACD32"]

    # for ID in IDs:
        # df = process_data('Data_L2/csv/{}-0.csv'.format(ID), add_EV_to_loads=False)
        # df = pd.read_csv('Data_L2/loads/{}-0_loads.csv'.format(ID))
        # df = convert_datetime(df)

        # startIndex = np.where(df.datetime == datetime.datetime(2018, month, start_day, 0, 0))[0][0]
        # # print("startIndex = {}".format(startIndex))
        # print("the total loads of the year is {} and the total ev is {}".format(sum(df.loads) / 4, sum(df.ev) / 4))
        # print("{} Solar Size = {} kW and the maximum solar value = {} kW".format(ID, sizeSolar(df), max(df.solar)))
        # sys.exit()
        
        # plt.figure(figsize=(12, 8))

        # ix = 0
        # for col in df.columns:
        #     if col != "datetime" and col != "net" and col != "total":
        #         if max(df[col]) > 1.5:
        #             print("ix = {} and df.columns[ix] = {}".format(ix, col))
        #             plt.plot(df.datetime[startIndex:startIndex+672], df["{}".format(col)][startIndex:startIndex+672], label="{}".format(col), color=colors[ix])
        #             ix += 1
        # plt.legend()
        # plt.title("home: {} loads July 1-8, 2018".format(ID), fontsize=15)
        # plt.xlabel("Date", fontsize=15)
        # plt.ylabel("Power [kW]", fontsize = 15)
        # plt.savefig("Data_L2/loads/{}_loads.png".format(ID))
        # plt.show()
        # sys.exit()
        # startIndex = 17568

        # plt.figure(figsize=(12,8))
        # plt.plot(df.datetime[startIndex:startIndex+672], df["solar"][startIndex:startIndex+672], label = "solar")
        # plt.plot(df.datetime[startIndex:startIndex+672], df["loads"][startIndex:startIndex+672], label = "total home loads")
        # plt.plot(df.datetime[startIndex:startIndex+672], df["ev"][startIndex:startIndex+672], label = "ev")
        # plt.legend(loc = "upper left")
        # plt.title("home: {} energy profile July 1-8, 2018".format(ID), fontsize=15)
        # plt.xlabel("Date", fontsize=15)
        # plt.ylabel("Power [kW]", fontsize = 15)

        # plt.savefig("{}_july_1_7_2018_solar_loads_ev.png".format(ID))

    # print("date = {}".format(df.datetime[17568]))

    # assert(True == False)


#    IDs = [102808, 108199, 122340, 131172, 138546, 146132, 153948, 165637, 180477]
#    createFileName = lambda ID : "Data_L2/csv/{}-0.csv".format(ID)
#    filenames = [createFileName(ID) for ID in IDs]

    # print(filenames)
    
#    nHomes = 11

    month = 7
    start_day = 2

    random.seed(50)

    folder_path = "Data_L2/csv/"
    files = os.listdir(folder_path)
    files = [file for file in files if "loads" not in file and "july" not in file and "jan" not in file]
    files = ["Data_L2/csv/{}".format(file) for file in files]

#    filenames = [random.sample(files, nHomes) for i in range(25)] 

    july_folder_path = "Data_L2/csv/july/"
    july_files = os.listdir(july_folder_path)
    july_files = ["{}{}".format(july_folder_path, july_file) for july_file in july_files]

    jan_folder_path = "Data_L2/csv/jan/"
    jan_files = os.listdir(jan_folder_path)
    jan_files = ["{}{}".format(jan_folder_path, jan_file) for jan_file in jan_files]

    [setupHomes(july_files) for i in range(30)]
#    [setupHomes(jan_files, iters = 1344, jan = True) for i in range(48)]

    start_date = datetime.datetime(2018, month, start_day, 0 , 0)

    for trial in range(31, 51):

#        filenames = random.sample(files, 9)

        filenames, nHomes = setupHomes(july_files)        
#        filenames, nHomes = setupHomes(jan_files, iters = 1344, jan = True)        
#        data, nHomes = setupHomes(files, start_date = start_date)

        data = []

#        for home_file in filenames[trial-1]:
#        for home_file in filenames[0]:

        for home_file in filenames:
            print("opening {} and storing into array".format(home_file))
            data.append(process_data(home_file, add_EV_to_loads = False))

        # size solar appropriately for loads
#        for ix in range(len(data)):
#            maxSolar      = max(data[ix].solar)
#            currSolarSize = sizeSolar(data[ix])
#
#            if currSolarSize > maxSolar:
#                ratio = currSolarSize / maxSolar
#                data[ix].solar *= ratio
            
#        start_date = datetime.datetime(2018, month, start_day + 4, 0 , 0)
        # start_date = datetime.datetime(2018, month, start_day + 4, 0 , 15)



#        ev_forecast = "0_jan_trial_{}".format(trial)
        ev_forecast = "0_75_trial_{}".format(trial)
#        ev_forecast = "0_trial_{}".format(trial)
#        ev_forecast = "0_sizing_trial_{}".format(trial)
#        ev_forecast = "0_jan_sizing_trial_{}".format(trial)

        # forecast_1
#        experiment_file = 'Code/Experiments/case1_9_homes.json'


        experiment_file = 'Code/Experiments/case1.json'
        mpc_problem = MPC(data, experiment_file)
#
        mpc_problem.batt_energy  = [13.5] * nHomes
        mpc_problem.chg_power    = [5] * nHomes
        mpc_problem.dischg_power = [5] * nHomes
        mpc_problem.num_homes    = nHomes 
        mpc_problem.reshape_data()
#        mpc_problem.size_batteries()


#
#        solar, loads, time = mpc_problem.getSolarLoads(start_date - datetime.timedelta(days = 30))
#        solar, loads, time = mpc_problem.getSolarLoads(start_date)
#        fraction = getFraction(mpc_problem, solar, loads, mpc_problem.num_mpc_iters, mpc_problem.num_homes, time, mpc_problem.soc_init)
#        # mpc_problem.ev_forecast_days = ev_forecast

#        path = 'Code/Results/case_1/f1'
#        run_case(mpc_problem, start_date, path, case=1, forecast=ev_forecast)
#        
#        cost_list_mpc_1     = np.loadtxt("{}/cost_list_mpc_1_ev_forecast_{}.csv".format(path, ev_forecast))
#        cost_list_determ_1  = np.loadtxt("{}/cost_list_determ_1_ev_forecast_{}.csv".format(path, ev_forecast))
#        cost_list_1_no_bess = np.loadtxt("{}/cost_list_1_no_bess_ev_forecast_{}.csv".format(path, ev_forecast))

        # forecast_3
#        experiment_file = 'Code/Experiments/case3.json'
#        mpc_problem = MPC(data, experiment_file)
#
#        mpc_problem.batt_energy  = [13.5 * nHomes]
#        mpc_problem.chg_power    = [5 * nHomes]
#        mpc_problem.dischg_power = [5 * nHomes]
#        mpc_problem.num_homes    = nHomes 
##        mpc_problem.reshape_data()
##        mpc_problem.size_batteries()
#        
#        #mpc_problem.ev_forecast_days = ev_forecast
#
#        path = 'Code/Results/case_3'
#        run_case(mpc_problem, start_date, path, case=3, forecast=ev_forecast)
#
#        cost_list_mpc_3    = np.loadtxt("{}/cost_list_mpc_3_ev_forecast_{}.csv".format(path, ev_forecast))
#        cost_list_determ_3 = np.loadtxt("{}/cost_list_determ_3_ev_forecast_{}.csv".format(path, ev_forecast))
#        cost_list_3_no_bess = np.loadtxt("{}/cost_list_3_no_bess_ev_forecast_{}.csv".format(path, ev_forecast))

        # forecast_4
        experiment_file = 'Code/Experiments/case4.json'
        mpc_problem = MPC(data, experiment_file)

        mpc_problem.batt_energy  = [13.5] * nHomes
        mpc_problem.chg_power    = [5] * nHomes
        mpc_problem.dischg_power = [5] * nHomes
        mpc_problem.num_homes    = nHomes 

        mpc_problem.fraction = [0.75] * mpc_problem.num_homes
#        mpc_problem.reshape_data()
#        mpc_problem.size_batteries()
#        mpc_problem.fraction = fraction
        #mpc_problem.ev_forecast_days = ev_forecast
        
        path = 'Code/Results/case_4/f1'
        run_case(mpc_problem, start_date, path, case=4, forecast=ev_forecast)

        cost_list_mpc_4     = np.loadtxt("{}/cost_list_mpc_4_ev_forecast_{}.csv".format(path, ev_forecast))
        cost_list_determ_4  = np.loadtxt("{}/cost_list_determ_4_ev_forecast_{}.csv".format(path, ev_forecast))
        cost_list_4_no_bess = np.loadtxt("{}/cost_list_4_no_bess_ev_forecast_{}.csv".format(path, ev_forecast))

        # print and plot data

#        print('PF cost savings')
#        cost_savings(cost_list_4_no_bess, cost_list_determ_1, cost_list_determ_4)
#        print('MPC cost savings')
#        cost_savings(cost_list_4_no_bess, cost_list_mpc_1, cost_list_mpc_4)

#        plot_case_1_3(mpc_problem, cost_list_4_no_bess, cost_list_mpc_1, cost_list_determ_1, cost_list_mpc_4, cost_list_determ_4, filename=sys.argv[1])
    return
    


if __name__ == '__main__':
    main()

