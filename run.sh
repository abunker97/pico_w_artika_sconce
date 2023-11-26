if ls $SCREENDIR | grep "openocd" >> /dev/null
then
    echo "Killing old OpenOcd Server"
    screen -XS openocd kill
    sleep 0.5
fi

echo "Starting OpenOcd Server"

if ! screen -dms openocd ./launchOpenOCD.sh;
then
    echo "Error starting OpenOcd"
    exit 1
else
    echo "OpenOcd server started"
fi

gdb-multiarch ./build/picowota_pico_w_artika_sconce.elf -ex "target remote localhost:3333" -ex "load" -ex "monitor reset init" -ex "tui enable"
