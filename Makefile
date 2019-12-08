APP = nrf5340

OSDIR = mdepx

CMD = python3 -B ${OSDIR}/tools/emitter.py

app:
	@${CMD} mdepx-app.conf
	${CROSS_COMPILE}objcopy -O ihex obj/${APP}-app.elf obj/${APP}-app.hex
	${CROSS_COMPILE}size obj/${APP}-app.elf

net:
	@${CMD} mdepx-net.conf
	${CROSS_COMPILE}objcopy -O ihex obj/${APP}-net.elf obj/${APP}-net.hex
	${CROSS_COMPILE}size obj/${APP}-net.elf

clean:
	@rm -rf obj/*

include ${OSDIR}/mk/user.mk
