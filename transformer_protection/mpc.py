def getDates(self, startDate):
    for timestep in range(self.num_timesteps):
        for day in range(self.forecast_days):
            yield startDate - datetime.timedelta(days  = self.forecast_days) + \
                              datetime.timedelta(hours = timestep * self.dt) + \
                              datetime.timedelta(days  = day)


def return_forecast(self, start, plotting=False):
        forecast_df = pd.DataFrame(columns = ['datetime', 'solar_mean', 'load_mean'])
        forecast_df.loc[0, 'datetime'] = start 
        for timestep in range(1, self.num_timesteps):
            forecast_df.loc[timestep, 'datetime'] = forecast_df.loc[0, 'datetime'] + timestep * datetime.timedelta(hours=self.dt)

        for timestep in range(self.num_timesteps):
            avg_idxs = []
            for day in range(self.forecast_days):

                avg_idxs.append(np.where(self.data.loc[:, 'datetime'] == start - datetime.timedelta(days=self.forecast_days) + datetime.timedelta(hours = timestep * self.dt) + datetime.timedelta(days=day))[0][0])
            forecast_df.loc[timestep, 'solar_mean'] = np.average(self.data.loc[avg_idxs, 'solar'])
            forecast_df.loc[timestep, 'load_mean'] = np.average(self.data.loc[avg_idxs, 'loads'])

        if plotting:
            # a bit of plotting
            plt.figure()
            for i in range(self.forecast_days):
                idxs = []
                for j in range(self.num_timesteps) :
                    idxs.append(np.where(self.data.loc[:, 'datetime']==start - datetime.timedelta(days=self.forecast_days) + datetime.timedelta(days = i) + datetime.timedelta(hours = j * self.dt))[0][0])
                plt.plot(forecast_df.datetime, self.data.loc[idxs , 'solar'])
            plt.plot(forecast_df.datetime, forecast_df.solar_mean, label = 'avg')
            plt.legend()
            plt.show()


            plt.figure()
            idxs = []
            for j in range(self.num_timesteps):
                idxs.append(np.where(self.data.loc[:, 'datetime']==start + datetime.timedelta(hours = j * self.dt))[0][0])
            plt.plot(self.data.loc[idxs, 'datetime'], self.data.loc[idxs , 'solar'], label  = 'actual')
            plt.plot(forecast_df.datetime, forecast_df.solar_mean, label = 'avg')
            plt.legend()
            plt.title('Solar')
            plt.show()

            plt.figure()
            idxs = []
            for j in range(self.num_timesteps):
                idxs.append(np.where(self.data.loc[:, 'datetime']==start + datetime.timedelta(hours = j * self.dt))[0][0])
            plt.plot(self.data.loc[idxs, 'datetime'], self.data.loc[idxs, 'loads'], label = 'Actual')
            plt.plot(forecast_df.datetime, forecast_df.load_mean, label = 'Prediction')
            plt.legend()
            plt.title('Loads')
            plt.show()
        return forecast_df
