#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Function prototypes
void run_test(const char *cmd);
void run_comment_test(const char *input, const char *expected_output);
void run_redirection_test(const char *cmd, const char *expected_output_file, const char *expected_content);
void run_variable_test(const char *cmd, const char *expected_output);
void run_path_test(const char *cmd, const char *expected_output);

void run_history_test();
void run_history_set_test();
void run_history_execution_test();


int main() {
    int result;

    // Test 1: Batch mode with valid commands
    result = system("echo '/bin/pwd' > script.wsh");
    if (result != 0) { perror("Error creating script.wsh"); return result; }

    result = system("echo '/bin/ls' >> script.wsh");
    if (result != 0) { perror("Error appending to script.wsh"); return result; }

    result = system("echo '/usr/bin/whoami' >> script.wsh");
    if (result != 0) { perror("Error appending to script.wsh"); return result; }

    run_test("./wsh script.wsh");

    // Test 2: Batch mode with an empty file
    result = system("touch empty.wsh");
    if (result != 0) { perror("Error creating empty.wsh"); return result; }
    
    run_test("./wsh empty.wsh");

    // Test 3: Non-existent batch file
    run_test("./wsh non_existent.wsh");

    // Test 4: More than one argument
    run_test("./wsh arg1 arg2");

    // Test 5: Invalid command
    result = system("echo 'fakecmd' > invalid_cmd.wsh");
    if (result != 0) { perror("Error creating invalid_cmd.wsh"); return result; }

    run_test("./wsh invalid_cmd.wsh");

    // Comment tests:
    printf("\nRunning comment tests:\n");

    // Test 6: Line with just a comment
    run_comment_test("# This is a comment", "");

    // Test 7: Line with spaces followed by a comment
    run_comment_test("   # Comment with spaces", "");

    // Test 8: Line with a command and comment (the comment should be ignored)
    run_comment_test("/bin/echo hello # this is a comment", "hello\n");

    // Test 9: Empty line (not a comment, should do nothing)
    run_comment_test("", "");

    // Test 10: Line with spaces only
    run_comment_test("    ", "");

    // Redirection tests:
    printf("\nRunning redirection tests:\n");

    // Test 11: Output redirection (>)
    printf("Test 11: Output redirection\n");
    result = system("/bin/echo hello > test_output.txt");
    if (result != 0) { perror("Error creating test_output.txt"); return result; }
    run_redirection_test("/bin/echo hello > test_output.txt", "test_output.txt", "hello\n");

    // Test 12: Input redirection (<)
    printf("Test 12: Input redirection\n");
    result = system("/bin/echo 'This is a test input file.' > test_input.txt");
    if (result != 0) { perror("Error creating test_input.txt"); return result; }
    run_redirection_test("/bin/cat < test_input.txt", "test_input.txt", "This is a test input file.\n");

    // Test 13: Append output redirection (>>)
    printf("Test 13: Append output redirection\n");
    result = system("/bin/echo 'first line' > test_output.txt");
    if (result != 0) { perror("Error creating test_output.txt"); return result; }
    run_redirection_test("/bin/echo second line >> test_output.txt", "test_output.txt", "first line\nsecond line\n");

    // Test 14: Redirect stdout and stderr (&>)
    printf("Test 14: Redirect stdout and stderr\n");
    run_redirection_test("/bin/ls non_existent_file &> test_output.txt", "test_output.txt", "/bin/ls: cannot access 'non_existent_file': No such file or directory\n");

    // Test 15: Append stdout and stderr (&>>)
    printf("Test 15: Append stdout and stderr\n");
    result = system("/bin/echo 'initial output' > test_output.txt");
    if (result != 0) { perror("Error creating test_output.txt"); return result; }
    run_redirection_test("/bin/ls non_existent_file &>> test_output.txt", "test_output.txt", "initial output\n/bin/ls: cannot access 'non_existent_file': No such file or directory\n");

    // Variable tests:
    printf("\nRunning variable tests:\n");

    // Test 16: Local variable assignment and echo
    run_variable_test("local myvar=/home/user\n/bin/echo $myvar\n", "/home/user\n");

    // Test 17: Environment variable assignment and echo
    run_variable_test("export PATH=/usr/bin\n/bin/echo $PATH\n", "/usr/bin\n");

    // Test 18: Displaying local variables using vars
    run_variable_test("local myvar=/home/user\n local othervar=123\n vars\n", "othervar=123\nmyvar=/home/user\n");

    // Test 19: Modifying local variable and echo
    run_variable_test("local myvar=/home/otheruser\n /bin/echo $myvar\n", "/home/otheruser\n");

    // Path tests:
    printf("\nRunning Path tests:\n");

    // Test 20: Execute a command from /bin (default $PATH)
    printf("Test 20: Execute a command from /bin\n");
    run_path_test("echo hello", "hello\n");

    // Test 21: Execute a command using a full path
    printf("Test 21: Execute a command using a full path\n");
    run_path_test("/bin/ls", NULL);  // We just expect it to run without an error

    // Test 22: Modify $PATH and execute a command
    printf("Test 22: Modify $PATH and execute a command\n");
    result = system("echo 'echo PATH test' > /tmp/myprogram");
    if (result != 0) { perror("Error creating /tmp/myprogram"); return result; }
    result = system("chmod +x /tmp/myprogram");  // Make it executable
    if (result != 0) { perror("Error making /tmp/myprogram executable"); return result; }
    run_path_test("export PATH=/tmp; myprogram", "PATH test\n");

    // Test 23: Non-existent command
    printf("Test 23: Non-existent command\n");
    run_path_test("nonexistentcmd", "nonexistentcmd: command not found\n");

    // Test 24: History command
    run_history_test();
    run_history_set_test();
    run_history_execution_test();
    
    printf("All tests finished.\n");

    
    // Cleanup
    result = system("rm script.wsh empty.wsh invalid_cmd.wsh output.txt test_script.wsh test_output.txt test_input.txt");
    if (result != 0) { perror("Error cleaning up test files"); return result; }

    
    return 0;
}

