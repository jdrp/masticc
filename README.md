# masticc

First install cppyy 2.4.2 `pip install cppyy==2.4.2`

Clone to ns-3-xxx/masticc

Move network_topology.cc to ns-3-xxx/scratch

Move random_noise_client to ns-3-xxx/contrib

Run `./ns3 build`

Run `./ns3 run network_topology`

Requirements for the lstm model are:
  - Pytorch
  - Pandas
  - Numpy
  - scikit-learn
  - matplotlib
