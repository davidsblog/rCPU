#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h> // needed to run server on a new thread
#include <termios.h> // needed for unbuffered_getch()

#include "dwebsvr.h"

#define FILE_CHUNK_SIZE 1024
#define BIGGEST_FILE 104857600 // 100 Mb

struct {
	char *ext;
	char *filetype;
} extensions [] = {
	{"gif", "image/gif" },  
	{"jpg", "image/jpg" }, 
	{"jpeg","image/jpeg"},
	{"png", "image/png" },  
	{"ico", "image/ico" },  
	{"zip", "image/zip" },  
	{"gz",  "image/gz"  },  
	{"tar", "image/tar" },  
	{"htm", "text/html" },  
	{"html","text/html" },  
	{"js","text/javascript" },
    {"txt","text/plain" },
    {"css","text/css" },
    {"map","application/json" },
    {"woff","application/font-woff" },
    {"woff2","application/font-woff2" },
    {"ttf","application/font-sfnt" },
    {"svg","image/svg+xml" },
    {"eot","application/vnd.ms-fontobject" },
    {"mp4","video/mp4" },
	{0,0} };

void send_response(struct hitArgs *args, char*, char*, http_verb);
void log_filter(log_type, char*, char*, int);
void send_api_response(struct hitArgs *args, char*, char*);
void send_file_response(struct hitArgs *args, char*, char*, int);

struct termios original_settings;
pthread_t server_thread_id;

void* server_thread(void *args)
{
    pthread_detach(pthread_self());
    char *arg = (char*)args;
    dwebserver(atoi(arg), &send_response, &log_filter);
    return NULL;
}

void close_down()
{
    tcsetattr(STDIN_FILENO, TCSANOW, &original_settings);
    dwebserver_kill();
    pthread_cancel(server_thread_id);
    puts("Bye bye");
}

void wait_for_key()
{
    struct termios unbuffered;
    tcgetattr(STDIN_FILENO, &original_settings);
    
    unbuffered = original_settings;
    unbuffered.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSANOW, &unbuffered);
    
    getchar();
    close_down();
}

int main(int argc, char **argv)
{
    if (argc < 2 || !strncmp(argv[1], "-h", 2))
	{
		printf("hint: dweb [port number]\n");
		return 0;
	}
    if (argc > 2 && !strncmp(argv[2], "-d", 2))
    {
        // don't read from the console or log anything
        dwebserver(atoi(argv[1]), &send_response, NULL);
    }
    else
    {
        if (pthread_create(&server_thread_id, NULL, server_thread, argv[1]) !=0)
        {
            puts("Error: pthread_create could not create server thread");
            return 0;
        }
    
        puts("dweb server started\nPress a key to quit");
        wait_for_key();
    }
}

void log_filter(log_type type, char *s1, char *s2, int socket_fd)
{
    if (type!=ERROR) return;
    printf("ERROR: %s: %s (errno=%d pid=%d socket=%d)\n",s1, s2, errno, getpid(), socket_fd);
}

// decide if we need to send an API response or a file...
void send_response(struct hitArgs *args, char *path, char *request_body, http_verb type)
{
    int path_length=(int)strlen(path);
    if (!strncmp(&path[path_length-3], "api", 3))
	{
		return send_api_response(args, path, request_body);
	}
    if (path_length==0)
	{
        return send_file_response(args, "index.html", request_body, 10);
	}
    send_file_response(args, path, request_body, path_length);
}

// a simple API, it receives a number, increments it and returns the response
void send_api_response(struct hitArgs *args, char *path, char *request_body)
{
	char response[4];
	
	if (args->form_value_counter==1 && !strncmp(form_name(args, 0), "counter", strlen(form_name(args, 0))))
	{
		int c = atoi(form_value(args, 0));
		if (c>998) c=0;
		sprintf(response, "%d", ++c);
		return ok_200(args, "\nContent-Type: text/plain", response, path);
	}
	else
	{
		return forbidden_403(args, "Bad request");
	}
}

void send_file_response(struct hitArgs *args, char *path, char *request_body, int path_length)
{
	int file_id, i;
	long len;
	char *content_type = NULL;
    STRING *response = new_string(FILE_CHUNK_SIZE);
	
	if (args->form_value_counter > 0 &&
        string_matches_value(args->content_type, "application/x-www-form-urlencoded"))
	{
        string_add(response, "<html><head><title>Response Page</title></head>");
        string_add(response, "<body><h1>Thanks...</h1>You sent these values<br/><br/>");
        
        int v;
        for (v=0; v<args->form_value_counter; v++)
        {
            string_add(response, form_name(args, v));
            string_add(response, ": <b>");
            string_add(response, form_value(args, v));
            string_add(response, "</b><br/>");
        }
        
        string_add(response, "</body></html>");
		ok_200(args, "\nContent-Type: text/html", string_chars(response), path);
        string_free(response);
        return;
	}
	
	// work out the file type and check we support it
	for (i=0; extensions[i].ext != 0; i++)
	{
		len = strlen(extensions[i].ext);
		if( !strncmp(&path[path_length-len], extensions[i].ext, len))
		{
			content_type = extensions[i].filetype;
			break;
		}
	}
	if (content_type == NULL)
	{
		string_free(response);
        return forbidden_403(args, "file extension type not supported");
	}
	
	if (file_id = open(path, O_RDONLY), file_id == -1)
	{
		string_free(response);
        return notfound_404(args, "failed to open file");
	}
	
	// open the file for reading
	len = (long)lseek(file_id, (off_t)0, SEEK_END); // lseek to the file end to find the length
	lseek(file_id, (off_t)0, SEEK_SET); // lseek back to the file start
    
    if (len > BIGGEST_FILE)
    {
        string_free(response);
        return forbidden_403(args, "files this large are not supported");
    }
    
    string_add(response, "HTTP/1.1 200 OK\nServer: dweb\n");
    string_add(response, "Connection: close\n");
    string_add(response, "Content-Type: ");
    string_add(response, content_type);
    write_header(args->socketfd, string_chars(response), len);
    
	// send file in blocks
	while ((len = read(file_id, response->ptr, FILE_CHUNK_SIZE)) > 0)
	{
		if (write(args->socketfd, response->ptr, len) <=0) break;
	}
    string_free(response);
    close(file_id);
    
    // allow socket to drain before closing
	sleep(1);
}
