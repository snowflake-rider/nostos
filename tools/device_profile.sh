#!/usr/bin/env bash

# Show serial ports first because this is the path used by idf.py/esptool.
printf '%s\n' 'Serial ports:'
serial_port_found=0

while IFS= read -r serial_port; do
    printf '  %s\n' "$serial_port"
    serial_port_found=1
done < <(
    find /dev -maxdepth 1 \
        \( -name 'cu.usb*' -o -name 'tty.usb*' \
        -o -name 'cu.SLAB*' -o -name 'tty.SLAB*' \
        -o -name 'cu.wchusb*' -o -name 'tty.wchusb*' \) \
        -print 2>/dev/null | sort
)

if ((serial_port_found == 0)); then
    printf '%s\n' '  No USB serial port found.'
fi

# system_profiler may return no SPUSBDataType output on some macOS systems.
# IORegistry still exposes the USB product, serial number, and VID/PID.
printf '\n%s\n' 'USB device details:'
ioreg -p IOUSB -w 0 -l | awk '
    /[+]\-o / {
        device = $0
        sub(/^.*[+]\-o /, "", device)
        sub(/  <class.*$/, "", device)
    }
    /"USB Product Name"|"USB Serial Number"|"idVendor"|"idProduct"/ {
        if (device != last_device) {
            printf "\n  %s\n", device
            last_device = device
        }
        line = $0
        sub(/^[[:space:]|]*/, "", line)
        printf "    %s\n", line
    }
'
