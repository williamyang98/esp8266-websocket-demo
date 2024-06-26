import argparse
import datetime
import sys
import numpy as np

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

def filter_values(x, alpha=0.1, init_value=None):
    N = len(x)
    y = np.zeros((N,), dtype=np.float32)
    if init_value is None:
        N_avg = min(len(x), 50)
        init_value = sum(x[:N_avg])/N_avg
    v = init_value
    for i in range(0, N):
        v = v*(1-alpha) + x[i]*alpha
        y[i] = v
    return y

def main():
    parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument("--input", default="temp_readings.csv", type=str, help="Filepath to csv data")
    parser.add_argument("--lowpass", default=0.1, type=float, help="Amount to lowpass temp/humidity readings")
    args = parser.parse_args()

    if args.lowpass < 0 or args.lowpass > 1:
        print(f"lowpass filter takes values between 0 and 1, but got {args.lowpass:.2f}", file=sys.stderr)
        return

    def get_data():
        with open(args.input, "r") as fp:
            return read_data(fp)

    import matplotlib as mpl
    import matplotlib.pyplot as plt
    import matplotlib.ticker as mpl_ticker
    import matplotlib.dates as mpl_dates
    import mplcursors

    fig = plt.figure(figsize=(11.69,8.27))

    def refresh_plot():
        data = get_data()
        last_date = max((series.time[-1] for series in data.values()))
        if args.lowpass < 1:
            temperature = filter_values(data["t"].value, alpha=args.lowpass)
            humidity = filter_values(data["h"].value, alpha=args.lowpass)
        else:
            temperature = data["t"].value
            humidity = data["h"].value

        fig.clear()
        ax = fig.add_subplot(1,1,1)
        ax.plot(data["t"].time, temperature, label="Temperature ('C)", marker=".")
        ax.plot(data["h"].time, humidity, label="Humidity (%)", marker=".")
        ADC_SCALE = 33/1024
        ax.plot(data["a"].time, [v*ADC_SCALE for v in data["a"].value], label="Fan Voltage (V)", marker=".")
        ax.set_ylim(ymin=0)
        ax.grid(True, which="both")
        ax.get_yaxis().set_minor_locator(mpl_ticker.MultipleLocator(1))
        ax.get_yaxis().set_major_locator(mpl_ticker.MultipleLocator(5))
        ax.get_xaxis().set_minor_locator(mpl_dates.HourLocator(interval=1))
        # ax.get_xaxis().set_major_locator(mpl_dates.HourLocator(interval=2))
        ax.get_xaxis().set_major_formatter(mpl_dates.DateFormatter("%H:%M:%S"))
        ax.set_xlabel("Time")
        ax.set_title(f"Fridge Measurements ({last_date})")
        ax.legend()
        mplcursors.cursor(hover=True)
        plt.show()

    def on_click(ev):
        if ev.dblclick:
            refresh_plot()
    plt.connect("button_press_event", on_click)
    refresh_plot()

if __name__ == "__main__":
    main()

