#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>

#define MAX_NAME 256
#define MAX_COMMAND 1024
#define MAX_COMPONENTS 100
#define MAX_ARGS 64
#define MAX_PARTS 10

// Component types
typedef enum {
    TYPE_NODE,
    TYPE_PIPE,
    TYPE_CONCATENATE,
    TYPE_STDERR,
    TYPE_FILE
} ComponentType;

// Node structure
typedef struct {
    char name[MAX_NAME];
    char command[MAX_COMMAND];
} Node;

// Pipe structure
typedef struct {
    char name[MAX_NAME];
    char from[MAX_NAME];
    char to[MAX_NAME];
} Pipe;

// Concatenate structure
typedef struct {
    char name[MAX_NAME];
    int parts;
    char part_names[MAX_PARTS][MAX_NAME];
} Concatenate;

// Stderr structure
typedef struct {
    char name[MAX_NAME];
    char from[MAX_NAME];
} Stderr;

// File structure (Extra Credit)
typedef struct {
    char name[MAX_NAME];
    char filename[MAX_NAME];
} File;

// Generic component structure
typedef struct {
    ComponentType type;
    union {
        Node node;
        Pipe pipe;
        Concatenate concat;
        Stderr stderr;
        File file;
    } data;
} Component;

// Global storage
Component components[MAX_COMPONENTS];
int component_count = 0;

// Function declarations
void parse_flow_file(const char *filename);
Component* find_component(const char *name);
void execute_component(Component *comp);
void execute_node(Node *node);
void execute_pipe(Pipe *pipe_comp);
void execute_concatenate(Concatenate *concat);
void execute_stderr(Stderr *serr);
char** parse_command(const char *command);
void free_args(char **args);

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <flow_file> <target>\n", argv[0]);
        return 1;
    }

    const char *flow_file = argv[1];
    const char *target = argv[2];

    // Parse the flow file
    parse_flow_file(flow_file);

    // Find the target component
    Component *target_comp = find_component(target);
    if (target_comp == NULL) {
        fprintf(stderr, "Error: Target '%s' not found\n", target);
        return 1;
    }

    // Execute the target component
    execute_component(target_comp);

    return 0;
}

// Step 13: Parse the flow file
void parse_flow_file(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        perror("Error opening file");
        exit(1);
    }

    char line[1024];
    Component *current = NULL;

    while (fgets(line, sizeof(line), fp)) {
        // Remove newline
        line[strcspn(line, "\n")] = 0;
        
        // Skip empty lines
        if (strlen(line) == 0) continue;
        
        // Find the '=' separator
        char *equals = strchr(line, '=');
        if (equals == NULL) continue;
        
        *equals = '\0';
        char *key = line;
        char *value = equals + 1;
        
        // Parse based on key
        if (strcmp(key, "node") == 0) {
            // Create new node
            current = &components[component_count++];
            current->type = TYPE_NODE;
            strncpy(current->data.node.name, value, MAX_NAME - 1);
            current->data.node.name[MAX_NAME - 1] = '\0';
            
        } else if (strcmp(key, "command") == 0) {
            // Add command to current node
            if (current && current->type == TYPE_NODE) {
                strncpy(current->data.node.command, value, MAX_COMMAND - 1);
                current->data.node.command[MAX_COMMAND - 1] = '\0';
            }
            
        } else if (strcmp(key, "pipe") == 0) {
            // Create new pipe
            current = &components[component_count++];
            current->type = TYPE_PIPE;
            strncpy(current->data.pipe.name, value, MAX_NAME - 1);
            current->data.pipe.name[MAX_NAME - 1] = '\0';
            
        } else if (strcmp(key, "from") == 0) {
            // Add 'from' to current pipe or stderr
            if (current && current->type == TYPE_PIPE) {
                strncpy(current->data.pipe.from, value, MAX_NAME - 1);
                current->data.pipe.from[MAX_NAME - 1] = '\0';
            } else if (current && current->type == TYPE_STDERR) {
                strncpy(current->data.stderr.from, value, MAX_NAME - 1);
                current->data.stderr.from[MAX_NAME - 1] = '\0';
            }
            
        } else if (strcmp(key, "to") == 0) {
            // Add 'to' to current pipe
            if (current && current->type == TYPE_PIPE) {
                strncpy(current->data.pipe.to, value, MAX_NAME - 1);
                current->data.pipe.to[MAX_NAME - 1] = '\0';
            }
            
        } else if (strcmp(key, "concatenate") == 0) {
            // Create new concatenate
            current = &components[component_count++];
            current->type = TYPE_CONCATENATE;
            strncpy(current->data.concat.name, value, MAX_NAME - 1);
            current->data.concat.name[MAX_NAME - 1] = '\0';
            
        } else if (strcmp(key, "parts") == 0) {
            // Set number of parts
            if (current && current->type == TYPE_CONCATENATE) {
                current->data.concat.parts = atoi(value);
            }
            
        } else if (strncmp(key, "part_", 5) == 0) {
            // Add part to concatenate
            if (current && current->type == TYPE_CONCATENATE) {
                int part_num = atoi(key + 5);
                if (part_num >= 0 && part_num < MAX_PARTS) {
                    strncpy(current->data.concat.part_names[part_num], value, MAX_NAME - 1);
                    current->data.concat.part_names[part_num][MAX_NAME - 1] = '\0';
                }
            }
            
        } else if (strcmp(key, "stderr") == 0) {
            // Create new stderr
            current = &components[component_count++];
            current->type = TYPE_STDERR;
            strncpy(current->data.stderr.name, value, MAX_NAME - 1);
            current->data.stderr.name[MAX_NAME - 1] = '\0';
            
        } else if (strcmp(key, "file") == 0) {
            // Create new file (Extra Credit)
            current = &components[component_count++];
            current->type = TYPE_FILE;
            strncpy(current->data.file.name, value, MAX_NAME - 1);
            current->data.file.name[MAX_NAME - 1] = '\0';
            
        } else if (strcmp(key, "name") == 0) {
            // Add filename to file component (Extra Credit)
            if (current && current->type == TYPE_FILE) {
                strncpy(current->data.file.filename, value, MAX_NAME - 1);
                current->data.file.filename[MAX_NAME - 1] = '\0';
            }
        }
    }

    fclose(fp);
}

