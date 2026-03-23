import numpy as np
import pandas as pd
from datetime import datetime
import matplotlib.pyplot as plt

# Your raw data
data = {
    'timestamp': [
        "2025-06-13 09:54:00",
        "2025-06-13 10:02:00",
        "2025-06-13 10:09:00",
        "2025-06-13 10:17:00",
        "2025-06-13 10:24:00",
        "2025-06-13 10:32:00",
        "2025-06-13 10:39:00",
        "2025-06-13 10:46:00",
    ],
    'reading_1': [
        10.14728292,
        9.703585598,
        9.67660977,
        9.676103389,
        9.59301696,
        9.574690758,
        9.721960407,
        9.639514141
    ]
}

df = pd.DataFrame(data)
df['timestamp'] = pd.to_datetime(df['timestamp'])

# Calculate delta_t in minutes
df['delta_t'] = df['timestamp'].diff().dt.total_seconds().fillna(0) / 60

# Initial state: [depth, rate of change]
x = np.array([[df['reading_1'].iloc[0]], [0]])
P = np.array([[1, 0], [0, 1]])

H = np.array([[1, 0]])       # Only measure depth
R = np.array([[0.01]])       # Measurement noise
Q = np.array([[0.001, 0], [0, 0.001]])  # Process noise

depth_estimates = []
rate_estimates = []

for i in range(len(df)):
    z = np.array([[df['reading_1'].iloc[i]]])
    dt = df['delta_t'].iloc[i]
    if dt == 0: dt = 1  # Handle first row or duplicates
    
    # State transition matrix
    F = np.array([[1, dt],
                  [0, 1]])
    
    # Prediction
    x = F @ x
    P = F @ P @ F.T + Q
    
    # Kalman Gain
    S = H @ P @ H.T + R
    K = P @ H.T @ np.linalg.inv(S)
    
    # Update
    y = z - H @ x
    x = x + K @ y
    P = (np.eye(2) - K @ H) @ P
    
    depth_estimates.append(x[0, 0])
    rate_estimates.append(x[1, 0])

# Add to DataFrame
df['filtered_depth'] = depth_estimates
df['estimated_rate'] = rate_estimates

# 📈 Plotting
plt.figure(figsize=(12, 5))

plt.subplot(1, 2, 1)
plt.plot(df['timestamp'], df['reading_1'], label='Raw Depth', linestyle='dotted')
plt.plot(df['timestamp'], df['filtered_depth'], label='Filtered Depth', linewidth=2)
plt.xlabel("Time")
plt.ylabel("Depth")
plt.title("River Depth (Kalman Filter)")
plt.legend()

plt.subplot(1, 2, 2)
plt.plot(df['timestamp'], df['estimated_rate'], color='orange', label='Estimated Rate')
plt.axhline(0, color='gray', linestyle='--')
plt.xlabel("Time")
plt.ylabel("Rate (m/min)")
plt.title("Estimated Rate of Depth Change")
plt.legend()

plt.tight_layout()
plt.show()
