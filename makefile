# =============================================================================
# Makefile for the Parallel Floorplanner Project (with src/inc separation)
# =============================================================================

# --- Compiler and Flags ---
CXX := g++
CXXFLAGS := -std=c++17 -O3 -Wall -fopenmp -MMD -MP
LDFLAGS := -fopenmp

# --- Project Structure ---
EXEC := floorplanner
SRCDIR := src
INCDIR := inc 
OBJDIR := obj

CXXFLAGS += -I$(INCDIR)

# --- Automatic File Discovery ---
SOURCES := $(wildcard $(SRCDIR)/*.cpp)
OBJECTS := $(SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
DEPS := $(OBJECTS:.o=.d)

# =============================================================================
# Targets
# =============================================================================

# --- Default Target ---
all: $(EXEC)

# --- Linking Target ---
$(EXEC): $(OBJECTS)
	@echo "�쵲�i������..."
	$(CXX) $^ -o $@ $(LDFLAGS)
	@echo "�ظm�����C�i�����ɬ� '$(EXEC)'."

# --- Compilation Target ---
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(OBJDIR)
	@echo "�sĶ $<..."
	$(CXX) $(CXXFLAGS) -c $< -o $@

# --- Clean Target ---
clean:
	@echo "���b�M�z..."
	rm -rf $(OBJDIR) $(EXEC)
	@echo "�M�z�����C"

# --- Phony Targets ---
.PHONY: all clean

# --- Include Dependencies ---
# �ޤJ�Ҧ� .d �̿����Y�ɮסC
# -include ���O�|�������s�b���ɮסA�o�b�Ĥ@���ظm�ɫܦ��ΡC
-include $(DEPS)