// Step 14: Find component by name
Component* find_component(const char *name) {
    for (int i = 0; i < component_count; i++) {
        Component *comp = &components[i];
        
        switch (comp->type) {
            case TYPE_NODE:
                if (strcmp(comp->data.node.name, name) == 0)
                    return comp;
                break;
            case TYPE_PIPE:
                if (strcmp(comp->data.pipe.name, name) == 0)
                    return comp;
                break;
            case TYPE_CONCATENATE:
                if (strcmp(comp->data.concat.name, name) == 0)
                    return comp;
                break;
            case TYPE_STDERR:
                if (strcmp(comp->data.stderr.name, name) == 0)
                    return comp;
                break;
            case TYPE_FILE:
                if (strcmp(comp->data.file.name, name) == 0)
                    return comp;
                break;
        }
    }
    return NULL;
}

// Step 15: Parse command into arguments
char** parse_command(const char *command) {
    char **args = malloc(MAX_ARGS * sizeof(char*));
    if (args == NULL) {
        perror("malloc");
        exit(1);
    }
    
    char *command_copy = strdup(command);
    if (command_copy == NULL) {
        perror("strdup");
        exit(1);
    }
    
    int argc = 0;
    char *ptr = command_copy;
    char *start;
    char quote_char = 0;
    
    while (*ptr && argc < MAX_ARGS - 1) {
        // Skip whitespace
        while (*ptr && (*ptr == ' ' || *ptr == '\t')) {
            ptr++;
        }
        
        if (*ptr == '\0') break;
        
        start = ptr;
        
        // Check if starting with a quote
        if (*ptr == '\'' || *ptr == '\"') {
            quote_char = *ptr;
            start++; // Skip the opening quote
            ptr++;
            
            // Find closing quote
            while (*ptr && *ptr != quote_char) {
                ptr++;
            }
            
            if (*ptr == quote_char) {
                *ptr = '\0'; // Null terminate
                ptr++;
            }
        } else {
            // No quote, find next space
            while (*ptr && *ptr != ' ' && *ptr != '\t') {
                ptr++;
            }
            
            if (*ptr) {
                *ptr = '\0';
                ptr++;
            }
        }
        
        args[argc] = strdup(start);
        if (args[argc] == NULL) {
            perror("strdup");
            exit(1);
        }
        argc++;
    }
    
    args[argc] = NULL;
    
    free(command_copy);
    return args;
}

