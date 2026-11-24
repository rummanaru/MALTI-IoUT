# MALTI-IoUT
Material Aware Learner model for Transport-layer Intelligence in Underground IoT Network

# MALTI-IoUT
Code and data for "MALTI: A Material-Aware Learner Model for Transport Layer Intelligence in Underground IoT Networks" (IEEE CCNC 2027, IoTime workshop).

- ns3/src/propagation/model/: UG-NS changes to ns-3.36.1 propagation loss and delay models (concrete depth, VWC, bulk density). Copy over the stock files.
- ns3/scratch/node_iter_flow_modular2_UDP.cc: scenario generator (5 AGGs, 52 UGDs, uplink UDP, 50 packets per flow). Topology: coord_10Ap_flex_time.csv (first 5 clusters).
- ns3/run_segment.sh: runs segments 0-5 (depth group x traffic batch).
- data/: 1,200 per-flow CSVs (6,000 scenarios x 52 flows = 312,000 records), zipped by segment.
  Delay (ns) and Jitter (ns) are FlowMonitor per-flow sums (delaySum, jitterSum).
- ml/: training, corner-case and plotting scripts (scikit-learn, XGBoost).
