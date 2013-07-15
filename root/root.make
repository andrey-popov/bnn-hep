libCROOT.a: CTFile.o CTTree.o CTDirectory.o CTRandom.o
	rm -f libROOT.a
	ar -cvq libCROOT.a CTFile.o CTTree.o CTDirectory.o CTRandom.o

CTFile.o: CTFile.h CTFile.cxx
	$(CC) $(CFLAGS) -c CTFile.cxx -o CTFile.o

CTTree.o: CTTree.h CTTree.cxx
	$(CC) $(CFLAGS) -c CTTree.cxx -o CTTree.o

CTDirectory.o: CTDirectory.h CTDirectory.cxx
	$(CC) $(CFLAGS) -c CTDirectory.cxx -o CTDirectory.o

CTRandom.o: CTRandom.h CTRandom.cxx
	$(CC) $(CFLAGS) -c CTRandom.cxx -o CTRandom.o

CTFile.h:
	ln -s ../root/CTFile.h .

CTFile.cxx:
	ln -s ../root/CTFile.cxx .

CTTree.h:
	ln -s ../root/CTTree.h .

CTTree.cxx:
	ln -s ../root/CTTree.cxx .

CTDirectory.h:
	ln -s ../root/CTDirectory.h .

CTDirectory.cxx:
	ln -s ../root/CTDirectory.cxx .

CTRandom.h:
	ln -s ../root/CTRandom.h .

CTRandom.cxx:
	ln -s ../root/CTRandom.cxx .