void free_args(char **args) {
    if (args == NULL) return;
    
    for (int i = 0; args[i] != NULL; i++) {
        free(args[i]);
    }
    free(args);
}

// Step 16: Execute a node (command)
void execute_node(Node *node) {
    char **args = parse_command(node->command);
    
    execvp(args[0], args);
    
    // If we get here, execvp failed
    fprintf(stderr, "Error executing '%s': %s\n", args[0], strerror(errno));
    free_args(args);
    exit(1);
}

// Step 17: Execute a pipe
void execute_pipe(Pipe *pipe_comp) {
    // Find source and destination components
    Component *from_comp = find_component(pipe_comp->from);
    Component *to_comp = find_component(pipe_comp->to);
    
    if (from_comp == NULL) {
        fprintf(stderr, "Error: Component '%s' not found\n", pipe_comp->from);
        exit(1);
    }
    if (to_comp == NULL) {
        fprintf(stderr, "Error: Component '%s' not found\n", pipe_comp->to);
        exit(1);
    }
    
    // Create pipe
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(1);
    }
    
    // Fork for source component
    pid_t pid1 = fork();
    if (pid1 == -1) {
        perror("fork");
        exit(1);
    }
    
    if (pid1 == 0) {
        // Child 1: Execute source
        close(pipefd[0]);  // Close read end
        dup2(pipefd[1], STDOUT_FILENO);  // Redirect stdout to pipe
        close(pipefd[1]);
        
        // If source is a node, execute it directly
        if (from_comp->type == TYPE_NODE) {
            execute_node(&from_comp->data.node);
        } else {
            // Otherwise, recursively execute
            execute_component(from_comp);
        }
        exit(0);
    }
    
    // Fork for destination component
    pid_t pid2 = fork();
    if (pid2 == -1) {
        perror("fork");
        exit(1);
    }
    
    if (pid2 == 0) {
        // Child 2: Execute destination
        close(pipefd[1]);  // Close write end
        dup2(pipefd[0], STDIN_FILENO);  // Redirect stdin from pipe
        close(pipefd[0]);
        
        if (to_comp->type == TYPE_NODE) {
            execute_node(&to_comp->data.node);
        } else {
            execute_component(to_comp);
        }
        exit(0);
    }
    
    // Parent: close both ends and wait
    close(pipefd[0]);
    close(pipefd[1]);
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
}

// Step 18: Execute concatenate
void execute_concatenate(Concatenate *concat) {
    for (int i = 0; i < concat->parts; i++) {
        Component *part = find_component(concat->part_names[i]);
        if (part == NULL) {
            fprintf(stderr, "Error: Part '%s' not found\n", concat->part_names[i]);
            exit(1);
        }
        
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            exit(1);
        }
        
        if (pid == 0) {
            // Child: execute this part
            execute_component(part);
            exit(0);
        }
        
        // Parent: wait for this part to complete before next
        int status;
        waitpid(pid, &status, 0);
    }
}

// Step 19: Execute stderr redirection
void execute_stderr(Stderr *serr) {
    Component *from_comp = find_component(serr->from);
    if (from_comp == NULL) {
        fprintf(stderr, "Error: Component '%s' not found\n", serr->from);
        exit(1);
    }
    
    if (from_comp->type != TYPE_NODE) {
        fprintf(stderr, "Error: stderr can only be applied to nodes\n");
        exit(1);
    }
    
    // Fork to execute the node with stderr redirected
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(1);
    }
    
    if (pid == 0) {
        // Child: Redirect stderr to stdout
        dup2(STDOUT_FILENO, STDERR_FILENO);
        
        // Execute the node
        execute_node(&from_comp->data.node);
        exit(0);
    }
    
    // Parent: wait for child
    waitpid(pid, NULL, 0);
}

// Step 20: Main execute component dispatcher
void execute_component(Component *comp) {
    switch (comp->type) {
        case TYPE_NODE:
            execute_node(&comp->data.node);
            break;
            
        case TYPE_PIPE:
            execute_pipe(&comp->data.pipe);
            break;
            
        case TYPE_CONCATENATE:
            execute_concatenate(&comp->data.concat);
            break;
            
        case TYPE_STDERR:
            execute_stderr(&comp->data.stderr);
            break;
            
        default:
            fprintf(stderr, "Error: Unknown component type\n");
            exit(1);
    }
}
