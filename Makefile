CC = gcc

CFLAGS = -Wall -Wextra -fPIE -pie

LDFLAGS = -lSDL2 -lSDL2_ttf -lm

BIN_DIR = bin

SRCS = contor.c julia.c burningship.c kochsnowflake.c sierpinskitriangle.c mandelbrot.c

TARGET_NAMES = $(SRCS:.c=)

TARGETS = $(addprefix $(BIN_DIR)/,$(TARGET_NAMES))

.PHONY: all clean help $(BIN_DIR) $(TARGET_NAMES)

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)
	@echo "Created directory: $(BIN_DIR)/"

all: $(BIN_DIR) $(TARGETS)
	@echo "--- All fractal programs compiled and placed in '$(BIN_DIR)/' directory. ---"

$(BIN_DIR)/contor: contor.c $(BIN_DIR)
	@echo "Compiling and linking $< to $@..."
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)
	@if [ -f "$@" ]; then echo "SUCCESS: Executable '$@' created."; else echo "FAILURE: Executable '$@' NOT created. Check errors above."; fi

$(BIN_DIR)/julia: julia.c $(BIN_DIR)
	@echo "Compiling and linking $< to $@..."
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)
	@if [ -f "$@" ]; then echo "SUCCESS: Executable '$@' created."; else echo "FAILURE: Executable '$@' NOT created. Check errors above."; fi

$(BIN_DIR)/burningship: burningship.c $(BIN_DIR)
	@echo "Compiling and linking $< to $@..."
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)
	@if [ -f "$@" ]; then echo "SUCCESS: Executable '$@' created."; else echo "FAILURE: Executable '$@' NOT created. Check errors above."; fi

$(BIN_DIR)/kochsnowflake: kochsnowflake.c $(BIN_DIR)
	@echo "Compiling and linking $< to $@..."
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)
	@if [ -f "$@" ]; then echo "SUCCESS: Executable '$@' created."; else echo "FAILURE: Executable '$@' NOT created. Check errors above."; fi

$(BIN_DIR)/sierpinskitriangle: sierpinskitriangle.c $(BIN_DIR)
	@echo "Compiling and linking $< to $@..."
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)
	@if [ -f "$@" ]; then echo "SUCCESS: Executable '$@' created."; else echo "FAILURE: Executable '$@' NOT created. Check errors above."; fi

$(BIN_DIR)/mandelbrot: mandelbrot.c $(BIN_DIR)
	@echo "Compiling and linking $< to $@..."
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)
	@if [ -f "$@" ]; then echo "SUCCESS: Executable '$@' created."; else echo "FAILURE: Executable '$@' NOT created. Check errors above."; fi

julia: $(BIN_DIR)/julia
contor: $(BIN_DIR)/contor
burningship: $(BIN_DIR)/burningship
kochsnowflake: $(BIN_DIR)/kochsnowflake
sierpinskitriangle: $(BIN_DIR)/sierpinskitriangle
mandelbrot: $(BIN_DIR)/mandelbrot

# Clean target: Removes all compiled executables and generated .bmp screenshots
clean:
	@echo "Cleaning up..."
	@rm -f $(TARGETS) # Remove all executables from the bin directory
	@rm -f *.bmp      # Remove any .bmp screenshot files from the current directory
	@rmdir $(BIN_DIR) 2>/dev/null || true # Attempt to remove bin directory if empty, suppress errors if not.
	@echo "Cleanup complete."

# Help target: Displays usage information
help:
	@echo "-----------------------------------"
	@echo "  Fractal Programs Makefile Usage  "
	@echo "-----------------------------------"
	@echo "To compile all programs:"
	@echo "  make all             (Executables go to $(BIN_DIR)/)"
	@echo ""
	@echo "To compile a specific program (e.g., make julia):"
	@echo "  make <program_name>  (Executable goes to $(BIN_DIR)/)"
	@echo "  (e.g., make julia, make burningship, make contor, make kochsnowflake, make sierpinskitriangle, make mandelbrot)"
	@echo ""
	@echo "To remove all compiled executables and screenshots:"
	@echo "  make clean"
	@echo ""
	@echo "To see this help message:"
	@echo "  make help"
	@echo "-----------------------------------"
