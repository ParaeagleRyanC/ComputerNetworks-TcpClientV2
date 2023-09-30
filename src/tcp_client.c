#include "log.h"
#include "tcp_client.h"
#include "ctype.h"

#define REQUIRED_NUMBER_OF_ARGUMENTS_OFFSET 1
#define ALL_OPTIONS_PARSED -1
#define NUMBER_OF_ACTIONS 5
#define PAYLOAD_MEMORY_BUFFER 10
#define DEFAULT_BUFFER_SIZE 1024
#define SHORT_OPTIONS "vh:p:"
#define HELP_MESSAGE "\n\
    Usage: tcp_client [--help] [-v] [-h HOST] [-p PORT] FILE\n\
    \n\
    Arguments:\n\
    FILE   A file name containing actions and messages to\n\
           send to the server. If \"-\" is provided, stdin will\n\
           be read\"\n\
    \n\
    Options:\n\
    --help\n\
    -v, --verbose\n\
    --host HOSTNAME, -h HOSTNAME\n\
    --port PORT, -p PORT\n"

/*
Description:
    Parses the commandline arguments and options given to the program.
Arguments:
    int argc: the amount of arguments provided to the program (provided by the main function)
    char *argv[]: the array of arguments provided to the program (provided by the main function)
    Config *config: An empty Config struct that will be filled in by this function.
Return value:
    Returns a 1 on failure, 0 on success
*/
int tcp_client_parse_arguments(int argc, char *argv[], Config *config) {

    // for debug
    log_debug("There are %d arguments and these are the arguments:\n", argc);
    for (int i = 0; i < argc; i++) {
        log_debug("%s\n", argv[i]);
    }

    // set default port and host
    config->port = TCP_CLIENT_DEFAULT_PORT;
    config->host = TCP_CLIENT_DEFAULT_HOST;

    static struct option long_options[] = {
        {"help", no_argument, 0, 0},
        {"verbose", no_argument, 0, 'v'},
        {"host", required_argument, 0, 'h'},
        {"port", required_argument, 0, 'p'},
        {0, 0, 0, 0}
    };
    
    int getopt_return_value;

    while (1) {
        int option_index = 0;
        getopt_return_value = getopt_long(argc, argv, SHORT_OPTIONS, long_options, &option_index);

        // exit loop if all arguments are parsed
        if (getopt_return_value == ALL_OPTIONS_PARSED) break;

        // loop through optional arguments
        switch (getopt_return_value) {
        case 0:
            printf(HELP_MESSAGE);
            exit(EXIT_SUCCESS);

        case 'v':
            log_info("Verbose is ON\n");
            log_set_level(LOG_TRACE);
            break;

        case 'h':
            config->host = optarg;
            log_info("Host is set to '%s'\n", optarg);
            break;

        case 'p':
            // loop through input port number
            for (size_t i = 0; i < strlen(optarg); i++) {
                // check if input is digit
                if (!isdigit(optarg[i])) {
                    log_error("'%s' is not a valid port\n", optarg);
                    printf(HELP_MESSAGE);
                    exit(EXIT_FAILURE);
                }
            }
            config->port = optarg;
            log_info("Port is set to '%s'\n", optarg);
            break;

        case '?':
            printf(HELP_MESSAGE);
            exit(EXIT_FAILURE);
            break;

        default:
            printf("?? getopt returned character code 0%o ??\n", getopt_return_value);
        }
    }
    
    // check if there is not enough arguments
    if ((argc - optind) < REQUIRED_NUMBER_OF_ARGUMENTS_OFFSET) {
        log_error("Missing argument(s)!\n");
        printf(HELP_MESSAGE);
        exit(EXIT_FAILURE);
    }

    // check if there are too many arguments
    if ((argc - optind) > REQUIRED_NUMBER_OF_ARGUMENTS_OFFSET) {
        log_error("Too many arguments!\n");
        printf(HELP_MESSAGE);
        exit(EXIT_FAILURE);
    }

    // set file
    config->file = argv[argc - 1];
    
    // for debug
    if (optind < argc) {
        log_debug("non-option ARGV-elements: ");
        while (optind < argc)
            log_debug("%s ", argv[optind++]);
        log_debug("\n");
    }

    return EXIT_SUCCESS;
}

