#include "output.h"


#include "geodataflow_capi.h"
#include "vdsstore_capi.h"
#include <stddef.h>
#include <stdlib.h>
#include <moduleconfig_capi.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "gdlogger_capi.h"

  
void output_init(const char* myid, const char* buf)
{
  char c256[256] = "output_";
  struct output_data* my_data = (struct output_data*) malloc(sizeof(struct output_data));

  strcat(c256, myid);
  void* my_logger = LOG_init(c256);
  my_data->logger = my_logger;

  LOG_info(my_logger, "output_init");

  // parse job parameters
  MCID mcid = MC_init(buf);

  // primary key (group) definition
  MC_get_text(mcid, "output.url", c256);
  strcpy(my_data->data_url, c256);

  VSID vsid = VS_init(c256, CREATE, "");
  
  // set up the layout
  int brick_size = 128;
  int lod_levels = 2;
  if (!VS_setup_data_layout(vsid, brick_size, lod_levels) ) {
    LOG_error(my_logger, "Failed to set up the data layout"); 
    DF_set_job_aborted();
    return;
  }

  // set up axes
  int dim = 0;
  float min_data_axis;
  float max_data_axis;
  int num_samples;
  DF_get_data_axis(&min_data_axis, &max_data_axis, &num_samples);
  if (!VS_setup_axis(vsid, dim, num_samples, 
                DF_get_volume_data_name(), 
                DF_get_data_axis_unit(), 
                min_data_axis, 
                max_data_axis)) {
    LOG_error(my_logger, "Failed to set up the data axis"); 
    DF_set_job_aborted();
    return;
  }
  
  // secondary key axis
  dim = 1;
  int min_val = 0;
  int max_val = 0;
  int num_vals = 0;
  LOG_debug(my_logger, "Secondary key: %s\n", DF_get_secondary_key_name());
  DF_get_secondary_key_axis(&min_val, &max_val, &num_vals);
  if (!VS_setup_axis(vsid, dim, num_vals, 
                DF_get_secondary_key_name(),
                "",
                min_val,
                max_val)) {
    LOG_error(my_logger, "Failed to set up the secondary key axis");
    DF_set_job_aborted();
    return;
  }

  // primary key axis
  dim = 2;
  min_val = 0;
  max_val = 0;
  num_vals = 0;
  LOG_debug(my_logger, "xxxxxYYYYxxx primary key: %s\n", DF_get_primary_key_name());
  DF_get_primary_key_axis(&min_val, &max_val, &num_vals);
  if (!VS_setup_axis(vsid, dim, num_vals, 
                DF_get_primary_key_name(),
                "",
                min_val,
                max_val)) {
    LOG_error(my_logger, "Failed to set up the primary key axis\n");
    DF_set_job_aborted();
    return;
  }

  // Add the default data volume
  if (!VS_add_volume_attribute(vsid, VSFORMAT_R32, 1, 
                          DF_get_volume_data_name(),
                          "",
                          0.f,
                          0.f)) {
    LOG_error(my_logger, "Failed to add volume attribute %s\n", DF_get_volume_data_name());
    DF_set_job_aborted();
    return;
  }

  size_t num_attrs = DF_get_num_attributes();
  size_t i;
  int attr_id;
  const char* attr_name;
  struct AttributeInfo attr_info;
  LOG_debug(my_logger, "num_attrs = %d", num_attrs);
  for (i = 0; i < num_attrs; ++i) {
    attr_name = DF_get_attribute_name(i);

    if (strcmp(attr_name, DF_get_primary_key_name()) == 0) {
      continue;
    }

    if (strcmp(attr_name, DF_get_secondary_key_name()) == 0) {
      continue;
    }

    if (strcmp(attr_name, DF_get_volume_data_name()) == 0) {
      continue;
    }

    DF_get_attribute_info(attr_name, &attr_info);
    LOG_info(my_logger, "Adding attribute %s to %s", attr_name, my_data->data_url );

    if (attr_info.length == DF_get_data_vector_length()) {
      if (!VS_add_volume_attribute(vsid, VSFORMAT_R32, 1, 
                                   attr_name,
                                   "",
                                   0.f,
                                   0.f)) {
        LOG_error(my_logger, "Failed to add volume attribute %s\n", attr_name);
        DF_set_job_aborted();
        return;
      }
    }
    else {
      if (!VS_add_trace_attribute(vsid, VSFORMAT_R32, 1, 
                                  attr_name,
                                  "",
                                  0.f,
                                  0.f,
                                  attr_info.length)) {
        LOG_error(my_logger, "Failed to add volume attribute %s\n", attr_name);
        DF_set_job_aborted();
        return;
      }
    }
  }
  
  // Set compression
  VS_setup_compression(vsid, VSZip, 0.01f);

  // Create the dataset
  if (!VS_create(vsid)) {
    LOG_error(my_logger, "Failed to create dataset\n");
    DF_set_job_aborted();
    return;
  }

  
  // save vsid for process stage
  my_data->vsid = vsid;
  DF_set_module_struct(myid, (void*) my_data);
}


