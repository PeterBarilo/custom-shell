CC = gcc
CFLAGS = -Wall -Wextra -Werror -pedantic -std=gnu18
LOGIN = chengtao
SUBMITPATH = ~cs537-1/handin/$(LOGIN)/
TARGET = wsh
TEST_EXEC = tests

.PHONY: all clean submit

# Build both optimized and debug versions of the shell
all: $(TARGET) $(TARGET)-dbg $(TARGET)-asan

$(TARGET): $(TARGET).c $(TARGET).h
	$(CC) $(CFLAGS) -O2 -o $@ $< 

$(TARGET)-dbg: $(TARGET).c $(TARGET).h
	$(CC) $(CFLAGS) -Og -ggdb -o $@ $< 

# AddressSanitizer version
$(TARGET)-asan: $(TARGET).c $(TARGET).h
	$(CC) $(CFLAGS) -fsanitize=address -g -o $@ $<

# Build the test executable
# $(TEST_EXEC): $(TEST_EXEC).c
# 	$(CC) $(CFLAGS) -O2 -o $(TEST_EXEC) $(TEST_EXEC).c

# Clean up all generated files
clean: 
	rm -f $(TARGET) $(TARGET)-dbg $(TARGET)-asan

# Submit the project
submit: clean
	cp -r ../../p3 $(SUBMITPATH)

# Run batch mode tests by invoking the test program
# test: $(TEST_EXEC)
# 	@echo "Running tests..."
# 	./$(TEST_EXEC)

# # Test interactive mode simulation by using input redirection
# interactive_test: $(TARGET)
# 	@echo "Testing interactive mode with input redirection..."
# 	echo -e "pwd\nls\nexit" > input.txt
# 	./$(TARGET) < input.txt > interactive_output.txt
# 	@cat interactive_output.txt
# 	@rm -f input.txt

# # Clean up test output files only
# clean_test_outputs:
# 	@rm -f input.txt interactive_output.txt
