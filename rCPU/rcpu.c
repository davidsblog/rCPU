#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "dwebsvr.h"

// embedded resource files
#include "code.h"
#include "index.h"
#include "jquery-2-1-0-min.h"
#include "flot.h"

void* polling_thread(void *args);
void send_response(struct hitArgs *args, char*, char*, http_verb);
void log_filter(log_type, char*, char*, int);
void send_cpu_response(struct hitArgs *args, char*, char*);
void send_temp_response(struct hitArgs *args, char*, char*);
int path_ends_with(char *path, char *match);
void send_file_response(struct hitArgs *args, char*, char*, int);
double get_temp();
int get_graph_count();
void get_cpu_use(int *arr, int len);

int max_cpu;
int *usages;
double temp;
pthread_t polling_thread_id;

int main(int argc, char **argv)
{
	if (argc < 2 || !strncmp(argv[1], "-h", 2))
	{
		printf("hint: rcpu [port number]\n");
		return 0;
	}
	
    max_cpu = get_graph_count();
    usages = mallocx(max_cpu * sizeof(int));

    if (pthread_create(&polling_thread_id, NULL, polling_thread, NULL) !=0)
    {
        fprintf(stderr, "Error: pthread_create could not create polling thread");
        exit(EXIT_FAILURE);
    }
    
    // don't read from the console or log anything
    dwebserver(atoi(argv[1]), &send_response, &log_filter);
    
    free(usages);
    return 1; // just to stop compiler complaints
}

void* polling_thread(void *args)
{
    int i=5;
    while (1)
    {
        if (++i >= 5)
        {
            temp = get_temp();
            i=0;
        }
        get_cpu_use(usages, max_cpu);
    }
    return NULL;
}

void log_filter(log_type type, char *s1, char *s2, int socket_fd)
{
    // log to null :-)
}

// decide if we need to send an API response or a file...
void send_response(struct hitArgs *args, char *path, char *request_body, http_verb type)
{
	int path_length=(int)strlen(path);
	if (path_ends_with(path, "cpu.api"))
	{
		return send_cpu_response(args, path, request_body);
	}
	if (path_ends_with(path, "temp.api"))
	{
		return send_temp_response(args, path, request_body);
	}
    
	send_file_response(args, path, request_body, path_length);
}

// receives a number, returns the current CPU use
void send_cpu_response(struct hitArgs *args, char *path, char *request_body)
{
	char tmp[4];
        
	if (args->form_value_counter==1 && !strncmp(form_name(args, 0), "counter", strlen(form_name(args, 0))))
	{
        STRING *response = new_string(32);
        string_add(response, "[");
        for (int p=0; p<max_cpu; p++)
        {
            sprintf(tmp, "%d", usages[p]);
            string_add(response, tmp);
            if (p < max_cpu-1)
            {
                string_add(response, ",");
            }
        }
        string_add(response, "]");
        
        int c = atoi(form_value(args, 0));
        if (c > max_cpu) c=0;
        // TODO: use c if needed
		
        ok_200(args, "\nContent-Type: application/json", string_chars(response), path);
        string_free(response);
	}
	else
	{
		forbidden_403(args, "Bad request");
	}
}

// receives a number, returns the current CPU temperature
void send_temp_response(struct hitArgs *args, char *path, char *request_body)
{
    char tmp[13];
    STRING *response = new_string(32);
    
    if (temp >= 0)
    {
        sprintf(tmp, "%6.2f", temp);
    }
    else
    {
        sprintf(tmp, "?");
    }
    
    string_add(response, tmp);
    ok_200(args, "\nContent-Type: text/plain", string_chars(response), path);
    string_free(response);
}

double get_temp()
{
#ifdef __APPLE__
    return 25 + rand() % 100;
    //return -1;
#else
    FILE *temperature = fopen ("/sys/class/thermal/thermal_zone0/temp", "r");
    if (temperature != NULL)
    {
        double cpu_temp = 0;
        fscanf(temperature, "%lf", &cpu_temp);
        fclose (temperature);
        return cpu_temp / 1000;
    }
    else
    {
        return -1;
    }
#endif
}

int path_ends_with(char *path, char *match)
{
    int match_len = (int)strlen(match);
    int path_length = (int)strlen(path);
    return (path_length >= match_len && !strncmp(&path[path_length-match_len], match, match_len));
}

void send_file_response(struct hitArgs *args, char *path, char *request_body, int path_length)
{
    STRING *response = new_string(1024);
    string_add(response, "HTTP/1.1 200 OK\n");
    string_add(response, "Connection: close\n");
    string_add(response, "Content-Type: ");
    
	if (!strcmp(path, "") || path_ends_with(path, "index.html"))
    {
        string_add(response, "text/html");
        write_header(args->socketfd, string_chars(response), index_html_len);
        write(args->socketfd, index_html, index_html_len);
    }
    else if (path_ends_with(path, "code.js"))
    {
        string_add(response, "text/javascript");
        write_header(args->socketfd, string_chars(response), code_js_len);
        write(args->socketfd, code_js, code_js_len);
    }
    else if (path_ends_with(path, "jquery-2-1-0-min.js"))
    {
        string_add(response, "text/javascript");
        write_header(args->socketfd, string_chars(response), jquery_2_1_0_min_js_len);
        write(args->socketfd, jquery_2_1_0_min_js, jquery_2_1_0_min_js_len);
    }
    else if (path_ends_with(path, "flot.js"))
    {
        string_add(response, "text/javascript");
        write_header(args->socketfd, string_chars(response), flot_js_len);
        write(args->socketfd, flot_js, flot_js_len);
    }
    else
    {
        notfound_404(args, "no such file");
    }
    
    string_free(response);
    
    // allow socket to drain before closing
	sleep(1);
}

// adapted from here: http://phoxis.org/2013/09/05/finding-overall-and-per-core-cpu-utilization

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
  retval = sscanf (buffer, "c%*s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
        &fields[0], &fields[1], &fields[2], &fields[3], &fields[4],
        &fields[5], &fields[6], &fields[7], &fields[8], &fields[9]);

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

// Returns the number of CPUs +1
int get_graph_count()
{
#ifdef __APPLE__
    // just so that I can test it in Xcode on my Mac...
    return 4;
#else
	char buffer[BUF_MAX];
	int count=0;
	FILE *fp = fopen ("/proc/stat", "r");
	if (fp == NULL)
	{
		return -1;
	}
    
	while (fgets (buffer, BUF_MAX, fp) != NULL)
	{
		if (strncmp(buffer, "cpu", 3) != 0) break;
		count++;
	}
	fclose (fp);
	return count;
#endif
}

void get_cpu_use(int *cpu, int len)
{
	int count;
    
#ifdef __APPLE__
    // just so that I can test it in xcode on my mac...
    for (count = 0; count < len; count++)
    {
        cpu[count] = rand() % 100;
    }
    sleep(1);
    return;
#else
    unsigned long long int fields[10], total_tick[len], total_tick_old[len], idle[len], idle_old[len], del_total_tick[len], del_idle[len];
    int i, cpus = 0;
    double percent_usage;
    
    FILE *fp = fopen ("/proc/stat", "r");
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
    
    for (count = 0; count < len; count++)
    {
        total_tick_old[count] = total_tick[count];
        idle_old[count] = idle[count];
        
        if (!read_fields (fp, fields))
        {
            fclose (fp);
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
        
        cpu[count] = (int)percent_usage;
    }
    
    fclose (fp);
#endif
}
