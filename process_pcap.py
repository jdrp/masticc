from glob import glob
import statistics
import pickle
from scapy.all import rdpcap, IP, UDP
import numpy as np
import matplotlib.pyplot as plt
from typing import Optional


def smooth_data(data, window_size=5):
    smoothed = np.convolve(data, np.ones(window_size) / window_size, mode='valid')
    return list(smoothed)


def calculate_derivatives(data):
    first_order_derivative = np.diff(data, n=1).tolist()
    second_order_derivative = np.diff(data, n=2).tolist()
    return first_order_derivative, second_order_derivative


def process_pcap(pcap_file) -> Optional[dict]:
    sending_times = {}
    receiving_times = {}
    latencies = []

    packets = rdpcap(pcap_file)

    for packet in packets:
        if IP in packet and UDP in packet:
            ip_packet = packet[IP]

            if ip_packet.id in sending_times:
                receiving_times[ip_packet.id] = packet.time
                latencies.append(packet.time - sending_times[ip_packet.id])
            else:
                sending_times[ip_packet.id] = packet.time

    if not latencies:
        print(f"No latency values found in {pcap_file}")
        return None

    avg_latency = statistics.mean(latencies)
    std_dev_latency = statistics.stdev(latencies)
    latencies_smoothed = smooth_data(latencies)
    first_order_deriv, second_order_deriv = calculate_derivatives(latencies_smoothed)

    packet_loss = len(sending_times) - len(receiving_times)

    return {
        'avg_latency': avg_latency,
        'std_dev_latency': std_dev_latency,
        'latencies': latencies,
        'latencies_smoothed': latencies_smoothed,
        'first_order_deriv': first_order_deriv,
        'second_order_deriv': second_order_deriv,
        'packet_loss': packet_loss
    }


# Example usage
pcap_files = glob('../traces/client*.pcap')
all_data = []

for pcap_file in pcap_files:
    print(f"Processing {pcap_file}")
    data = process_pcap(pcap_file)
    if data:
        all_data.append(data)

with open('training_data/latency_data.pkl', 'wb') as f:
    pickle.dump(all_data, f)

print(all_data)


plt.figure(figsize=(10, 6))
plt.plot(all_data[0]['latencies'], color='green', label='Raw')
plt.plot(all_data[0]['latencies_smoothed'], color='blue', label='Smoothed')
plt.xlabel('Time')
plt.ylabel('Latency')
plt.title('Latency Over Time')
plt.legend()
#plt.show()


plt.figure(figsize=(10, 6))
plt.plot(all_data[0]['first_order_deriv'], color='green', label='1st deriv')
plt.xlabel('Time')
plt.ylabel('Latency 1st Derivative')
plt.title('Latency Over Time')
plt.legend()
#plt.show()

plt.figure(figsize=(10, 6))
plt.plot(all_data[0]['second_order_deriv'], color='green', label='2nd deriv')
plt.xlabel('Time')
plt.ylabel('Latency 2nd Derivative')
plt.title('Latency Over Time')
plt.legend()
plt.show()
