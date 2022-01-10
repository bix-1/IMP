# File: Makefile
# Brief: Makefile for IMP project at FIT BUT 2021
#
# Project: Piano simulator with play, record & replay
#
# Authors: Jakub Bartko    xbartk07@stud.fit.vutbr.cz

LOGIN = xbartk07

pack: $(LOGIN).tar

$(LOGIN).tar: *.cpp Makefile ../platformio.ini
	tar -cvf $@ $^ --transform 's,^,$(LOGIN)/src/,'
	cd ../doc && tar --append --file=../src/$@ documentation.pdf --transform 's,^,$(LOGIN)/,'

clean:
	rm -f $(EXEC) *.o *.tar