/*
Description:
    Creates a TCP socket and connects it to the specified host and port.
Arguments:
    Config config: A config struct with the necessary information.
Return value:
    Returns the socket file descriptor or -1 if an error occurs.
*/
int tcp_client_connect(Config config) {
    
    int sockfd; 
    struct addrinfo hints, *servinfo, *p;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    // see if host and port are valid
    if ((rv = getaddrinfo(config.host, config.port, &hints, &servinfo)) != 0) {
        log_error("%s\n", gai_strerror(rv));
        exit(EXIT_FAILURE);
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        // error has occurred
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("");
            continue;
        }

        // error has occurred
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("");
            continue;
        }
        break;
    }

    // no connection made
    if (p == NULL) {
        log_error("Failed to connect\n");
        return -1;
    }

    return sockfd;
}

/*
Description:
    Creates and sends request to server using the socket and configuration.
Arguments:
    int sockfd: Socket file descriptor
    char *action: The action that will be sent
    char *message: The message that will be sent
Return value:
    Returns a 1 on failure, 0 on success
*/
int tcp_client_send_request(int sockfd, char *action, char *message) {
    
    int message_length = strlen(message);
    int bytes_sent = 0;
    int total_bytes_sent = 0;

    int payload_length = strlen(action) + strlen(message) + PAYLOAD_MEMORY_BUFFER;
    
    char *request = (char *)malloc(payload_length);
    sprintf(request, "%s %d %s", action, message_length, message);
    int request_length = strlen(request);
    
    // send until all message is sent
    while (total_bytes_sent < request_length) {
        bytes_sent = send(sockfd, request, request_length - total_bytes_sent, 0);
        // check if an error has occurred
        if (bytes_sent == -1) {
            log_error("Send failed!\n");
            exit(EXIT_FAILURE);
        }
        total_bytes_sent += bytes_sent;
    }
    free(request);
    return EXIT_SUCCESS;
}

/*
Description:
    Receives the response from the server. The caller must provide a function pointer that handles
the response and returns a true value if all responses have been handled, otherwise it returns a
    false value. After the response is handled by the handle_response function pointer, the response
    data can be safely deleted. The string passed to the function pointer must be null terminated.
Arguments:
    int sockfd: Socket file descriptor
    int (*handle_response)(char *): A callback function that handles a response
Return value:
    Returns a 1 on failure, 0 on success
*/
int tcp_client_receive_response(int sockfd, int (*handle_response)(char *)) {


    int all_done = 0;
    int total_bytes_received = 0;
    int bytes_received = 0;
    char *buffer = malloc(DEFAULT_BUFFER_SIZE);
    char char_message_length[10];
    int buffer_size = DEFAULT_BUFFER_SIZE;

    // receive until all responses are received
    while (1) {

        if (all_done) {
            free(buffer);
            return EXIT_SUCCESS;
        }

        //log_debug("Total Bytes Received: %ld", total_bytes_received);
        
        bytes_received = recv(sockfd, buffer + total_bytes_received, buffer_size - total_bytes_received, 0);
        // check if an error has occurred
        if (bytes_received == -1) {
            log_error("Receive failed!\n");
            exit(EXIT_FAILURE);
        }
        // exit loop if connection is closed
        if (bytes_received == 0) {
            log_info("Connection closed.\n");
            break;
        }
        total_bytes_received += bytes_received;
        
        while (1) {
            // look for the first space char ' ' to find message length
            char *first_space_addr = strchr(buffer,' ');
            if (first_space_addr == NULL || !isdigit(buffer[0])) {
                total_bytes_received = 0;
                break;
            }
            strncpy(char_message_length, buffer, first_space_addr - buffer);
            char_message_length[first_space_addr - buffer] = '\0';

            int message_length = atoi(char_message_length);
            
            // check if buffer is not big enough
            if (message_length > (buffer_size - (first_space_addr - buffer) - 1)) {
                buffer_size = buffer_size * 2;
                buffer = realloc(buffer, buffer_size);
                break;
            }

            if (total_bytes_received < message_length) {
                break;
            }

            // buffer holds complete message
            char *message = malloc(message_length + 1);
            strncpy(message, first_space_addr + 1, message_length);
            message[message_length] = '\0';
            if(handle_response(message)) {
                all_done = 1;
                break;
            }
            int bytes_read = strlen(char_message_length) + message_length + 1;
            memmove(buffer, buffer + bytes_read, buffer_size - bytes_read);
            total_bytes_received -= bytes_read;
            free(message);
        }
    }
    return EXIT_SUCCESS;
}


