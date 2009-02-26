#
# USRP tests makefile
#

LDFLAGS	= -lusrp

sources	:= src/usrp_init_test.cc

objects		= $(patsubst %.cc,%.cc.o,$(sources))
programs	= $(patsubst %.cc,%,$(sources))

all: $(programs)

$(objects): $(sources)
	g++ -Wall -g -O2 -c $< -o $@

$(programs): $(objects)
	g++ -Wall -g -O2 $(LDFLAGS) $< -o $@

clean:
	$(RM) $(objects)
	$(RM) $(programs)
