from glob import glob
import statistics
import pickle
from scapy.all import rdpcap, IP, UDP
import numpy as np
import matplotlib.pyplot as plt
from typing import Optional
import csv


def smooth_data(data, window_size=4):
    smoothed = np.convolve(data, np.ones(window_size) / window_size, mode='valid')
    smoothed = np.concatenate((np.full(window_size - 1, smoothed[0]), smoothed))
    return list(smoothed)


def calculate_stats(data, window_size=20):
    mean = smooth_data(data, window_size)
    stdeviation = np.subtract(data, mean)
    return mean, stdeviation


def calculate_derivatives(data):
    first_order_derivative = np.concatenate(([0], np.diff(data, n=1).tolist()))
    second_order_derivative = np.concatenate(([0, 0], np.diff(data, n=2).tolist()))
    return first_order_derivative, second_order_derivative


def calculate_packet_loss(sending_times: dict, receiving_times: dict, window_size=20):
    sorted_sending_times = sorted(sending_times.items(), key=lambda x: x[1])
    sorted_receiving_times = sorted(receiving_times.items(), key=lambda x: x[1])
    rolling_packet_loss = []
    window = []
    send_idx = 0

    for recv_key, recv_time in sorted_receiving_times:
        while send_idx < len(sorted_sending_times) and sorted_sending_times[send_idx][1] <= recv_time:
            window.append(sorted_sending_times[send_idx][0])
            send_idx += 1
        if len(window) > window_size:
            window = window[-window_size:]
        lost_packets = sum(1 for key in window if key not in receiving_times)
        packet_loss = lost_packets / min(window_size, len(window))
        rolling_packet_loss.append(packet_loss)
    print(len(rolling_packet_loss))
    return rolling_packet_loss


def process_pcap(pcap_file) -> Optional[dict]:
    sending_times = {}
    receiving_times = {}
    ts = []
    latencies = []

    packets = rdpcap(pcap_file)

    for packet in packets:
        if IP in packet and UDP in packet:
            ip_packet = packet[IP]

            if ip_packet.id in sending_times:
                receiving_times[ip_packet.id] = packet.time
                latencies.append(packet.time - sending_times[ip_packet.id])
                ts.append(packet.time)
            else:
                sending_times[ip_packet.id] = packet.time

    if not latencies:
        print(f"No latency values found in {pcap_file}")
        return None

    latencies_smoothed = smooth_data(latencies, 4)
    first_order_deriv, second_order_deriv = calculate_derivatives(latencies_smoothed)

    packet_loss = calculate_packet_loss(sending_times, receiving_times, 20)
    mean, stdev = calculate_stats(latencies_smoothed, 20)

    return {
        'ts': ts,
        'mean_latency': mean,
        'stdev_latency': stdev,
        'latencies': latencies,
        'latencies_smoothed': latencies_smoothed,
        'first_order_deriv': first_order_deriv,
        'second_order_deriv': second_order_deriv,
        'packet_loss': packet_loss
    }


# Example usage
pcap_file = glob('../traces/client*.pcap')[0]

print(f"Processing {pcap_file}")
data = process_pcap(pcap_file)

with open('training_data/latency_data.pkl', 'wb') as f:
    pickle.dump(data, f)

with open('training_data/latency_data.csv', 'w') as outfile:
    writer = csv.writer(outfile)
    writer.writerow(data.keys())
    writer.writerows(zip(*data.values()))

plt.figure(figsize=(10, 6))
plt.plot(data['ts'], data['latencies'], color='green', label='Raw')
plt.plot(data['ts'], data['latencies_smoothed'], color='blue', label='Smoothed')
plt.xlabel('Time')
plt.ylabel('Latency')
plt.title('Latency Over Time')
plt.legend()
# plt.show()


plt.figure(figsize=(10, 6))
plt.plot(data['ts'], data['first_order_deriv'], color='green', label='1st deriv')
plt.xlabel('Time')
plt.ylabel('Latency 1st Derivative')
plt.title('Latency Over Time')
plt.legend()
# plt.show()

plt.figure(figsize=(10, 6))
plt.plot(data['ts'], data['second_order_deriv'], color='green', label='2nd deriv')
plt.xlabel('Time')
plt.ylabel('Latency 2nd Derivative')
plt.title('Latency Over Time')
plt.legend()

plt.figure(figsize=(10, 6))
plt.plot(data['ts'], data['mean_latency'], color='green', label='Mean latency')
plt.xlabel('Time')
plt.ylabel('Mean latency')
plt.title('Latency Over Time')
plt.legend()

plt.figure(figsize=(10, 6))
plt.plot(data['ts'], data['stdev_latency'], color='green', label='Std Dev latency')
plt.xlabel('Time')
plt.ylabel('Std Dev Latency')
plt.title('Latency Over Time')
plt.legend()

plt.figure(figsize=(10, 6))
plt.plot(data['ts'], data['packet_loss'], color='green', label='Packet loss')
plt.xlabel('Time')
plt.ylabel('Packet loss')
plt.title('Packet loss Over Time')
plt.legend()
plt.show()