/*
Description:
    Closes the given socket.
Arguments:
    int sockfd: Socket file descriptor
Return value:
    Returns a 1 on failure, 0 on success
*/
int tcp_client_close(int sockfd) {
    return close(sockfd);
}

/*
Description:
    Opens a file.
Arguments:
    char *file_name: The name of the file to open
Return value:
    Returns NULL on failure, a FILE pointer on success
*/
FILE *tcp_client_open_file(char *file_name) {

    if (strcmp(file_name, "-") == 0) {
        return stdin;
    }

    FILE *file = fopen(file_name, "r");
    // failed to open file
    if (file == NULL) {
        log_error("Failed to open file.\n");
        exit(EXIT_FAILURE);
    }

    // check if file is empty
    FILE *temp_file = fopen(file_name, "r");
    fseek(temp_file, 0, SEEK_END);
    if (ftell(temp_file) == 0) {
        fclose(temp_file);
        log_error("File is empty");
        exit(EXIT_FAILURE);
    }
    fclose(temp_file);

    return file;
}

/*
Description:
    Checks if the action is valid.
Arguments:
    char *action: The char pointer to action
Return value:
    Returns a 1 on failure, 0 on success
*/
int is_valid_action(char *action) {
    // Will be nice to convert input action to lowercase
    char *actions[NUMBER_OF_ACTIONS] = {"uppercase", "lowercase", "reverse", "shuffle", "random"};
    // loop through 5 available actions
    for (int i = 0; i < NUMBER_OF_ACTIONS; i++) {
        // check if input action is a match to one of the available actions
        if (!strcmp(action, actions[i])) return true;
    }
    // at end of the available action and still no match
    return false;
}

/*
Description:
    Gets the next line of a file, filling in action and message. This function should be similar
    design to getline() (https://linux.die.net/man/3/getline). *action and message must be allocated
    by the function and freed by the caller.* When this function is called, action must point to the
    action string and the message must point to the message string.
Arguments:
    FILE *fd: The file pointer to read from
    char **action: A pointer to the action that was read in
    char **message: A pointer to the message that was read in
Return value:
    Returns -1 on failure, the number of characters read on success
*/
int tcp_client_get_line(FILE *fd, char **action, char **message) {
    
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
   
    // keep reading the next line unless there's an error
    while ((read = getline(&line, &len, fd)) != -1) {
        // skip empty line
        if (!strcmp(line, "\n") || line[0] == ' ') continue;

        // look for the first space char ' ' to split action and message
        char *first_space_addr = strchr(line,' ');

        // proceed if addr is not NULL
        if (first_space_addr != NULL) {
            *action = malloc(first_space_addr - line + 1);
            *message = malloc(strlen(first_space_addr + 1));
            sscanf(line, "%s %[^\n]", *action, *message);

            // skip to next line if action is bad
            if (!is_valid_action(*action)) continue;
            break;
        }
    }
    printf("here\n");
    free(line);
    return read;
}

/*
Description:
    Closes a file.
Arguments:
    FILE *fd: The file pointer to close
Return value:
    Returns a 1 on failure, 0 on success
*/
int tcp_client_close_file(FILE *fd) {
    // bad file pointer
    if (fd == NULL) {
        log_error("Failed to close file: Bad pointer\n");
        return EXIT_FAILURE; 
    }
    // failed to close file
    if (fclose(fd) != EXIT_SUCCESS) {
        log_error("Failed to close file\n");
        return EXIT_FAILURE; 
    }
    return EXIT_SUCCESS;
}