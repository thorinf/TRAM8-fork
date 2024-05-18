import math


def pitch2voltage(noteon):
    return noteon / 12.0


def voltage2pitch(voltage):
    return 12.0 * voltage


def dac_quantise(voltage, v_ref=8, bits=12):
    levels = 2**bits
    lsb = v_ref / (levels - 1)
    dac = round(voltage / lsb)
    voltage = dac * lsb
    return dac, voltage


def pitch_tracking(input_pitch, target_pitch):
    cents = 100 * (input_pitch - target_pitch)
    return cents


def main():
    tunings = []
    dav_values = []
    for pitch in range(128):
        voltage = pitch2voltage(pitch)
        dac, nearest_voltage = dac_quantise(voltage)
        nearest_pitch = voltage2pitch(nearest_voltage)
        cents = pitch_tracking(nearest_pitch, pitch)
        
        if dac >= 2**12:
            print(f"\nNoteon: {pitch:3d}, DAC value exceeds 12-bit range")
            break

        print(f"Noteon: {pitch:3d}, Nearest: {nearest_pitch:0.3f}, Cents: {cents:0.5f}")
        dav_values.append(dac)
        tunings.append(abs(cents))

    print(f"\nAverage Cents: {sum(tunings) / len(tunings):0.5f}")

    # Printing in C array format with 12 values per line
    print(f"\nuint16_t pitch_lookup[{len(dav_values)}]" + " = {")
    for i, value in enumerate(dav_values):
        uint16_t = hex(value << 4)[2:].zfill(4)
        if i % 12 == 0 and i != 0:
            print()
        print(f"0x{uint16_t.upper()}", end=", " if i < len(dav_values) - 1 else "")
    print("};\n")


if __name__ == "__main__":
    main()
