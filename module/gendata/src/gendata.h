#ifndef GENDATA_H
#define GENDATA_H

#include <vector>
#include <string>

struct Gendata {
  std::string pkey_name;
  std::string skey_name;
  std::string trace_name;
  int fpkey;
  int lpkey;
  int fskey;
  int lskey;
  int pkinc;
  int skinc;
  int trace_length;
  int sinterval;
  int num_skey;
  int current_pkey;
  float max_time;
  float* trace_buf;
  std::vector<float> trace_data;
  float ormsby_f1;
  float ormsby_f2;
  float ormsby_f3;
  float ormsby_f4;
  void* logger;
};

#ifdef __cplusplus
extern "C" {
#endif

void gendata_init(const char* myid, const char* mod_cfg);
void gendata_process(const char* myid);

#ifdef __cplusplus
}
#endif

#endif /* ifndef GENDATA_H */
