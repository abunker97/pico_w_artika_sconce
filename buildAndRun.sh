cd ./build/

# if fails return to parent dir and exit
if ! ninja
then
    cd ..
    exit 1
fi

cd ..


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

gdb-multiarch ./build/pico_w_artika_sconce.elf -ex "target remote localhost:3333" -ex "load" -ex "monitor reset init" -ex "tui enable"
