import os
import numpy as np

bottleneckRates = np.arange(5., 50., 5.)  # Mbps
noiseSizes = [1000, 3000, 500]  # np.arange(600, 1200, 250)  # bytes

for bottleneckRate in bottleneckRates:
    noiseRates = np.arange(0.1, 1.1, 0.2) * bottleneckRate
    print(noiseRates)
    for noiseRate in noiseRates:
        for noiseSize in noiseSizes:
            noiseInterval = np.ceil((noiseSize * 8) / (noiseRate * 1e3))
            print(f'bottleneck {bottleneckRate} Mbps, noise {noiseRate} Mbps {noiseSize} bytes {noiseInterval} ms')
            os.chdir('..')
            os.system(f'./ns3 run "network_topology --meanNoiseInterval={noiseInterval} --meanNoiseSize={noiseSize} --bottleneckRate={bottleneckRate} --verbose=False"')
            os.chdir('masticc')
            os.system(f'python3 process_pcap.py {bottleneckRate}')
