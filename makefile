# Compiler
CXX = g++

# Compiler flags
CXXFLAGS = -std=c++11 -Wall -Wextra

# Output executable
TARGET = Tiny-OS

# Source files
SRCS = Tiny_OS.cpp Tiny_fileSystem.cpp Tiny_inode.cpp Tiny_openFileManager.cpp Tiny_system.cpp Tiny_user.cpp Tiny_cache.cpp Tiny_disk.cpp Tiny_file.cpp Tiny_buf.cpp Tiny_DiskNode.cpp

# Header files
HDRS = Tiny_fileSystem.h Tiny_inode.h Tiny_openFileManager.h Tiny_system.h Tiny_user.h Tiny_cache.h Tiny_disk.h Tiny_file.h Tiny_buf.h Tiny_DiskNode.h

# Object files
OBJS = $(SRCS:.cpp=.o)

# Default target
all: $(TARGET) post-build

# Link the target executable
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

# Compile the source files into object files
%.o: %.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) -c $< -o $@


# Post-build target to remove object files
post-build:
	rm -f $(OBJS)

# Clean up object files only
clean-obj:
	rm -f $(OBJS)

# Clean up build files (executable and object files)
clean:
	rm -f $(TARGET) $(OBJS)

# Phony targets
.PHONY: all clean clean-obj
