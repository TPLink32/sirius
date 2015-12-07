#include <stdio.h>
#include "Analyp.h"

#define ARRLEN 1024*1024
int arr_p;
double filted_arr[ARRLEN];

static int
simple_handler(CManager cm, void *vevent, void *client_data, attr_list attrs)
{
    filtered_particles_ptr event = vevent;
    printf("I got %d\n", event->num_mi);
    printf("\n");

    int i;
    for(i = 0; i < event->num_mi*7; i++)
	filted_arr[arr_p+i] = event->pgmi_qualify[i];
    arr_p+=i;
    printf("the total filted paticles # %d\n\n", arr_p/7);
    for(i = 0; i < arr_p; i++){
	if(i%7 == 0) printf("\n");
	printf("%lf ", filted_arr[i]);
    }
    printf("\n\n");
}

/* this file is evpath/examples/net_recv.c */
int main(int argc, char **argv)
{
    CManager cm;
    EVstone stone;
    char *string_list;

    arr_p = 0;
    cm = CManager_create();
    CMlisten(cm);

    stone = EValloc_stone(cm);
    EVassoc_terminal_action(cm, stone, filtered_particles_format_list, simple_handler, NULL);
    string_list = attr_list_to_string(CMget_contact_list(cm));
    printf("Contact list \"%d:%s\"\n", stone, string_list);
    CMrun_network(cm);
}
