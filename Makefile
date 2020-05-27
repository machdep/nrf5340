APP = nrf5340

OSDIR = mdepx

CMD = python3 -B ${OSDIR}/tools/emitter.py

all:	app net

app:
	@${CMD} -j src-app/mdepx.conf
	${CROSS_COMPILE}objcopy -O ihex obj/${APP}-app.elf obj/${APP}-app.hex
	${CROSS_COMPILE}size obj/${APP}-app.elf

net:
	@${CMD} -j src-net/mdepx.conf
	${CROSS_COMPILE}objcopy -O ihex obj/${APP}-net.elf obj/${APP}-net.hex
	${CROSS_COMPILE}size obj/${APP}-net.elf

dtb-app:
	cpp -nostdinc -Imdepx/dts -Imdepx/dts/arm -Imdepx/dts/common	\
	    -Imdepx/dts/include -undef, -x assembler-with-cpp		\
	    nrf5340_app.dts -O obj/nrf5340_app.dts
	dtc -I dts -O dtb obj/nrf5340_app.dts -o obj/nrf5340_app.dtb
	bin2hex.py --offset=1015808 obj/nrf5340_app.dtb obj/nrf5340_app.dtb.hex
	nrfjprog -f NRF53 --erasepage 0xf8000-0xfc000
	nrfjprog -f NRF53 --program obj/nrf5340_app.dtb.hex -r

dtb-net:
	cpp -nostdinc -Imdepx/dts -Imdepx/dts/arm -Imdepx/dts/common	\
	    -Imdepx/dts/include -undef, -x assembler-with-cpp		\
	    nrf5340_net.dts -O obj/nrf5340_net.dts
	dtc -I dts -O dtb obj/nrf5340_net.dts -o obj/nrf5340_net.dtb
	bin2hex.py --offset=17022976 obj/nrf5340_net.dtb obj/nrf5340_net.dtb.hex
	nrfjprog -f NRF53 --coprocessor CP_NETWORK	\
	    --erasepage 0x0103c000-0x01040000
	nrfjprog -f NRF53 --coprocessor CP_NETWORK	\
	    --program obj/nrf5340_net.dtb.hex -r

flash-app:
	nrfjprog -f NRF53 --erasepage 0x40000-0x60000
	nrfjprog -f NRF53 --program obj/nrf5340-app.hex -r

flash-net:
	nrfjprog -f NRF53 --coprocessor CP_NETWORK	\
	    --erasepage 0x01000000-0x0103c000
	nrfjprog -f NRF53 --coprocessor CP_NETWORK	\
	    --program obj/nrf5340-net.hex -r

reset:
	nrfjprog -f NRF53 -r

clean:
	@rm -rf obj/*

include ${OSDIR}/mk/user.mk
