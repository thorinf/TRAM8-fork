import math
import numpy as np


def pitch2voltage(noteon):
    return noteon / 12.0


def voltage2pitch(voltage):
    return 12.0 * voltage


def dac_quantise(voltage, v_ref=8, bits=12):
    levels = 2**bits
    lsb = v_ref / (levels - 1)
    dac = np.round(voltage / lsb)
    voltage = dac * lsb
    return dac, voltage


def pitch_tracking(input_pitch, target_pitch):
    cents = 100 * (input_pitch - target_pitch)
    return cents


def to_lookup(dac12_values):
    # Printing in C array format with 12 values per lin
    print(f"\nuint16_t pitch_lookup[{len(dac12_values)}]" + " = {")
    for i, value in enumerate(dac12_values):
        uint16_t = hex(value << 4)[2:].zfill(4)
        if i % 12 == 0 and i != 0:
            print()
        print(f"0x{uint16_t.upper()}", end=", " if i < len(dac12_values) - 1 else "")
    print("};\n")



def main():
    noteOns = np.arange(0, 128)
    biases = np.linspace(0.0, 1.0, 1024)

    noteOns_biased = noteOns[np.newaxis, :] + biases[:, np.newaxis]

    voltages = pitch2voltage(noteOns_biased)
    dac_values, nearest_voltages = dac_quantise(voltages)
    nearest_pitches = voltage2pitch(nearest_voltages)
    cents = pitch_tracking(nearest_pitches, noteOns_biased)

    in_range = dac_values < 2**12
    coverage = np.sum(in_range, axis=-1)
    mean_detune = np.sum(np.abs(cents) * in_range, axis=-1) / np.sum(in_range, axis=-1)

    print(f"\nCoverage: {coverage}")
    print(f"\nAverage Cents: {mean_detune}")

    semitone_bias = np.mean(cents) / 100
    noteOns_biased = noteOns + semitone_bias
    voltages_biased = pitch2voltage(noteOns_biased)
    dac_values, nearest_voltages = dac_quantise(voltages_biased)
    nearest_pitches = voltage2pitch(nearest_voltages)
    cents = pitch_tracking(nearest_pitches, noteOns_biased)
    mean_detune = np.mean(np.abs(cents))
    print(f"\nAverage Cents: {np.mean(mean_detune)}")

    print()

    # tunings = []
    # dav_values = []
    # for pitch in range(128):
    #     voltage = pitch2voltage(pitch)
    #     dac, nearest_voltage = dac_quantise(voltage)
    #     nearest_pitch = voltage2pitch(nearest_voltage)
    #     cents = pitch_tracking(nearest_pitch, pitch)
        
    #     if dac >= 2**12:
    #         print(f"\nNoteon: {pitch:3d}, DAC value exceeds 12-bit range")
    #         break

    #     print(f"Noteon: {pitch:3d}, Nearest: {nearest_pitch:0.3f}, Cents: {cents:0.5f}")
    #     dav_values.append(dac)
    #     tunings.append(abs(cents))

    # print(f"\nAverage Cents: {sum(tunings) / len(tunings):0.5f}")



if __name__ == "__main__":
    main()
