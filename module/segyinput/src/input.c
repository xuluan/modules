#include "input.h"

#include "geodataflow_capi.h"
#include "vdsstore_capi.h"
#include <stddef.h>
#include <stdlib.h>
#include <moduleconfig_capi.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>

int select_channel(VSID vsid, int sz)
{
  char cname[40];
  char unit[40];
  int width;
  int height;

  for(int i = 0; i< VS_get_number_channels(vsid); i++) {
    VS_get_channel_attr(vsid, 2, i, 0, cname, unit, &width, &height);
    if ((width * height) == sz) {
      printf("select_channel %d, %d, %d\n", i, width, height);
      return i;
    }
  } 
  printf("select_channel fail\n");

  return -1; //no found
}

void input_init(const char* myid, const char* buf)
{
  char c256[256];
  struct input_data* my_data = (struct input_data*) malloc(sizeof(struct input_data));
  // parse job parameters
  MCID mcid = MC_init(buf);

  // primary key (group) definition
  MC_get_text(mcid, "input.url", c256);
  strcpy(my_data->data_url, c256);

  VSID vsid = VS_init(c256, READ, "");
  if (!vsid) {
    printf("Failed to open dataset\n");
    DF_set_job_aborted();
    return;
  }  
  my_data->vsid = vsid;
  DF_set_module_struct(myid, (void*) my_data);

  // set up axes
  float min;
  float max;
  int dim = 0;

  VS_get_axis(vsid, dim, &my_data->trace_length, my_data->trace_name, my_data->trace_unit, &my_data->tmin, &my_data->tmax, &my_data->tstep);

  dim = 1;
  VS_get_axis(vsid, dim, &my_data->num_skey, my_data->skey_name, my_data->skey_unit, &min, &max, &my_data->sstep);
  my_data->smin = (int)roundf(min);
  my_data->smax = (int)roundf(max);

  dim = 2;
  VS_get_axis(vsid, dim, &my_data->num_pkey, my_data->pkey_name, my_data->pkey_unit, &min, &max, &my_data->pstep);
  my_data->pmin = (int)roundf(min);
  my_data->pmax = (int)roundf(max);

  // Add trace attribute

  DF_add_float_attribute(my_data->trace_name, my_data->trace_length); 
  DF_set_volume_data_name(my_data->trace_name);

  DF_set_data_axis_unit(my_data->trace_unit);

  DF_set_data_axis(my_data->tmin, my_data->tmax, my_data->trace_length);

  printf("set dim 0 done\n");

  // Add second key attribute
  DF_add_int_attribute(my_data->skey_name, 1);
  DF_set_secondary_key_name(my_data->skey_name);

  // set the group size, to allocate buffers
  DF_set_group_size(my_data->num_skey);  
  // set up secondary key axis
  DF_set_secondary_key_axis(my_data->smin, my_data->smax, my_data->num_skey);

  printf("set dim 1 done\n");


  // Add primary key attribute
  DF_add_int_attribute(my_data->pkey_name, 1);
  DF_set_primary_key_name(my_data->pkey_name);

  // Set up primary key axis
  DF_set_primary_key_axis(my_data->pmin, my_data->pmax, my_data->num_pkey);

  printf("set dim 2 done\n");

  // select channel
  int channel_id = select_channel(vsid, my_data->num_skey * my_data->trace_length);
  if(channel_id < 0) {
    printf("Failed to select channel\n");
    DF_set_job_aborted();
    return;    
  }

  printf("select_channel done\n");

  my_data->channel_id = channel_id;
  // reset the buffers
  float *trc = (float *) DF_get_writable_buffer(my_data->trace_name);
  if (trc == NULL) {
    // log the error
    // cancel the job
    DF_set_job_aborted();
    return;
  }

  memset(trc, 0, my_data->num_skey * my_data->trace_length * sizeof(float));
  my_data->current_pkey = my_data->pmin;
}

void input_process(const char* myid)
{
  struct input_data* my_data = (struct input_data*) DF_get_module_struct(myid);
  int *pkey, *skey;
  int num_skey;
  int i;

  VSID vsid = my_data->vsid;

  // have we reached the end of data
  if (my_data->pstep > 0) {
   if (my_data->current_pkey > my_data->pmax) {
      // set the flag
      DF_set_job_finished();
      VS_finish(vsid);
      return;
   }
    
  }
  else {
   if (my_data->current_pkey < my_data->pmin) {
      // set the flag
      DF_set_job_finished();
      VS_finish(vsid);
      return;
   }
  }
  
  float *trc = (float *) DF_get_writable_buffer(my_data->trace_name);
  if (trc == NULL) {
    // log the error
    // cancel the job
    DF_set_job_aborted();
    VS_finish(vsid);
    return;
  }

  VS_get_channel_slice(vsid, 2, 0, my_data->channel_id, 0, trc, my_data->num_skey * my_data->trace_length * sizeof(float));
  
  printf("Current pkey = %d\n", my_data->current_pkey);
  printf("Module parameters:\n");
  printf("\tpkey_name=%s\n", my_data->pkey_name);
  printf("\tpmin=%d\n", my_data->pmin);
  printf("\tpmax=%d\n", my_data->pmax);
  printf("\tpstep=%d\n", my_data->pstep);
  printf("\tskey_name=%s\n", my_data->skey_name);
  printf("\tsmin=%d\n", my_data->smin);
  printf("\tsmax=%d\n", my_data->smax);
  printf("\ttstep=%d\n", my_data->tstep);
  
  pkey = (int *) DF_get_writable_buffer(my_data->pkey_name);
  skey = (int *) DF_get_writable_buffer(my_data->skey_name);
  num_skey = my_data->num_skey;

  for (i = 0; i < num_skey; ++i) {
    pkey[i] = my_data->current_pkey;
    skey[i] = my_data->smin + i * my_data->sstep;
  }
 
  my_data->current_pkey = my_data->current_pkey + my_data->pstep;
}