void output_process(const char* myid)
{
  int* pkey;
  struct output_data* my_data = (struct output_data*) DF_get_module_struct(myid);
  VSID vsid = my_data->vsid;
  void* my_logger = my_data->logger;

  if (DF_job_finished()) {
    VS_finish(vsid);
    LOG_info(my_logger, "Output VDS dataset: %s", my_data->data_url);
    LOG_flush_log(my_logger, 100);
    return;
  }

  pkey = DF_get_writable_buffer(DF_get_primary_key_name());
  if (pkey == NULL) {
    LOG_error(my_logger, "DF returned buffer of pkey is NULL\n");
    DF_set_job_aborted();
    LOG_flush_log(my_logger, 100);
    return;
  }

  int fpkey, lpkey, num_pkeys;
  DF_get_primary_key_axis(&fpkey, &lpkey, &num_pkeys);
  int pkey_inc = (lpkey - fpkey) / (num_pkeys - 1);
  int pkey_index = (pkey[0] -fpkey) / pkey_inc;
  LOG_info(my_logger, "Process primary key %d, pkey_index=%d\n", pkey[0], pkey_index);


  size_t i;
  int attr_channel_id;
  void* attr_data;
  size_t buf_bytesize;
  int attr_bytesize;
  int grp_size = DF_get_group_size();
#if 1
  const char* attr_name;
  int num_attrs = DF_get_num_attributes();
  LOG_debug(my_logger, "num_attrs = %d, grp_size = %d", num_attrs, grp_size);
  for (i = 0; i < num_attrs; ++i) {
    attr_name = DF_get_attribute_name(i);
    if (!VS_has_attribute(vsid, attr_name)) {
      continue;
    }
    attr_data = DF_get_writable_buffer(attr_name);
    if (attr_data == NULL) {
      continue;
    }
    attr_channel_id = VS_get_attribute_channel_id(vsid, attr_name);
    buf_bytesize = grp_size * DF_get_attribute_byte_size(attr_name); 
  
    LOG_info(my_logger, "Saving attribute %s at primary key %d", attr_name, pkey[0]);
    VS_write_attribute_slice(vsid, attr_channel_id, 2, pkey_index, attr_data, buf_bytesize);
  }
#endif
#if 0
  char attr_name[256];
  int num_attrs = VS_get_number_attributes(vsid);
  LOG_debug(my_logger, "num_attrs = %d, grp_size = %d", num_attrs, grp_size);
  for (i = 0; i < num_attrs; ++i) {
    VS_get_attribute_name(vsid, i, attr_name);
    attr_data = DF_get_writable_buffer(attr_name);
    if (attr_data == NULL) {
      continue;
    }
    attr_channel_id = VS_get_attribute_channel_id(vsid, attr_name);
    buf_bytesize = grp_size * DF_get_attribute_byte_size(attr_name); 
  
    LOG_info(my_logger, "Saving attribute %s at primary key %d", attr_name, pkey[0]);
    VS_write_attribute_slice(vsid, attr_channel_id, 2, pkey_index, attr_data, buf_bytesize);
  }
#endif
}
  
