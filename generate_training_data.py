import os
import numpy as np

bottleneckRates = [50.] # np.arange(5., 51., 5.)  # Mbps
noiseSizes = np.arange(600, 1200, 250)  # bytes
os.chdir('..')
os.system('cp masticc/network_topology.cc scratch/')

for bottleneckRate in bottleneckRates:
    noiseRates = np.arange(0.1, 1.2, 0.2) * bottleneckRate
    print(noiseRates)
    for noiseRate in noiseRates:
        for noiseSize in noiseSizes:
            noiseInterval = (noiseSize * 8) / (noiseRate * 1e3)
            print(f'bottleneck {bottleneckRate} Mbps, noise {noiseRate} Mbps {noiseSize} bytes {noiseInterval} ms')
            os.system(f'./ns3 run "network_topology --meanNoiseInterval={noiseInterval} --meanNoiseSize={noiseSize} --bottleneckRate={bottleneckRate} --bottleneckDelay=0 --verbose=False"')
            os.chdir('masticc')
            os.system(f'python3 process_pcap.py {bottleneckRate}')
            os.chdir('..')
