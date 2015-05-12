/****************************************************************************
** Released under The MIT License (MIT). This code comes without warranty, **
** but if you use it you must provide attribution back to David's Blog     **
** at http://www.codehosting.net   See the LICENSE file for more details.  **
****************************************************************************/

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h> // for struct timeval

// set the correct mode here, options are:
// SINGLE_THREADED, MULTI_PROCESS, OR MULTI_THREADED
#define MODE MULTI_THREADED

#if MODE == MULTI_THREADED
    #include <pthread.h>
#endif

#include "dwebsvr.h"

// this is the maximum amount of bytes that can be read from a request
// it includes the headers
#define MAX_INCOMING_REQUEST 4096

// get_form_values() will allocate memory in blocks of this size
#define FORM_VALUE_BLOCK 10

// a global place to store the listening socket descriptor
int listenfd;
volatile sig_atomic_t doing_shutdown = 0;

// assumes a content type of "application/x-www-form-urlencoded" (the default type)
void get_form_values(struct hitArgs *args, char *body)
{
    char *saveptr;
    int t=0, i, alloc = FORM_VALUE_BLOCK;
	char *tmp, *token = strtok_r(body, "&", &saveptr);
    
    args->form_values = mallocx(alloc * sizeof(FORM_VALUE));
    memset(args->form_values, 0, alloc * sizeof(FORM_VALUE));
    
    while(token != NULL)
    {
        tmp = mallocx((int)strlen(token)+1);
        strcpy(tmp, token);
        url_decode(tmp);
        
		for (i=0; i<strlen(tmp); i++)
        {
			if (tmp[i]=='=') break;
        }
        
        if (alloc<=t)
        {
            int newsize = alloc+FORM_VALUE_BLOCK;
            args->form_values = reallocx(args->form_values, newsize * sizeof(FORM_VALUE));
            memset(args->form_values+alloc, 0, FORM_VALUE_BLOCK * sizeof(FORM_VALUE));
            alloc = newsize;
        }
        
        args->form_values[t].data = mallocx((int)strlen(tmp)+1);
        strcpy(args->form_values[t].data, tmp);
        args->form_values[t].name = args->form_values[t].data;
        args->form_values[t].value = args->form_values[t].data+1+i;
		args->form_values[t++].data[i] = 0;
		token = strtok_r(NULL, "&", &saveptr);
        free (tmp);
    }
    args->form_value_counter = t;
}

void clear_form_values(struct hitArgs *args)
{
    if (args->form_values == NULL) return;
    for (args->form_value_counter--; args->form_value_counter>=0; args->form_value_counter--)
    {
        free(args->form_values[args->form_value_counter].data);
    }
    free(args->form_values);
}

void finish_hit(struct hitArgs *args, int exit_code)
{
    close(args->socketfd);
    if (args->buffer)
    {
        string_free(args->buffer);
    }
    if (args->headers)
    {
        free(args->headers);
    }
    clear_form_values(args);
    if (args->content_type)
    {
        free(args->content_type);
    }
    free(args);
    
#if MODE == MULTI_PROCESS
    exit(exit_code);
#elif MODE == MULTI_THREADED
    pthread_exit(NULL);
#endif
}

// writes the given headers and sets the Content-Length
void write_header(int socket_fd, char *head, long content_len)
{
    STRING *header = new_string(255);
    string_add(header, head);
    string_add(header, "\nContent-Length: ");
    char cl[10]; // 100Mb = 104,857,600 bytes
    snprintf(cl, 10, "%ld", content_len);
    string_add(header, cl);
    string_add(header, "\r\n\r\n");
#ifndef SO_NOSIGPIPE
    send(socket_fd, string_chars(header), header->used_bytes-1, MSG_NOSIGNAL);
#else
    write(socket_fd, string_chars(header), header->used_bytes-1);
#endif
    string_free(header);
}

void write_html(int socket_fd, char *head, char *html)
{
    write_header(socket_fd, head, strlen(html));
#ifndef SO_NOSIGPIPE
    send(socket_fd, html, strlen(html), MSG_NOSIGNAL);
#else
    write(socket_fd, html, strlen(html));
#endif
}

