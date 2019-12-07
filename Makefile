APP = nrf5340

OSDIR = mdepx

CMD = python3 -B ${OSDIR}/tools/emitter.py

all:
	@${CMD} mdepx.conf
	${CROSS_COMPILE}objcopy -O ihex obj/${APP}.elf obj/${APP}.hex
	${CROSS_COMPILE}size obj/${APP}.elf

clean:
	@rm -rf obj/*

include ${OSDIR}/mk/user.mk
