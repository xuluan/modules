#include "demoattr.h"
#include "GeoDataFlow.h"
#include <ModuleConfig.h>
#include <GdLogger.h>
#include <string>

void demoattr_init(const char* myid, const char* buf)
{
  std::string logger_name = std::string {"demoattr_"} + myid;
  auto& gd_logger = gdlog::GdLogger::GetInstance();
  void* my_logger = gd_logger.Init(logger_name);
  gd_logger.LogInfo(my_logger, "demoattr_init");

  // Need to pass the pointer to DF after init function returns
  // So we use raw pointer instead of a smart pointer here
  Demoattr* my_data = new Demoattr {};

  my_data->logger = my_logger;

  auto& job_df = df::GeoDataFlow::GetInstance();

  // A handy function to clean up resources if errors happen
  auto _clean_up = [&] ()-> void {
    if (my_data != nullptr) {
      delete my_data;
    }
  };

  // parse job parameters
  mc::ModuleConfig mod_conf = mc::ModuleConfig {};  mod_conf.Parse(buf);
  my_data->fval = mod_conf.GetFloat("demoattr.fval");
  if(mod_conf.HasError()) {
    gd_logger.LogError(my_logger, "Failed to get fval. Error: {}", mod_conf.ErrorMessage().c_str());
    job_df.SetJobAborted();
    _clean_up();
    return;
  }

  // As a demo, add a new attribute whose value will be the secondary key's multiplied by the fval
  my_data->attr_name = std::string("demoattr");
  int attr_length = 1;
  job_df.AddAttribute(my_data->attr_name.c_str(), as::DataFormat::FORMAT_U32, attr_length);

  job_df.SetModuleStruct(myid, static_cast<void*>(my_data));
}

void demoattr_process(const char* myid)
{
  auto& gd_logger = gdlog::GdLogger::GetInstance();
  auto& job_df = df::GeoDataFlow::GetInstance();

  Demoattr* my_data = static_cast<Demoattr*>(job_df.GetModuleStruct(myid));
  void* my_logger = my_data->logger;

  // A handy function to clean up resources if errors happen, 
  // or job has finished
  auto _clean_up = [&] ()-> void {
    if (my_data != nullptr) {
      delete my_data;
    }
  };

  if (job_df.JobFinished()) {
    _clean_up();
    return;
  }

  int* pkey;
  pkey = static_cast<int*>(job_df.GetWritableBuffer(job_df.GetPrimaryKeyName()));
  if(pkey == nullptr) {
    gd_logger.LogError(my_logger, "DF returned a nullptr to the buffer of pkey is NULL");
    job_df.SetJobAborted();
    _clean_up();
    return;
  }

  gd_logger.LogInfo(my_logger, "Process primary key {}\n", pkey[0]);

  int* attr_buf = static_cast<int*>(job_df.GetWritableBuffer(my_data->attr_name.c_str()));
  int grp_size = job_df.GetGroupSize();

  std::string skey_name = job_df.GetSecondaryKeyName();
  int* skey_buf = static_cast<int*>(job_df.GetWritableBuffer(skey_name.c_str()));

  std::transform(attr_buf, attr_buf + grp_size,
                 skey_buf,
                 [my_data](auto val) { return val * my_data->fval; });

}