void forbidden_403(struct hitArgs *args, char *info)
{
	write_html(args->socketfd,
        "HTTP/1.1 403 Forbidden\nServer: dweb\nConnection: close\nContent-Type: text/html",
		"<html><head>\n<title>403 Forbidden</title>\n"
		"</head><body>\n<h1>Forbidden</h1>\nThe requested URL, file type or operation is not allowed on this simple webserver.\n</body>"
		"</html>");
	args->logger_function(LOG, "403 FORBIDDEN", info, args->socketfd);
}

void notfound_404(struct hitArgs *args, char *info)
{
	write_html(args->socketfd,
        "HTTP/1.1 404 Not Found\nServer: dweb\nConnection: close\nContent-Type: text/html",
		"<html><head>\n<title>404 Not Found</title>\n"
		"</head><body>\n<h1>Not Found</h1>\nThe requested URL was not found on this server.\n</body></html>");

	args->logger_function(LOG, "404 NOT FOUND", info, args->socketfd);
}

void ok_200(struct hitArgs *args, char *custom_headers, char *html, char *path)
{
	STRING *headers = new_string(255);
    string_add(headers, "HTTP/1.1 200 OK\nServer: dweb\nCache-Control: no-cache\nPragma: no-cache");
    if (custom_headers != NULL)
    {
        string_add(headers, custom_headers);
    }
    write_html(args->socketfd, string_chars(headers), html);
    string_free(headers);
	
	args->logger_function(LOG, "200 OK", path, args->socketfd);
}

void default_logger(log_type type, char *title, char *description, int socket_fd)
{
	switch (type)
	{
		case ERROR:
			printf("ERROR: %s: %s (errno=%d pid=%d socket=%d)\n",title, description, errno, getpid(), socket_fd);
			break;
		default:
			printf("INFO: %s: %s (pid=%d socket=%d)\n",title, description, getpid(), socket_fd);
			break;
	}
	fflush(stdout);
}

struct http_header get_header(const char *name, char *request)
{
    struct http_header retval;
    int x=0;
    char *ptr = strstr(request, name);
    strncpy(retval.name, name, sizeof(retval.name)-1);
    retval.name[sizeof(retval.name)-1] = 0;
    
    if (ptr == NULL)
    {
        retval.value[0] = 0;
        return retval;
    }
    
    while (*ptr++!=':') ;
    while (isblank(*++ptr)) ;
    while (x<sizeof(retval.value)-1 && *ptr!='\r' && *ptr!='\n')
    {
        retval.value[x++] = *ptr++;
    }
    retval.value[x]=0;
    return retval;
}

long get_body_start(char *request)
{
    // return the starting index of the request body
    // so ... just find the end of the HTTP headers
    char *ptr = strstr(request, "\r\n\r\n");
    return (ptr==NULL) ? -1 : (ptr+4) - request;
}

http_verb request_type(char *request)
{
	if (strncmp(request, "GET ", 4)==0 || strncmp(request, "get ", 4)==0)
	{
		return HTTP_GET;
	}
	if (strncmp(request, "POST ", 5)==0 || strncmp(request, "post ", 5)==0)
	{
		return HTTP_POST;
	}
	
	return HTTP_NOT_SUPPORTED;
}

// webhit() will read data from the socket in chunks of this size
#define READ_BUF_LEN 255

