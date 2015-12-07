#include <stdlib.h>
#include <stdio.h>
#include "Analyp.h"

int main(int argc, char **argv){
	CManager cm;
	filtered_particles filpar;
	EVstone stone;
	EVsource source;
	char string_list[2048];
	attr_list contact_list;
	EVstone remote_stone;

	int rt;

        filpar.pgmi_qualify = (double *)malloc(sizeof(double)*7*PG_NUM);

	if (filpar.pgmi_qualify == NULL) {
                printf("Allocate filter results array filpar.pgmi_qualify failed\n");
                exit(0); // cannot allocate memory
        }

	filpar.gts_local_features = (gts_local *)malloc(sizeof(gts_local));

	rt = Analyp(filpar.pgmi_qualify, &(filpar.num_mi), filpar.gts_local_features);

	printf("The filted count #  %d\n", filpar.num_mi);

	int i;

        for(i = 0; i < filpar.num_mi*7; i++){
                if(i%7 == 0) printf("\n");
                printf("%lf ", filpar.pgmi_qualify[i]);
        }
	printf("\n\n");

	if (sscanf(argv[1], "%d:%s", &remote_stone, &string_list[0]) != 2) {
		printf("Bad arguments \"%s\"\n", argv[1]);
		exit(0);
	}

	cm = CManager_create();
	CMlisten(cm);

	stone = EValloc_stone(cm);
	contact_list = attr_list_from_string(string_list);
	EVassoc_bridge_action(cm, stone, contact_list, remote_stone);

	source = EVcreate_submit_handle(cm, stone, filtered_particles_format_list);
//	filpar.num_mi = 30;
	EVsubmit(source, &filpar, NULL);

	return rt;
}
