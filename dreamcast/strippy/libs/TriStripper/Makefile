LIB_NAME = libTriStripper.a

HEADER_FILES = \
    include/public_types.h \
    include/tri_stripper.h

DETAIL_FILES = \
    include/detail/cache_simulator.h \
    include/detail/connectivity_graph.h \
    include/detail/graph_array.h \
    include/detail/heap_array.h \
    include/detail/policy.h \
    include/detail/types.h

SRC_FILES = \
    src/connectivity_graph.cpp \
    src/policy.cpp \
    src/tri_stripper.cpp

INCLUDE_DIRS = include

OBJ_FILES = $(SRC_FILES:.cpp=.o)

.PHONY: all clean

all: $(LIB_NAME)

$(LIB_NAME): $(OBJ_FILES)
	ar rcs $@ $^

%.o: %.cpp $(HEADER_FILES) $(DETAIL_FILES)
	g++ -c $< -o $@ -I$(INCLUDE_DIRS)

clean:
	rm -f $(OBJ_FILES) $(LIB_NAME)