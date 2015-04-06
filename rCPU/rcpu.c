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
void get_cpu_use(int *cpu, int len);

int main(int argc, char **argv)
{
    if (argc < 2 || !strncmp(argv[1], "-h", 2))
	{
		printf("hint: rcpu [port number]\n");
		return 0;
	}
    
    // don't read from the console or log anything
    dwebserver(atoi(argv[1]), &send_response, &log_filter);
}

void log_filter(log_type type, char *s1, char *s2, int socket_fd)
{
    // log to null :-)
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

#define MAX_CPU 128
int arr[MAX_CPU] = { 0 };

// a simple API, it receives a number, increments it and returns the response
void send_api_response(struct hitArgs *args, char *path, char *request_body)
{
	char tmp[4];
        
	if (args->form_value_counter==1 && !strncmp(form_name(args, 0), "counter", strlen(form_name(args, 0))))
	{
        get_cpu_use(&arr[0], MAX_CPU);
        
        STRING *response = new_string(32);
        string_add(response, "[");
        for (int p=0; p<MAX_CPU; p++)
        {
            sprintf(tmp, "%d", arr[p]);
            string_add(response, tmp);
            if (p<MAX_CPU-1)
            {
                string_add(response, ", ");
            }
        }
        string_add(response, "]");
        
        int c = atoi(form_value(args, 0));
		if (c>MAX_CPU) c=0;
        // TODO: use c if needed
		
		ok_200(args, "\nContent-Type: text/plain", string_chars(response), path);
        string_free(response);
	}
	else
	{
		forbidden_403(args, "Bad request");
	}
}

void send_file_response(struct hitArgs *args, char *path, char *request_body, int path_length)
{
	int file_id, i;
	long len;
	char *content_type = NULL;
    STRING *response = new_string(FILE_CHUNK_SIZE);
	
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
    
    string_add(response, "HTTP/1.1 200 OK\n");
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

/* Show per core CPU utilization of the system
 * This is a part of the post http://phoxis.org/2013/09/05/finding-overall-and-per-core-cpu-utilization
 */
#include <stdio.h>
#include <unistd.h>

#define BUF_MAX 1024

int read_fields (FILE *fp, unsigned long long int *fields)
{
  int retval;
  char buffer[BUF_MAX];

  if (!fgets (buffer, BUF_MAX, fp))
  {
      return -1;
  }
  /* line starts with c and a string. This is to handle cpu, cpu[0-9]+ */
  retval = sscanf (buffer, "c%*s %Lu %Lu %Lu %Lu %Lu %Lu %Lu %Lu %Lu %Lu",
                            &fields[0], 
                            &fields[1], 
                            &fields[2], 
                            &fields[3], 
                            &fields[4], 
                            &fields[5], 
                            &fields[6], 
                            &fields[7], 
                            &fields[8], 
                            &fields[9]); 
  if (retval == 0)
  {
      return -1;
  }
  if (retval < 4) /* At least 4 fields is to be read */
  {
    fprintf (stderr, "Error reading /proc/stat cpu field\n");
    return 0;
  }
  return 1;
}

void get_cpu_use(int cpu[], int len)
{
  FILE *fp;
  unsigned long long int fields[10], total_tick[MAX_CPU], total_tick_old[MAX_CPU], idle[MAX_CPU], idle_old[MAX_CPU], del_total_tick[MAX_CPU], del_idle[MAX_CPU];
  int update_cycle = 0, i, cpus = 0, count;
  double percent_usage;

  fp = fopen ("/proc/stat", "r");
  if (fp == NULL)
  {
    return;
  }

  while (read_fields (fp, fields) != -1)
  {
    for (i=0, total_tick[cpus] = 0; i<10; i++)
    {
        total_tick[cpus] += fields[i];
    }
    idle[cpus] = fields[3]; /* idle ticks index */
    cpus++;
  }

    sleep (1);
    fseek (fp, 0, SEEK_SET);
    fflush (fp);
    printf ("[Update cycle %d]\n", update_cycle); 
    for (count = 0; count < cpus; count++)
    {
      total_tick_old[count] = total_tick[count];
      idle_old[count] = idle[count];
    
      if (!read_fields (fp, fields))
      {
          return;
      }

      for (i=0, total_tick[count] = 0; i<10; i++)
      {
          total_tick[count] += fields[i];
      }
      idle[count] = fields[3];

      del_total_tick[count] = total_tick[count] - total_tick_old[count];
      del_idle[count] = idle[count] - idle_old[count];

      percent_usage = ((del_total_tick[count] - del_idle[count]) / (double) del_total_tick[count]) * 100;
      if (count == 0)
      {
          printf ("Total CPU Usage: %3.2lf%%\n", percent_usage);
      }
      else 
      {
          printf ("\tCPU%d Usage: %3.2lf%%\n", count - 1, percent_usage);
      }

      cpu[count] = (int)percent_usage;
    }
    //update_cycle++;
    //printf ("\n");

  /* Ctrl + C quit, therefore this will not be reached. We rely on the kernel to close this file */
  fclose (fp);
}
