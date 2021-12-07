#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <time.h>
#include <sys/file.h>
#include <fcntl.h> 
#include <time.h>
#include <ctype.h>
#include <sys/stat.h>
#define DEFAULT_MIME_TYPE "application/octet-stream"
#define SERVER_ROOT "./root"

struct file_data {
    int size;
    void *data;
};

struct file_data *file_load(char *filename)
{
    char *buffer, *p;
    struct stat buf;
    int bytes_read, bytes_remaining, total_bytes = 0;

    // Get the file size
    if (stat(filename, &buf) == -1) {
        return NULL;
    }

    // Make sure it's a regular file
    if (!(buf.st_mode & S_IFREG)) {
        return NULL;
    }

    // Open the file for reading
    FILE *fp = fopen(filename, "rb");

    if (fp == NULL) {
        return NULL;
    }

    bytes_remaining = buf.st_size;
    p = buffer = malloc(bytes_remaining);

    if (buffer == NULL) {
        return NULL;
    }

    while (bytes_read = fread(p, 1, bytes_remaining, fp), bytes_read != 0 && bytes_remaining > 0) {
        if (bytes_read == -1) {
            free(buffer);
            return NULL;
        }

        bytes_remaining -= bytes_read;
        p += bytes_read;
        total_bytes += bytes_read;
    }

    struct file_data *filedata = malloc(sizeof *filedata);

    if (filedata == NULL) {
        free(buffer);
        return NULL;
    }

    filedata->data = buffer;
    filedata->size = total_bytes;

    return filedata;
}

void file_free(struct file_data *filedata)
{
    free(filedata->data);
    free(filedata);
}

void send_response(int fd, char *header, char *content_type, void *body, int content_length)
{
    const int max_response_size = 65536;
    char response[max_response_size];
    time_t rawtime;
    struct tm *info;
  
    time(&rawtime);

   
   int r = sprintf(response,
      "%s\r\n"
      "Connection: close\r\n"
      "Content-Length: %d\r\n"
      "Content-Type: %s\r\n"
      "Date: %s\r\n",
      header, content_length, content_type, asctime(localtime(&rawtime)));

    memcpy(response+r, body, content_length);
    write(fd,response,content_length+r);
  
}


void post_save(int fd, char *body)
{

  char response_body[1024];
  char *status;


  int fp = open("post.text", O_CREAT|O_WRONLY, 0644);

  if (fp) {
    flock(fp, LOCK_EX); 
    write(fp, body, strlen(body)); 

    flock(fp, LOCK_UN); 
    
    close(fp);

    status = "ok";
  } else {
    status = "error";
  }


  send_response(fd, "HTTP/1.1 201 Created", "application/json", response_body,sizeof(response_body));
}

char *strlower(char *s)
{
    for (char *p = s; *p != '\0'; p++) {
        *p = tolower(*p);
    }

    return s;
}

char *mime_type_get(char *filename)
{
    char *ext = strrchr(filename, '.');

    if (ext == NULL) {
        return DEFAULT_MIME_TYPE;
    }
    
    ext++;

    strlower(ext);

    if (strcmp(ext, "html") == 0 || strcmp(ext, "htm") == 0) { return "text/html"; }
    if (strcmp(ext, "jpeg") == 0 || strcmp(ext, "jpg") == 0) { return "image/jpg"; }
    if (strcmp(ext, "css") == 0) { return "text/css"; }
    if (strcmp(ext, "js") == 0) { return "application/javascript"; }
    if (strcmp(ext, "json") == 0) { return "application/json"; }
    if (strcmp(ext, "txt") == 0) { return "text/plain"; }
    if (strcmp(ext, "gif") == 0) { return "image/gif"; }
    if (strcmp(ext, "png") == 0) { return "image/png"; }

    return DEFAULT_MIME_TYPE;
}

char *find_end_of_header(char *header)
{
 
  char *nl;

  if (strstr(header, "\n\n")) {
    nl = strstr(header, "\n\n");
  } else if (strstr(header, "\r\r")) {
    nl = strstr(header, "\r\r");
  } else {
    nl = strstr(header, "\r\n\r\n");
  }
  return nl;
}

void get_file(int fd, char *request_path)
{    
    char filepath[4096];
    struct file_data *filedata;
    char *mime_type;

    snprintf(filepath, sizeof(filepath), "%s%s", SERVER_ROOT, request_path);
    filedata = file_load(filepath);
    mime_type = mime_type_get(filepath);
    if(filedata==NULL){
        filedata = file_load("./root/404.html");
        send_response(fd, "HTTP/1.1 200 OK", mime_type, filedata->data,filedata->size);
    }
     else{
        send_response(fd, "HTTP/1.1 200 OK", mime_type, filedata->data,filedata->size);

     }
  
    file_free(filedata);
}

void handle_http_request(int fd)
{
    const int request_buffer_size = 65536; 
    char request[request_buffer_size];
     char *p;

    int bytes_recvd = recv(fd, request, request_buffer_size - 1, 0);

    if (bytes_recvd < 0) {
        perror("recv");
        return;
    }
    request[bytes_recvd] = '\0';

    char method[10], path[100], protocol[20];
    sscanf(request, "%s %s %s", method, path, protocol);
    p = find_end_of_header(request); 

    if(strcmp(method, "GET") == 0) {
          get_file(fd,path);
    }
    else{
        post_save(fd,p);
    }
 

}

    
int main() {    
   int create_socket, new_socket;    
   socklen_t addrlen;    
   int bufsize = 1024;    
   char *buffer = malloc(bufsize);    
   struct sockaddr_in address;    
 
   if ((create_socket = socket(AF_INET, SOCK_STREAM, 0)) > 0){    
      printf("The socket was created\n");
   }
    
   address.sin_family = AF_INET;    
   address.sin_addr.s_addr = INADDR_ANY;    
   address.sin_port = htons(80);    
    

   if (bind(create_socket, (struct sockaddr *) &address, sizeof(address)) == 0){    
      printf("Binding Socket\n");
   }
    if (listen(create_socket, 50) < 0) {    
         perror("server: listen");      
    }    
    
   while (1) {  
     
      if ((new_socket = accept(create_socket, (struct sockaddr *) &address, &addrlen)) < 0) {    
         perror("server: accept");    
      }    
    
      if (new_socket > 0){    
         printf("The Client is connected...\n");
      }
            
      handle_http_request(new_socket);
      close(new_socket); 
    
    
   }   
     
   
   
   return 0;    
}
