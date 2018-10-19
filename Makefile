WORK_DIR=$(shell pwd)
cc=gcc
bin=httpd
src=httpd.c
LDFLAGS=-lpthread

.PHONY:all
all:$(bin) cgi

$(bin):$(src)
	$(cc) -o $@ $^ $(LDFLAGS)

cgi:
	cd wwwroot/cgi; make clean;make; cd -

.PHONY:clean
clean:
	rm -f $(bin)
	cd wwwroot/cgi; make clean; cd -

