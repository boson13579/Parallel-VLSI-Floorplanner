# =============================================================================
# Makefile for the Parallel Floorplanner Project (with src/inc separation)
# =============================================================================

# --- Compiler and Flags ---
CXX := g++
CXXFLAGS := -std=c++17 -O3 -Wall -fopenmp -MMD -MP
LDFLAGS := -fopenmp

# --- Configurable Defaults ---
TIME_LIMIT ?= 595
STRATEGY ?= MultiStart_Coarse

# --- Project Structure ---
EXEC := floorplanner
SRCDIR := src
INCDIR := inc 
OBJDIR := obj

CXXFLAGS += -I$(INCDIR)
CXXFLAGS += -DDEFAULT_TIME_LIMIT_SECONDS=$(TIME_LIMIT)
CXXFLAGS += -DDEFAULT_STRATEGY_NAME=\"$(STRATEGY)\"

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
	@echo "鏈結可執行檔..."
	$(CXX) $^ -o $@ $(LDFLAGS)
	@echo "建置完成。可執行檔為 '$(EXEC)'."

# --- Compilation Target ---
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(OBJDIR)
	@echo "編譯 $<..."
	$(CXX) $(CXXFLAGS) -c $< -o $@

# --- Clean Target ---

clean:
	@echo "正在清理..."
	rm -rf $(OBJDIR) $(EXEC)
	@echo "清理完成。"

# --- Phony Targets ---
.PHONY: all clean

# --- Include Dependencies ---
# 引入所有 .d 依賴關係檔案。
# -include 指令會忽略不存在的檔案，這在第一次建置時很有用。
-include $(DEPS)