void webhit(struct hitArgs *args)
{
	int j;
	http_verb type;
	long i, body_size = 0, request_size = 0, body_start, headers_end;
    char tmp_buf[READ_BUF_LEN+1];
	char *body;
    struct http_header content_length;
    args->buffer = new_string(READ_BUF_LEN);
    
    // we need to read the HTTP headers first...
    // so loop until we receive "\r\n\r\n"
    while (get_body_start(string_chars(args->buffer)) < 0
           && args->buffer->used_bytes <= MAX_INCOMING_REQUEST)
    {
        memset(tmp_buf, 0, READ_BUF_LEN+1);
        request_size += read(args->socketfd, tmp_buf, READ_BUF_LEN);
        string_add(args->buffer, tmp_buf);
        if (tmp_buf[0]==0) break;
    }
    
    if (request_size == 0)
    {
        finish_hit(args, 3);
        return;
    }
    
    content_length = get_header("Content-Length", string_chars(args->buffer));
    args->content_length = atoi(content_length.value);
    body_start = get_body_start(string_chars(args->buffer));
    headers_end = body_start-4;
    
    if (headers_end > 0)
    {
        args->headers = mallocx((int)headers_end+1);
        strncpy(args->headers, string_chars(args->buffer), headers_end);
        args->headers[headers_end]=0;
    }
    else
    {
        args->headers = mallocx(1);
        args->headers[0] = 0;
    }
    
    if (body_start >= 0)
    {
        body_size = request_size - body_start;
    }
    
    // safari seems to send the headers, and then the body slightly later
    while (body_size < args->content_length
           && args->buffer->used_bytes <= MAX_INCOMING_REQUEST)
    {
        memset(tmp_buf, 0, READ_BUF_LEN+1);
        i = read(args->socketfd, tmp_buf, READ_BUF_LEN);
        if (i>0)
        {
            request_size += i;
            string_add(args->buffer, tmp_buf);
            body_size = request_size - body_start;
        }
        else
        {
            // stop looping if we cannot read any more bytes
            break;
        }
    }
    
    if (request_size <= 0)
	{
		// cannot read request, so we'll stop
		forbidden_403(args, "failed to read http request");
        finish_hit(args, 3);
        return;
	}
    
    args->logger_function(LOG, "request", string_chars(args->buffer), args->hit);
    
	if (type = request_type(string_chars(args->buffer)), type == HTTP_NOT_SUPPORTED)
	{
		forbidden_403(args, "Only simple GET and POST operations are supported");
        finish_hit(args, 3);
        return;
	}
	
	// get a pointer to the request body (or NULL if it's not there)
    body = (type==HTTP_GET) ? NULL : args->buffer->ptr+get_body_start(string_chars(args->buffer));
	
	// the request will be "GET [URL] " or "POST [URL] " followed by other details
	// we will terminate after the second space, to ignore everything else
	for (i = (type==HTTP_GET) ? 4 : 5; i < args->buffer->used_bytes; i++)
	{
		if (string_chars(args->buffer)[i] == ' ')
		{
			string_chars(args->buffer)[i] = 0; // second space, terminate string here
			break;
		}
	}

    j = (type==HTTP_GET) ? 4 : 5;
    
    // check for an absolute directory
    if (string_chars(args->buffer)[j+1] == '/')
    {
        forbidden_403(args, "Sorry, absolute paths are not permitted");
        finish_hit(args, 3);
        return;
    }
    
	for (; j<i-1; j++)
	{
		// check for any parent directory use
		if (string_chars(args->buffer)[j] == '.' && string_chars(args->buffer)[j+1] == '.')
		{
			forbidden_403(args, "Sorry, parent paths (..) are not permitted");
            finish_hit(args, 3);
	        return;
		}
	}
    
    struct http_header ctype = get_header("Content-Type", args->headers);
    j = (int)strlen(ctype.value);
    if (j > 0)
    {
        args->content_type = mallocx(j+1);
        strncpy(args->content_type, ctype.value, j);
        
        if (string_matches_value(args->content_type, "application/x-www-form-urlencoded"))
        {
            get_form_values(args, body);
        }
    }
    else
    {
        args->content_type = mallocx(1);
        args->content_type[0] = 0;
    }
    
	// call the "responder function" which has been provided to do the rest
    args->responder_function(args, string_chars(args->buffer) + ((type==HTTP_GET) ? 5 : 6), body, type);
    finish_hit(args, 1);
}

#if MODE == MULTI_THREADED
void* threadMain(void *targs)
{
	struct hitArgs *args = (struct hitArgs*)targs;
    pthread_detach(pthread_self());
    
    webhit(args);
    return NULL;
}
#endif

void inthandler(int sig)
{
    if (doing_shutdown==1) return;
    
    doing_shutdown=1;
    puts("\nwebserver shutting down");
    close(listenfd);
    if (sig != SIGUSR1)
    {
        exit(0);
    }
}

void dwebserver_kill(void)
{
    inthandler(SIGUSR1);
}

