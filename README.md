# Pipe Flow Implementation

## Design Overview

### Architecture
This program implements an interpreter for `.flow` files that execute process pipelines similar to Unix shell pipes.

**Components:**
1. **Parser**: Reads `.flow` files line-by-line and populates a global array of Component structures
2. **Executor**: Recursively resolves component dependencies and creates appropriate processes
3. **Process Manager**: Uses `fork()`, `pipe()`, `dup2()`, and `execvp()` to manage processes

### Data Structures

- **Component Union**: Used to store different component types (node, pipe, concatenate, stderr) in a single array
- **Global Component Array**: Stores all parsed components for easy lookup during execution

### Key Implementation Details

**Pipe Execution:**
- Creates a pipe using `pipe()`
- Forks two child processes
- Child 1: Redirects stdout to pipe write end, executes source
- Child 2: Redirects stdin from pipe read end, executes destination
- Parent: Closes both ends, waits for both children

**Concatenate Execution:**
- Executes each part sequentially
- Waits for each part to complete before starting the next
- Parts are independent (output of one doesn't feed into another)

**Stderr Handling:**
- Uses `dup2()` to redirect file descriptor 2 (stderr) to descriptor 1 (stdout)
- Always applied to a node component

### Challenges Faced
1. Properly closing file descriptors to avoid deadlocks
2. Handling recursive component execution
3. Parsing commands with spaces correctly

## Testing

### Test Cases Included

**filecount.flow**: Basic pipe test (ls | wc)
**complicated.flow**: Concatenate with nested pipe
**your_tests.flow**: Chained pipes (pipe → pipe → node)

## Extra Credit Test: your_tests.flow

**Component**: `count_flows`

**What it tests**: Chaining pipes where the source of a pipe is another pipe (not just a node)

**Why it's challenging**: 
Most students might only test direct node-to-node pipes. This tests whether the recursive execution properly handles pipes as sources, which requires correctly managing multiple levels of fork/pipe/wait sequences.

The flow creates:
- `list_long` (ls -la) → `filter_flows` (pipe to grep) → `grep_flow` (grep flow)
- `filter_flows` → `count_flows` (pipe to wc) → `word_count` (wc -l)

This tests that when a pipe's source is another pipe, the output is correctly threaded through multiple pipe connections.

## How to Build and Run

```bash
# Compile
make

# Run tests
make test      # Runs filecount.flow
make test2     # Runs complicated.flow

# Run your tests
./flow your_tests.flow count_flows

# Clean
make clean
```


