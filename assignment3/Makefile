.phony all:
all: disk

disk:
	gcc diskinfo.c diskhelpers.c -o diskinfo
	gcc disklist.c diskhelpers.c -o disklist
	gcc diskget.c diskhelpers.c -o diskget
	gcc diskput.c diskhelpers.c -o diskput

.PHONY clean:
clean:
	-rm -rf *.o *.exe
