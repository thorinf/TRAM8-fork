import torch
import torch.nn as nn

bias = nn.Parameter(torch.tensor(0.0))
optimizer = torch.optim.Adam([bias], lr=0.01)

for i in range(2000):
    pitches = torch.arange(0, 128)
    voltages = (pitches / 12.0) + bias.sigmoid()

    v_ref, bits = 8, 12
    levels = 2**bits
    lsb = v_ref / (levels - 1)

    lower_dac = torch.floor(voltages / lsb)
    upper_dac = torch.ceil(voltages / lsb)

    lower_voltages = lower_dac * lsb
    upper_voltages = upper_dac * lsb

    dac_voltages = torch.stack([lower_voltages, upper_voltages], dim=1)

    # Pick the closest voltage
    nearest_voltages = torch.where(torch.abs(lower_voltages - voltages) < torch.abs(upper_voltages - voltages), lower_voltages, upper_voltages)

    # Calculate the voltage difference
    voltage_error = nearest_voltages - voltages

    # Calculate the cents
    cents = 100 * (voltage_error * 12)

    # Mask the DAC voltages the exceeed v_ref
    mask = nearest_voltages <= v_ref

    # Compute the error
    error = (cents.abs() * mask.float()).sum() / mask.sum()

    # Backward pass
    optimizer.zero_grad()
    error.backward()
    optimizer.step()

    if i % 100 == 0:
        print(f"Epoch: {i:3d}, Error: {error.item():0.5f}, Bias: {bias.sigmoid().item():0.5f}")