// Run a batch mode test
void run_test(const char *cmd) {
    printf("Running test: %s\n", cmd);
    int result = system(cmd);
    if (result != 0) {
        printf("Test failed with status %d.\n", result);
    } else {
        printf("Test passed.\n");
    }
}

// This function simulates running a command in the shell and compares output for comment tests
void run_comment_test(const char *input, const char *expected_output) {
    // Prepare a temporary file for batch mode testing
    FILE *script_file = fopen("test_script.wsh", "w");
    if (script_file == NULL) {
        perror("Failed to create test script");
        exit(1);
    }

    // Write the input line to the script file
    fprintf(script_file, "%s\n", input);
    fclose(script_file);

    // Run the shell in batch mode with the test script and check the result of system()
    int result = system("./wsh test_script.wsh > output.txt");
    if (result != 0) {
        perror("Failed to run wsh on test script");
        exit(1);
    }

    // Open the output file
    FILE *output_file = fopen("output.txt", "r");
    if (output_file == NULL) {
        perror("Failed to open output file");
        exit(1);
    }

    // Read the actual output from the shell
    char actual_output[1024] = {0};
    if (fgets(actual_output, sizeof(actual_output), output_file) == NULL && !feof(output_file)) {
        perror("Failed to read from output file");
        fclose(output_file);
        exit(1);
    }
    fclose(output_file);

    // Compare the actual output to the expected output
    if (strcmp(actual_output, expected_output) == 0) {
        printf("Comment test passed: '%s'\n", input);
    } else {
        printf("Comment test failed: '%s'\nExpected: '%s', but got: '%s'\n", input, expected_output, actual_output);
    }

    // Clean up the temporary files
    remove("test_script.wsh");
    remove("output.txt");
}

// Function to run the shell command in batch mode and check the output file content for redirection tests
void run_redirection_test(const char *cmd, const char *expected_output_file, const char *expected_content) {
    // Prepare a temporary batch file for the redirection test
    FILE *script_file = fopen("test_script.wsh", "w");
    if (script_file == NULL) {
        perror("Failed to create test script");
        exit(1);
    }

    // Write the command to the script file
    fprintf(script_file, "%s\n", cmd);
    fclose(script_file);

    // Run the shell in batch mode with the test script
    int result = system("./wsh test_script.wsh");
    if (result != 0) {
        perror("Failed to run shell command");
        exit(1);
    }

    // Open the output file and check its contents
    FILE *output_file = fopen(expected_output_file, "r");
    if (output_file == NULL) {
        perror("Failed to open output file");
        exit(1);
    }

    // Read the contents of the output file
    char actual_content[1024] = {0};
    size_t bytes_read = fread(actual_content, sizeof(char), sizeof(actual_content) - 1, output_file);
    if (bytes_read == 0 && !feof(output_file)) {
        perror("Failed to read from output file");
        fclose(output_file);
        exit(1);
    }
    fclose(output_file);

    // Compare the actual content with the expected content
    if (strcmp(actual_content, expected_content) == 0) {
        printf("Test passed: %s\n", cmd);
    } else {
        printf("Test failed: %s\nExpected: '%s', but got: '%s'\n", cmd, expected_content, actual_content);
    }

    // Clean up the temporary script file
    remove("test_script.wsh");
}

