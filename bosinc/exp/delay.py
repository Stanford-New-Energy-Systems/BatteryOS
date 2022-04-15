import datetime
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt

def calc_overhead(bos_latency: float, ping_latency: float) -> float:
    total_latency = bos_latency + ping_latency
    return bos_latency / total_latency

def main(aggrDelayFile: str, chainDelayFile: str, pingDelayFile = "ping_latencies.csv", exp_name = "BOS Overhead") -> None:
    # load data and save to array
    aggr_delay_data  = pd.read_csv(aggrDelayFile)
    chain_delay_data = pd.read_csv(chainDelayFile)
    ping_delay_data  = pd.read_csv(pingDelayFile)

    # take median of the five aggregate samples
    #aggr_delay_median = aggr_delay_data.groupby(["Aggregate"]).median()

    # take minimum of the five aggregate samples
    aggr_delay_min = aggr_delay_data.groupby(["Aggregate"]).min()

    # change latency units to microseconds
    row_total = aggr_delay_min.shape[0]
    for row_index in range(row_total):
        aggr_delay_min.iloc[row_index] *= 1e-3

    # add ping delay latency to aggr_delay to get total delay
    #for row_index in range(row_total):
    #    aggr_delay_min.iloc[row_index] += ping_delay_data.loc[row_index, "Expected Latency"]
    
    # calculate overhead from bos latency
    for row_index in range(row_total):
        aggr_delay_min.iloc[row_index] = calc_overhead(aggr_delay_min.iloc[row_index], ping_delay_data.loc[row_index, "Expected Latency"])

    # reset index so that dataframe has "Aggregate" as column instead of index
    aggr_delay_min = aggr_delay_min.reset_index()    

    # set up plot
    ax = plt.gca()
    ax.clear

    # plot data
    #ping_delay_data.plot(kind='line', x='Number of Batteries', y='Expected Latency', ax=ax, label='Ping Latency')
    aggr_delay_min.plot(kind='line', x="Aggregate", y="Latency", ax=ax, label='Overhead Percentage')

    # add ping delay latency to aggr_delay to get total delay
    #for row_index in range(row_total):
    #    aggr_delay_min.iloc[row_index] += ping_delay_data.loc[row_index, "Expected Latency"]
 
    #aggr_delay_min.plot(kind='line', x="Aggregate", y="Latency", ax=ax, label='Combined Latency')

    lg=plt.legend(loc='center left', bbox_to_anchor=(.8, 0.5))
    
    #plt.ylabel('Latency [\u03BCs]')
    plt.ylabel('Overhead [%]')
    plt.xlabel('Aggregate Batteries')
    plt.title(exp_name)
    save_path='osdi22/plots/'+exp_name+'.png'
    plt.savefig(save_path,dpi=400, 
            format='png', 
            bbox_extra_artists=(lg,), 
            bbox_inches='tight')
   

if __name__ == "__main__":
    main("aggdelay.csv", "chaindelay.csv")
