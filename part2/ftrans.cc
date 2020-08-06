#include <arpa/inet.h>		
#include <stdio.h>		
#include <stdlib.h>		
#include <sys/socket.h>		
#include <unistd.h>		
#include <string.h>		
#include <netdb.h>		
#include <netinet/in.h>		

static const size_t MAX_MESSAGE_SIZE = 256;

int run_server(int port, int queue_size) {

	// (1) Create socket
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		perror("Error opening stream socket");
		return -1;
	}

	// (2) Set the "reuse port" socket option
	int yesval = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yesval, sizeof(yesval)) == -1) {
		perror("Error setting socket options");
		return -1;
	}

	// (3) Create a sockaddr_in struct for the proper port and bind() to it.
	struct sockaddr_in addr;

	// 3(a) Make sockaddr_in
	addr.sin_family = AF_INET;
	// Let the OS map it to the correct address.
	addr.sin_addr.s_addr = INADDR_ANY;
	// If port is 0, the OS will choose the port for us. Here we specify a port.
	// Use htons to convert from local byte order to network byte order.
	addr.sin_port = htons(port);
	// Bind to the port.
	if (bind(sockfd, (sockaddr *) &addr, sizeof(addr)) == -1) {
		perror("Error binding stream socket");
		return -1;
	}

	// (3b)Detect which port was chosen.
	// port = get_port_number(sockfd);
	printf("Server listening on port %d...\n", port);

	// (4) Begin listening for incoming connections.
	listen(sockfd, queue_size);

	// (5) Serve incoming connections one by one forever.
	while (true) {
		int connectionfd = accept(sockfd, 0, 0);
		if (connectionfd == -1) {
			perror("Error accepting connection");
			return -1;
		}

		printf("New connection %d\n", connectionfd);

		// (5.1) Receive filename from client.
		char filename[MAX_MESSAGE_SIZE];
		memset(filename, 0, sizeof(filename));
		// Call recv() enough times to consume all the data the server sends.
		size_t recvd = 0;
		ssize_t rval;
		do {
			rval = recv(connectionfd, filename + recvd, MAX_MESSAGE_SIZE - recvd, 0);
			if (rval == -1) {
				perror("Error reading streaming file data");
				exit(1);
			}
			recvd += rval;
		} while (strcmp(filename + recvd, "\0") != 0); 
		printf("Filename received '%s'\n", filename);
		
		// (5.2) Open file
		FILE *file = fopen(filename, "rb");
		if (file == NULL){
			printf("Open file failed!\n");
			return -1;
		}
		
		fseek(file, 0, SEEK_END);
		unsigned int length = ftell(file);
		printf("File size: %d\n", length);
		fseek(file, 0, SEEK_SET);
		// rewind(file);
		char* fileData = (char*) malloc(length);
		fread(fileData, length, 1, file);
		fclose(file);
		
		// (5.3) Send length to client
		if (send(connectionfd, (char *)&length, sizeof(length), 0) == -1) {
			perror("Error sending file length");
			return -1;
		}
		
		// (5.4) Send data to client
		if (send(connectionfd, fileData, length, 0) == -1) {
			perror("Error sending file data");
			return -1;
		}
		
		// (5.5) Close connection
		free(fileData);
		close(connectionfd);
	}
}

int run_client(const char *hostname, int port, const char *filename, int client_port){
	// (1) Create a socket
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);

	// (2) Create a sockaddr_in to specify remote host and port
	struct sockaddr_in server_addr; // specify server's address
	server_addr.sin_family = AF_INET;

	// Step (2): specify socket address (hostname).
	// The socket will be a client, so call this unix helper function to convert a hostname string to a useable `hostent` struct.
	// hostname here may be 10.0.0.1
	struct hostent *host = gethostbyname(hostname);
	if (host == nullptr) {
		fprintf(stderr, "%s: unknown host\n", hostname);
		return -1;
	}
	memcpy(&(server_addr.sin_addr), host->h_addr, host->h_length);

	// bind client's socket to a port number...
	struct sockaddr_in my_addr;
	memset(&my_addr, 0, sizeof(my_addr));
	my_addr.sin_family = AF_INET;
	my_addr.sin_addr.s_addr = INADDR_ANY; //INADDRY_ANY == 0.0.0.0
	my_addr.sin_port = htons(client_port);
	bind(sockfd, (struct sockaddr *) & my_addr, sizeof(my_addr));

	// Step (3): Set the port value.
	// Use htons to convert from local byte order to network byte order.
	server_addr.sin_port = htons(port);

	// (3) Connect to remote server
	if (connect(sockfd, (sockaddr *) &server_addr, sizeof(server_addr)) == -1) {
		perror("Error connecting stream socket");
		return -1;
	}

	// (4) Send filename to remote server, `strlen(filename) + 1` here to send the null terminator at the end
	if (send(sockfd, filename, strlen(filename) + 1, 0) == -1) {
		perror("Error sending filename");
		return -1;
	}
	
	// (5) Reads file length from server
	unsigned int length;
	if (recv(sockfd, (char *)&length, sizeof(length), 0) == -1) {
		perror("Error reading file length");
		return -1;
	}
	printf("File length received (client side): %d\n", length);

	// (6) Reads file data from server and writes to file with filename
	char* fileData = (char*) malloc(length+1);
	
	// Call recv() enough times to consume all the data the server sends.
	size_t recvd = 0;
	ssize_t rval;
	do {
		rval = recv(sockfd, fileData + recvd, length - recvd, 0);
		if (rval == -1) {
			perror("Error reading streaming file data");
			exit(1);
		}
		recvd += rval;
	} while (recvd < length);
	fileData[length] = 0;        // null terminator at the end
	// printf("File data received (client side): %s\n", fileData);
	
	FILE *file = fopen(filename, "wb");
	if (file == NULL){
		printf("Open file failed!\n");
		return -1;
	}
	
    fwrite(fileData, length, 1, file);	
	fclose(file);
	free(fileData);

	// (7) Close connection
	close(sockfd);

	return 0;
}

int main(int argc, const char **argv) {
	// Parse command line arguments, assuming all the arguments are exactly correct
	if (strcmp(argv[1], "-s") == 0){
		// server mode
		int port = atoi(argv[3]);
		printf("Running in server mode\n");
		if (run_server(port, 10) == -1) {
			return 1;
		}
	}
	
	if (strcmp(argv[1], "-c") == 0){
		// client mode
		const char *hostname = argv[3];
		int port = atoi(argv[5]);
		const char *filename = argv[7];
		int clientPort = atoi(argv[9]);
		printf("Running in client mode\n");
		if (run_client(hostname, port, filename, clientPort) == -1) {
			return 1;
		}
	}
	
	return 0;
}