// Function to test local and environment variables
void run_variable_test(const char *cmd, const char *expected_output) {
    printf("Running variable test: %s\n", cmd);

    // Create a temporary script for testing
    FILE *script_file = fopen("test_script.wsh", "w");
    if (script_file == NULL) {
        perror("Failed to create test script");
        exit(1);
    }

    // Write the command to the script file
    fprintf(script_file, "%s\n", cmd);
    fclose(script_file);

    // Run the shell in batch mode and redirect the output
    int result = system("./wsh test_script.wsh > output.txt");
    if (result != 0) {
        perror("Failed to run wsh on test script");
        exit(1);
    }

    // Open the output file
    FILE *output_file = fopen("output.txt", "r");
    if (output_file == NULL) {
        perror("Failed to open output file");
        exit(1);
    }

    // Read the actual output from the shell
    char actual_output[1024] = {0};
    if (fgets(actual_output, sizeof(actual_output), output_file) == NULL && !feof(output_file)) {
        perror("Failed to read from output file");
        fclose(output_file);
        exit(1);
    }
    fclose(output_file);

    // Compare the actual output to the expected output
    if (strcmp(actual_output, expected_output) == 0) {
        printf("Variable test passed: '%s'\n", cmd);
    } else {
        printf("Variable test failed: '%s'\nExpected: '%s', but got: '%s'\n", cmd, expected_output, actual_output);
    }

    // Clean up temporary files
    remove("test_script.wsh");
    remove("output.txt");
}

void run_path_test(const char *cmd, const char *expected_output) {
    // Prepare a temporary script for testing
    FILE *script_file = fopen("test_script.wsh", "w");
    if (script_file == NULL) {
        perror("Failed to create test script");
        exit(1);
    }

    // Write the command to the script file
    fprintf(script_file, "%s\n", cmd);
    fclose(script_file);

    // Run the shell in batch mode with the test script and redirect the output
    int result = system("./wsh test_script.wsh > output.txt 2>&1");  // Capture both stdout and stderr
    if (result != 0 && expected_output == NULL) {
        printf("Test failed for command: %s\n", cmd);
        return;
    }

    // Open the output file and check its contents
    FILE *output_file = fopen("output.txt", "r");
    if (output_file == NULL) {
        perror("Failed to open output file");
        exit(1);
    }

    // Read the actual output from the shell
    char actual_output[1024] = {0};
    if (fgets(actual_output, sizeof(actual_output), output_file) == NULL && !feof(output_file)) {
        perror("Failed to read from output file");
        fclose(output_file);
        exit(1);
    }
    fclose(output_file);

    // Compare the actual output with the expected output
    if (expected_output != NULL && strcmp(actual_output, expected_output) == 0) {
        printf("Test passed: %s\n", cmd);
    } else if (expected_output == NULL) {
        printf("Test passed: %s (no output expected)\n", cmd);
    } else {
        printf("Test failed: %s\nExpected: '%s', but got: '%s'\n", cmd, expected_output, actual_output);
    }

    // Clean up
    remove("test_script.wsh");
    remove("output.txt");
}

// Test 1: Basic history functionality
void run_history_test() {
    printf("\nRunning history tests:\n");

    // Simulate executing commands and storing them in history
    run_test("./wsh -c 'echo first'");
    run_test("./wsh -c 'echo second'");
    run_test("./wsh -c 'echo third'");
    run_test("./wsh -c 'echo second'");  // This should not be added as a duplicate
    run_test("./wsh -c 'history'");

    // Expected output:
    // 1) echo first
    // 2) echo second
    // 3) echo third
}

// Test 2: History set functionality
void run_history_set_test() {
    printf("\nRunning history set tests:\n");

    // Resize history to 2 and execute commands
    run_test("./wsh -c 'history set 2'");
    run_test("./wsh -c 'echo fourth'");
    run_test("./wsh -c 'echo fifth'");
    run_test("./wsh -c 'history'");

    // Expected output:
    // 1) echo fourth
    // 2) echo fifth

    // Now set history back to 5 and continue
    run_test("./wsh -c 'history set 5'");
    run_test("./wsh -c 'echo sixth'");
    run_test("./wsh -c 'history'");

    // Expected output:
    // 1) echo fifth
    // 2) echo sixth
}

// Test 3: Executing commands from history
void run_history_execution_test() {
    printf("\nRunning history execution tests:\n");

    // Execute a command from history
    run_test("./wsh -c 'echo seventh'");
    run_test("./wsh -c 'history 2'");  // This should re-execute "echo sixth"

    // Expected output for the last command: sixth

    run_test("./wsh -c 'history'");

    // Expected output:
    // 1) echo sixth
    // 2) echo seventh
}
