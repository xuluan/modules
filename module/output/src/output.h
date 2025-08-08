#ifndef OUTPUT_H
#define OUTPUT_H 

#include <vdsstore_capi.h>

struct output_data {
  char data_url[256];
  char connect_string[256];
  VSID vsid;
  void* logger;
};

void output_init(const char* myid, const char* mod_cfg);
void output_process(const char* myid);

#endif /* ifndef OUTPUT_H */
