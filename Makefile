MAJOR= $(shell cat version| head -n 1)
MINOR= $(shell cat version| tail -n 1)

ARCH=X86_64
LINK=SHARED

#ARCH= AARCH64
#LINK=STATIC

CXX_AARCH64 = aarch64-linux-gnu-g++
CXX_X86_64 = x86_64-linux-gnu-g++

CXX = $(CXX_$(ARCH))
CXXFLAGS =	-O2  -Wall -Wno-array-bounds -Wno-unused-result\
	 -Wno-register -fmessage-length=0 -std=c++17 -I$(INCLUDE_DIR) -DMAJORNUM=$(MAJOR) -DMINORNUM=$(MINOR)

INCLUDE_DIR= include
SRC_DIR=src
OBJ_DIR=obj

LEX=flex
YACC=bison

BISONFLAGS= -d   
BISONSRC= syntax/interp.y
BISONTGT= interp.cpp 

FLEXFLAGS= -Pinterp -+
FLEXSRC= syntax/interp.l
FLEXTGT= interpson_lexer.cpp


_OBJ = wadu.o kids.o wadu_instrumentation.o interpson_lexer.o\
 interp.o syntools.o wadu_interp.o varmap.o wiring.o tracing.o
OBJ = $(patsubst %,$(OBJ_DIR)/%,$(_OBJ)) 


LIBS-SHARED= -lpthread -lanjson
LIBS-STATIC= -lpthread -L libs/$(ARCH)/ -l:libanjson.a

LIBS = $(LIBS-$(LINK))
TARGET = wadu


$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(SRC_DIR)/$(BISONTGT) $(SRC_DIR)/$(FLEXTGT)
	$(CXX) $(CXXFLAGS)  -c -o $@  $<

$(TARGET):	$(OBJ)
	$(CXX) -o $(TARGET) $(OBJ) $(LIBS)

$(SRC_DIR)/$(FLEXTGT) : $(FLEXSRC) 
	$(LEX)   $(FLEXFLAGS) -o$(FLEXTGT)   $(FLEXSRC) 
	mv $(FLEXTGT) $(SRC_DIR)

$(SRC_DIR)/$(BISONTGT): $(BISONSRC)
	$(YACC)  $(BISONFLAGS) -o $(BISONTGT) $(BISONSRC)	
	mv $(BISONTGT) $(SRC_DIR)
	mv *hpp *hh $(INCLUDE_DIR)


all: $(TARGET)

clean:
	rm -f $(OBJ) $(TARGET) $(SRC_DIR)/$(FLEXTGT) $(SRC_DIR)/$(BISONTGT)\
	$(INCLUDE_DIR)/interp.hpp 	$(INCLUDE_DIR)/stack.hh
