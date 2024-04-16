from scapy.all import rdpcap, IP, UDP
import numpy as np
import matplotlib.pyplot as plt
from typing import Optional
import csv
import os
import sys


def smooth_data(data, window_size=4):
    smoothed = np.convolve(data, np.ones(window_size) / window_size, mode='valid')
    add_before = int(np.ceil((window_size - 1) / 2))
    smoothed = np.concatenate((np.full(add_before, smoothed[0]),
                               smoothed,
                               np.full(window_size - 1 - add_before, smoothed[-1])))
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
    # print(len(rolling_packet_loss))
    return rolling_packet_loss


def process_client_pcap(pcap_file) -> Optional[dict]:
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


def filter_packets_from_ip(packets, ip):
    return [packet for packet in packets if IP in packet and UDP in packet and packet[IP].src == ip]


def calculate_bw_ratio(r1_pcap, r2_pcap, client_ip, total_bw):
    bw_ratio = []
    r1_packets = filter_packets_from_ip(rdpcap(r1_pcap), client_ip)
    r2_packets = filter_packets_from_ip(rdpcap(r2_pcap), client_ip)
    for r1_packet, r2_packet in zip(r1_packets, r2_packets):
        delay = float(r2_packet.time - r1_packet.time)  # - 0.018432
        packet_size = len(r2_packet[IP]) * 8.0  # bytes to bits
        available_bw = packet_size / delay
        bw_ratio.append(available_bw / total_bw)
    return bw_ratio


client_ip = "192.168.1.2"
client_pcap_file = '../traces/client.pcap'
r1_pcap_file = '../traces/router1.pcap'
r2_pcap_file = '../traces/router2.pcap'
bottleneck_bw = 1.0e6 if len(sys.argv) != 2 else float(sys.argv[1]) * 1.0e6  # 1Mbps

# print(f"Processing {client_pcap_file}")
latency_data = process_client_pcap(client_pcap_file)
# print(f"Processing {r1_pcap_file}, {r2_pcap_file}")
bw_ratio = calculate_bw_ratio(r1_pcap_file, r2_pcap_file, client_ip, bottleneck_bw)
latency_data.update({'bw_ratio': bw_ratio})

# with open('training_data/latency_data.pkl', 'wb') as f:
#     pickle.dump(latency_data, f)

csv_path ='training_data/latency_data.csv'
with open(csv_path, 'a' if (file_exists := os.path.exists(csv_path)) else 'w') as outfile:
    writer = csv.writer(outfile)
    if not file_exists:
        writer.writerow(latency_data.keys())
    writer.writerows(list(zip(*latency_data.values()))[4:-4])

# plt.figure(figsize=(10, 6))
# plt.plot(latency_data['ts'], latency_data['latencies'], color='green', label='Raw')
# plt.plot(latency_data['ts'], latency_data['latencies_smoothed'], color='blue', label='Smoothed')
# plt.xlabel('Time')
# plt.ylabel('Latency')
# plt.title('Latency Over Time')
# plt.legend()
# # plt.show()
#
#
# plt.figure(figsize=(10, 6))
# plt.plot(latency_data['ts'], latency_data['bw_ratio'], color='green', label='1st deriv')
# plt.xlabel('Time')
# plt.ylabel('Latency 1st Derivative')
# plt.title('Latency Over Time')
# plt.legend()
# # plt.show()
#
#
# plt.figure(figsize=(10, 6))
# plt.plot(latency_data['ts'], latency_data['first_order_deriv'], color='green', label='1st deriv')
# plt.xlabel('Time')
# plt.ylabel('Latency 1st Derivative')
# plt.title('Latency Over Time')
# plt.legend()
# # plt.show()
#
# plt.figure(figsize=(10, 6))
# plt.plot(latency_data['ts'], latency_data['second_order_deriv'], color='green', label='2nd deriv')
# plt.xlabel('Time')
# plt.ylabel('Latency 2nd Derivative')
# plt.title('Latency Over Time')
# plt.legend()
#
# plt.figure(figsize=(10, 6))
# plt.plot(latency_data['ts'], latency_data['mean_latency'], color='green', label='Mean latency')
# plt.xlabel('Time')
# plt.ylabel('Mean latency')
# plt.title('Latency Over Time')
# plt.legend()
#
# plt.figure(figsize=(10, 6))
# plt.plot(latency_data['ts'], latency_data['stdev_latency'], color='green', label='Std Dev latency')
# plt.xlabel('Time')
# plt.ylabel('Std Dev Latency')
# plt.title('Latency Over Time')
# plt.legend()
#
# plt.figure(figsize=(10, 6))
# plt.plot(latency_data['ts'], latency_data['packet_loss'], color='green', label='Packet loss')
# plt.xlabel('Time')
# plt.ylabel('Packet loss')
# plt.title('Packet loss Over Time')
# plt.legend()
# plt.show()