int dwebserver(int port,
    void (*responder_func)(struct hitArgs *args, char*, char*, http_verb), // pointer to responder func
    void (*logger_func)(log_type, char*, char*, int) ) // pointer to logger (or NULL)
{
#if MODE == MULTI_PROCESS
    int pid;
#endif
    int socketfd, hit;
	socklen_t length;
    // get the compiler to initialise to zeros (C99 Standard 6.7.8.21)
    struct sockaddr_in cli_addr = {}, serv_addr = {};
    
	if (port <= 0 || port > 60000)
	{
		logger_func(ERROR, "Invalid port number (try 1 - 60000)", "", 0);
		exit(3);
	}
    
    // ignore child process deaths
#ifndef SIGCLD
	signal(SIGCHLD, SIG_IGN);
#else
	signal(SIGCLD, SIG_IGN);
#endif
    signal(SIGHUP, SIG_IGN);  // ignore terminal hangups
    signal(SIGPIPE, SIG_IGN); // ignore broken pipes
    
    if ((listenfd = socket(AF_INET, SOCK_STREAM,0)) < 0)
	{
		logger_func(ERROR, "system call", "socket", 0);
		return 0;
	}
    
    // But to support Linux, I've also used MSG_NOSIGNAL:
    // http://stackoverflow.com/questions/108183/how-to-prevent-sigpipes-or-handle-them-properly/450130#450130
    
    int y = 1;
#ifdef SO_NOSIGPIPE
    // use SO_NOSIGPIPE, to ignore any SIGPIPEs
    if (setsockopt(listenfd, SOL_SOCKET, SO_NOSIGPIPE, &y, sizeof(y)) < 0)
    {
        logger_func(ERROR, "system call", "setsockopt -> SO_NOSIGPIPE", 0);
		return 0;
    }
    y=1;
#endif

    // use SO_REUSEADDR, so we can restart the server without waiting
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &y, sizeof(y)) < 0)
    {
        logger_func(ERROR, "system call", "setsockopt -> SO_REUSEADDR", 0);
		return 0;
    }
    
    // as soon as listenfd is set, keep a handler
    // so we can close it on exit
    signal(SIGINT, &inthandler);
    signal(SIGTERM, &inthandler);
    
    serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(port);
    
	if (bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) <0)
	{
		logger_func(ERROR, "system call", "bind", 0);
		return 0;
	}
    
	if (listen(listenfd, 64) <0)
	{
		logger_func(ERROR, "system call", "listen", 0);
		return 0;
	}
    
    // use a 60 second timeout on individual sockets
    struct timeval timeout;
    timeout.tv_sec = 60;
    timeout.tv_usec = 0;
    
	for (hit=1; ; hit++)
	{
		length = sizeof(cli_addr);
		if ((socketfd = accept(listenfd, (struct sockaddr*)&cli_addr, &length)) < 0)
		{
			if (doing_shutdown==0 && logger_func != NULL)
            {
                logger_func(ERROR, "system call", "accept", 0);
			}
            continue;
		}
        // apply the timeout to this socket
        if (setsockopt(socketfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(struct timeval)) < 0)
        {
            logger_func(ERROR, "system call", "setsockopt -> SO_RCVTIMEO", 0);
        }
        
        struct hitArgs *args = mallocx(sizeof(struct hitArgs));
        args->buffer = NULL;
        args->form_values = NULL;
        args->headers = NULL;
        args->content_type = NULL;
        args->form_value_counter = 0;
        args->logger_function = logger_func != NULL ? logger_func : &default_logger;
        args->hit = hit;
        args->socketfd = socketfd;
        args->responder_function = responder_func;
        
#if MODE == SINGLE_THREADED
        webhit(args);
#elif MODE == MULTI_PROCESS
		if ((pid = fork()) < 0)
		{
			logger_func(ERROR, "system call", "fork", 0);
			return 0;
		}
		else
		{
			if (pid == 0) 
			{
				// child
				close(listenfd);
				webhit(args); // never returns
			}
            else
            {
                close(socketfd);
            }
		}
#elif MODE == MULTI_THREADED
        pthread_t threadId;
        if (pthread_create(&threadId, NULL, threadMain, args) !=0)
        {
            if (logger_func != NULL)
            {
                logger_func(ERROR, "system call", "pthread_create", 0);
            }
			continue;
        }
#endif
	}
}

