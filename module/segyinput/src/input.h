#ifndef INPUT_H
#define INPUT_H

#include <vector>
#include <memory>
#include <GdLogger.h>
#include <VdsStore.h>

struct Input {
  int pkey_dim;                 // the dimension of the pkey in VDS
  int current_pkey_index;
  std::vector<int> pkeys;
  std::vector<int> skeys;
  void* logger;
  ovds::VdsStore* vsid;
  std::string data_url;
};

#ifdef __cplusplus
extern "C" {
#endif

void input_init(const char* myid, const char* mod_cfg);
void input_process(const char* myid);

#ifdef __cplusplus
}
#endif

#endif /* ifndef INPUT_H */
