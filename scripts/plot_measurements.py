import argparse
import datetime
import sys

class TimeSeries:
    def __init__(self):
        self.time = []
        self.value = []

    def push_value(self, t, y):
        self.time.append(t)
        self.value.append(y)

def read_data(lines):
    data = {}
    for i, line in enumerate(lines):
        items = line.split(" ")
        time = items[0]
        if time[0] != "[" or time[-1] != "]":
            print(f"Invalid time format ({time})", file=sys.stderr)
            continue
        time = int(time[1:-1], base=10)
        time = datetime.datetime.fromtimestamp(time)
        values = items[1:]
        for pair in values:
            k, v = pair.split("=")
            if not k in data:
                series = TimeSeries()
                data[k] = series
            else:
                series = data[k]
            series.push_value(time, float(v))
    return data

def main():
    parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument("--input", default="temp_readings.csv", type=str, help="Filepath to csv data")
    args = parser.parse_args()

    def get_data():
        with open(args.input, "r") as fp:
            return read_data(fp)
        
    import matplotlib as mpl
    import matplotlib.pyplot as plt
    import matplotlib.dates as mpl_dates
    import mplcursors
    while True:
        data = get_data()
        last_date = max((series.time[-1] for series in data.values()))
        fig = plt.figure(figsize=(10,5))
        ax = fig.add_subplot(1,1,1)
        ax.plot(data["t"].time, data["t"].value, label="Temperature ('C)", marker=".")
        ax.plot(data["h"].time, data["h"].value, label="Humidity (%)", marker=".")
        ADC_SCALE = 33/1024
        ax.plot(data["a"].time, [v*ADC_SCALE for v in data["a"].value], label="ADC*10 (V)", marker=".")
        ax.set_xlabel("Time (seconds)")
        ax.get_xaxis().set_minor_locator(mpl.ticker.AutoMinorLocator())
        ax.get_xaxis().set_major_formatter(mpl_dates.DateFormatter("%H:%M:%S"))
        ax.set_title(f"Temperature/Humidity ({last_date.strftime('%H:%M:%S')})")
        ax.grid(True)
        ax.legend()
        mplcursors.cursor(hover=True)
        plt.show()


if __name__ == "__main__":
    main()