// The same algorithm as found here:
// http://spskhokhar.blogspot.co.uk/2012/09/url-decode-http-query-string.html
void url_decode(char *s)
{
    int i, len = (int)strlen(s);
    char s_copy[len+1];
    char *ptr = s_copy;
    memset(s_copy, 0, sizeof(s_copy));

    for (i=0; i < len; i++)
    {
        if (s[i]=='+')
        {
            *ptr++ = ' ';
        }
        else if ( (s[i] != '%') || (!isdigit(s[i+1]) || !isdigit(s[i+2])) )
        {
            *ptr++ = s[i];
        }
		else
		{
			*ptr++ = ((s[i+1] - '0') << 4) | (s[i+2] - '0');
			i += 2;
		}
    }
    *ptr = 0;
    strcpy(s, s_copy);
}

char* form_value(struct hitArgs *args, int i)
{
    if (i>=args->form_value_counter) return NULL;
    return args->form_values[i].value;
}

char* form_name(struct hitArgs *args, int i)
{
    if (i>=args->form_value_counter) return NULL;
    return args->form_values[i].name;
}

int string_matches_value(char *str, const char *value)
{
    if (str==NULL || value==NULL) return 0;
    return strncmp(str, value, strlen(value))==0;
}

/* ---------- Memory allocation helpers ---------- */

void* malloc_or_quit(size_t num_bytes, const char *src_file, int src_line)
{
    void *mem;
    if ((mem = malloc(num_bytes)) == NULL)
    {
        fprintf(stderr, "file: '%s' at line: %d failed to malloc %zu bytes", src_file, src_line, num_bytes);
        exit(EXIT_FAILURE);
    }
    else
    {
        return mem;
    }
}

void* realloc_or_quit(void *ptr, size_t num_bytes, const char *src_file, int src_line)
{
    void *mem;
    if ((mem = realloc(ptr, num_bytes)) == NULL)
    {
        fprintf(stderr, "file: '%s' at line: %d failed to realloc %zu bytes", src_file, src_line, num_bytes);
        exit(EXIT_FAILURE);
    }
    else
    {
        return mem;
    }
}

void* calloc_or_quit(size_t num, size_t size, const char *src_file, int src_line)
{
    void *mem;
    if ((mem = calloc(num, size)) == NULL)
    {
        fprintf(stderr, "file: '%s' at line: %d failed to calloc [%zu x %zu] bytes",
            src_file, src_line, num, size);
        exit(EXIT_FAILURE);
    }
    else
    {
        return mem;
    }
}

void bcreate(blk *b, int elem_size, int inc)
{
    b->elem_bytes = elem_size;
    b->chunk_size = inc;
    b->ptr = callocx(b->chunk_size, b->elem_bytes);
    b->alloc_bytes = b->chunk_size * b->elem_bytes;
    b->used_bytes = 0;
}

void badd(blk *b, void *data, int len)
{
    if (b->alloc_bytes - b->used_bytes < len)
    {
        while (b->alloc_bytes - b->used_bytes < len)
        {
			b->alloc_bytes+= (b->chunk_size * b->elem_bytes);
        }
        b->ptr=reallocx(b->ptr, b->alloc_bytes);
    }
    memcpy(b->ptr + b->used_bytes, data, len);
    b->used_bytes += len;
    memset(b->ptr + b->used_bytes, 0, b->alloc_bytes - b->used_bytes);
}

void bfree(blk *b)
{
    free(b->ptr);
    b->used_bytes = 0;
    b->alloc_bytes = 0;
}

STRING* new_string(int increments)
{
	STRING *s = mallocx(sizeof(STRING));
	bcreate(s, 1, increments);
	badd(s, "\0", 1);
	return s;
}

void string_add(STRING *s, char *char_array)
{
    s->used_bytes--;
    badd(s, char_array, (int)strlen(char_array)+1);
}

char* string_chars(STRING *s)
{
	return s->ptr;
}

void string_free(STRING *s)
{
	bfree(s);
	free(s);
}

/* ---------- End of memory allocation helpers ---------- */
