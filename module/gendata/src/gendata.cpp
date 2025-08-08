#include "gendata.h"
#include "ArrowStore.h"
#include "GeoDataFlow.h"
#include <ModuleConfig.h>
#include <GdLogger.h>
#include <string>
#include <vector>
#include <cmath>
#include "fort.hpp"
#include <iostream>


// Ref: https://www.fitzgibbon.ie/floating-point-equality
float simple_sinc(float x)
{
  if (std::fabs(x) < 0.0406015441f) {
    return 1.0f - (x * x) * (1.0f / 6.0f);
  }
  else
    return std::sin(x) / x;
}


void add_ormsby_to_trace(std::vector<float>& trc,
            std::vector<float> const& times,
            const float f1,
            const float f2,
            const float f3,
            const float f4,
            const float sinterval,
            const int gate
) 
{
  int num_times = times.size(); 

  // convert sampling interval from us to ms, then take the reverse
  float reverse_si = 1000.0f / sinterval;

  // sinterval in seconds
  float si_sec = sinterval * 1e-6;

  float _pi = 3.141592654f;

  float pi_f1 = _pi * f1;
  float pi_f2 = _pi * f2;
  float pi_f3 = _pi * f3;
  float pi_f4 = _pi * f4;
  float reverse_f2_f1 = 1.0f / (f2 - f1);
  float reverse_f4_f3 = 1.0f / (f4 - f3);

  int gate_length  = std::floor(gate * reverse_si);
  int trace_length = trc.size();

  for (auto i = 0; i < num_times; ++i) {
    int time_index  = std::floor(times[i] * reverse_si);

    int gate_beg = time_index - gate_length/2;
    if (gate_beg < 0) {
      gate_beg = 0;
    }

    int gate_end = gate_beg + gate_length;
    if (gate_end > trace_length) {
      gate_end = trace_length - 1;
    }

    for (auto k = gate_beg; k < gate_end; ++k) {
      float delta = (k-time_index) * si_sec;
      float tmp1 = simple_sinc(pi_f1 * delta);
      float tmp2 = simple_sinc(pi_f2 * delta);
      float tmp3 = simple_sinc(pi_f3 * delta);
      float tmp4 = simple_sinc(pi_f4 * delta);
      float ormsby = reverse_f2_f1 * ((pi_f1 * f1) * std::pow(tmp1, 2) - 
                                      (pi_f2 * f2) * std::pow(tmp2, 2)) - 
                     reverse_f4_f3 * ((pi_f3 * f3) * std::pow(tmp3, 2) - 
                                      (pi_f4 * f4) * std::pow(tmp4, 2));
                                      
      trc[k] += ormsby;
    }
  }
}

void add_ricker_to_trace(std::vector<float>& trc,
            std::vector<float> const& times,
            const float peak_freq,
            const float sinterval,
            const int gate
) 
{
  float pi_peak_freq = peak_freq * 3.141592654f;

  int num_times = times.size(); 
  int gate_length  = std::floor(gate / (sinterval * 0.001f));
  int trace_length = trc.size();

  for (auto i = 0; i < num_times; ++i) {
    int time_index  = std::floor(times[i] / (sinterval * 0.001f));

    int gate_beg = time_index - gate_length/2;
    if (gate_beg < 0) {
      gate_beg = 0;
    }

    int gate_end = gate_beg + gate_length;
    if (gate_end > trace_length) {
      gate_end = trace_length - 1;
    }

    for (auto k = gate_beg; k < gate_end; ++k) {
      float delta = (k-time_index) * sinterval*1.e-6;
      float rick = (pi_peak_freq * delta) * (pi_peak_freq * delta);
      rick = (1.f - 2.f * rick)*expf(-rick);
      trc[k] += rick;
    }
  }
}

void gendata_init(const char* myid, const char* buf)
{
  std::string logger_name = std::string {"gendata_"} + myid;
  auto& gd_logger = gdlog::GdLogger::GetInstance();
  void* my_logger = gd_logger.Init(logger_name);
  gd_logger.LogInfo(my_logger, "gendata_init");

  // Need to pass the pointer to DF after init function returns
  // So we use raw pointer instead of a smart pointer here
  Gendata* my_data = new Gendata {};

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
  my_data->max_time = mod_conf.GetInt("gendata.maxtime");
  if(mod_conf.HasError()) {
    gd_logger.LogError(my_logger, "Failed to get maxtime. Error: {}", 
                       mod_conf.ErrorMessage());
    job_df.SetJobAborted();
    _clean_up();
    return;
  }

  // primary key (group) definition
  mod_conf.GetText("gendata.primarykey.name", my_data->pkey_name);
  if (mod_conf.HasError()) {
    gd_logger.LogError(my_logger, "Failed to get primarykey name. Error: {}", 
                       mod_conf.ErrorMessage());
    job_df.SetJobAborted();
    _clean_up();
    return;
  }
  std::transform(my_data->pkey_name.begin(), my_data->pkey_name.end(),
                 my_data->pkey_name.begin(), [](unsigned char c) { return std::toupper(c);});

  my_data->fpkey = mod_conf.GetInt("gendata.primarykey.first");
  if (mod_conf.HasError()) {
    gd_logger.LogError(my_logger, "Failed to get primarykey first. Error: {}", 
                       mod_conf.ErrorMessage());
    job_df.SetJobAborted();
    _clean_up();
    return;
  }

  my_data->lpkey = mod_conf.GetInt("gendata.primarykey.last");
  if (mod_conf.HasError()) {
    gd_logger.LogError(my_logger, "Failed to get primarykey last. Error: {}", 
                       mod_conf.ErrorMessage());
    job_df.SetJobAborted();
    _clean_up();
    return;
  }

  my_data->pkinc = mod_conf.GetInt("gendata.primarykey.step");
  if (mod_conf.HasError()) {
    gd_logger.LogError(my_logger, "Failed to get primarykey step. Error: {}", 
                       mod_conf.ErrorMessage());
    job_df.SetJobAborted();
    _clean_up();
    return;
  }

  // secondary key (subset) definition
  mod_conf.GetText("gendata.secondarykey.name", my_data->skey_name);
  if (mod_conf.HasError()) {
    gd_logger.LogError(my_logger, "Failed to get secondarykey name. Error: {}", 
                       mod_conf.ErrorMessage());
    job_df.SetJobAborted();
    _clean_up();
    return;
  }
  std::transform(my_data->skey_name.begin(), my_data->skey_name.end(),
                 my_data->skey_name.begin(), [](unsigned char c) { return std::toupper(c);});

  my_data->fskey = mod_conf.GetInt("gendata.secondarykey.first");
  if (mod_conf.HasError()) {
    gd_logger.LogError(my_logger, "Failed to get secondarykey first. Error: {}", 
                       mod_conf.ErrorMessage());
    job_df.SetJobAborted();
    _clean_up();
    return;
  }

  my_data->lskey = mod_conf.GetInt("gendata.secondarykey.last");
  if (mod_conf.HasError()) {
    gd_logger.LogError(my_logger, "Failed to get secondarykey last. Error: {}", 
                       mod_conf.ErrorMessage());
    job_df.SetJobAborted();
    _clean_up();
    return;
  }

  my_data->skinc = mod_conf.GetInt("gendata.secondarykey.step");
  if (mod_conf.HasError()) {
    gd_logger.LogError(my_logger, "Failed to get secondarykey step. Error: {}", 
                       mod_conf.ErrorMessage());
    job_df.SetJobAborted();
    _clean_up();
    return;
  }

  // nummber of secondary key, which is used as group size later in the process
  my_data->num_skey = (my_data->lskey - my_data->fskey) / my_data->skinc + 1;

  my_data->sinterval = mod_conf.GetInt("gendata.sinterval");
  if (mod_conf.HasError()) {
    gd_logger.LogError(my_logger, "Failed to get sampling interval sinterval. Error: {}", 
                       mod_conf.ErrorMessage());
    job_df.SetJobAborted();
    _clean_up();
    return;
  }

  // sampling interval is in micro-seconds, max_time is in ms
  my_data->trace_length = my_data->max_time / (my_data->sinterval * 0.001f) + 1;

  // initialize the trace_data with zeros
  my_data->trace_data = std::vector<float>(my_data->trace_length, 0.f);

  // trace data name
  mod_conf.GetText("gendata.dataname", my_data->trace_name);
  if (mod_conf.HasError()) {
    gd_logger.LogError(my_logger, "Failed to get dataname. Error: {}", 
                       mod_conf.ErrorMessage());
    job_df.SetJobAborted();
    _clean_up();
    return;
  }
  std::transform(my_data->trace_name.begin(), my_data->trace_name.end(),
                 my_data->trace_name.begin(), [](unsigned char c) { return std::toupper(c);});

  my_data->current_pkey = my_data->fpkey; 

  // Store the information to the DF for later use in the process phase
  job_df.SetModuleStruct(myid, static_cast<void*>(my_data));

  // Dump key parameters
  gd_logger.LogInfo(my_logger, "Create_init has done!");
  gd_logger.LogInfo(my_logger, "Module parameters:");
  gd_logger.LogInfo(my_logger, "pkey_name={}", my_data->pkey_name);
  gd_logger.LogInfo(my_logger, "fpkey={}", my_data->fpkey);
  gd_logger.LogInfo(my_logger, "lpkey={}", my_data->lpkey);
  gd_logger.LogInfo(my_logger, "pkinc={}", my_data->pkinc);
  gd_logger.LogInfo(my_logger, "skey_name={}", my_data->skey_name);
  gd_logger.LogInfo(my_logger, "fskey={}", my_data->fskey);
  gd_logger.LogInfo(my_logger, "lskey={}", my_data->lskey);
  gd_logger.LogInfo(my_logger, "skinc={}", my_data->skinc);
  gd_logger.LogInfo(my_logger, "Data name={}", my_data->trace_name);
  gd_logger.LogInfo(my_logger, "Maximum time={} ms", my_data->max_time);
  gd_logger.LogInfo(my_logger, "Sampling interval time={} microseconds", my_data->sinterval);

  // Add primary and secondary keys. 
  // Firstly, add two attributes, then set them to keys respectively
  // This way, we can check the data formats of the attributes. Only interger 
  // attributes can be used as keys
  job_df.AddAttribute(my_data->pkey_name.c_str(), as::DataFormat::FORMAT_U32, 1);
  job_df.AddAttribute(my_data->skey_name.c_str(), as::DataFormat::FORMAT_U32, 1);

  job_df.SetPrimaryKeyName(my_data->pkey_name.c_str());
  job_df.SetSecondaryKeyName(my_data->skey_name.c_str());

  // Add trace attribute
  job_df.AddAttribute(my_data->trace_name.c_str(), 
                      as::DataFormat::FORMAT_R32, 
                      my_data->trace_length);
  job_df.SetVolumeDataName(my_data->trace_name.c_str());

  job_df.SetDataAxisUnit("ms");
  // set the group size, to allocate buffers
  job_df.SetGroupSize(my_data->num_skey);

  // Set up data axis
  float min_time = 0.f;
  float max_time = my_data->max_time;
  int trace_length = my_data->trace_length;
  job_df.SetDataAxis(min_time, max_time, trace_length);

  // Set up primary key axis
  int num_pkeys = (my_data->lpkey - my_data->fpkey) / my_data->pkinc + 1;
  job_df.SetPrimaryKeyAxis(my_data->fpkey, my_data->lpkey, num_pkeys);

  // set up secondary key axis
  job_df.SetSecondaryKeyAxis(my_data->fskey, my_data->lskey, my_data->num_skey);

  // reset the buffers
  float *trc = static_cast<float*>(job_df.GetWritableBuffer(my_data->trace_name.c_str()));
  if (trc == nullptr) {
    gd_logger.LogError(my_logger, "Failed to get buffer to write for dataname. Error: {}", 
                       mod_conf.ErrorMessage());
    // cancel the job
    job_df.SetJobAborted();
    return;
  }

  std::fill(trc, trc + my_data->num_skey * my_data->trace_length, 0.f); 

  // Check parameters of signals
  if (mod_conf.Has("gendata.signal.ormsby")) {
    my_data->ormsby_f1 = mod_conf.GetFloat("gendata.signal.ormsby.f1");
    if (mod_conf.HasError()) {
      gd_logger.LogError(my_logger, "Failed to get Ormsby f1. Error: {}", 
                         mod_conf.ErrorMessage());
      job_df.SetJobAborted();
      _clean_up();
      return;
    }

    my_data->ormsby_f2 = mod_conf.GetFloat("gendata.signal.ormsby.f2");
    if (mod_conf.HasError()) {
      gd_logger.LogError(my_logger, "Failed to get Ormsby f2. Error: {}", 
                         mod_conf.ErrorMessage());
      job_df.SetJobAborted();
      _clean_up();
      return;
    }

    my_data->ormsby_f3 = mod_conf.GetFloat("gendata.signal.ormsby.f3");
    if (mod_conf.HasError()) {
      gd_logger.LogError(my_logger, "Failed to get Ormsby f3. Error: {}", 
                         mod_conf.ErrorMessage());
      job_df.SetJobAborted();
      _clean_up();
      return;
    }

    my_data->ormsby_f4 = mod_conf.GetFloat("gendata.signal.ormsby.f4");
    if (mod_conf.HasError()) {
      gd_logger.LogError(my_logger, "Failed to get Ormsby f4. Error: {}", 
                         mod_conf.ErrorMessage());
      job_df.SetJobAborted();
      _clean_up();
      return;
    }

    float gate = mod_conf.GetFloat("gendata.signal.ormsby.gate");
    if (mod_conf.HasError()) {
      gd_logger.LogError(my_logger, "Failed to get ormsby gate. Error: {}", 
                         mod_conf.ErrorMessage());
      job_df.SetJobAborted();
      _clean_up();
      return;
    }
    if (gate > my_data->max_time) {
      gd_logger.LogError(my_logger, "ormsby gate {} > max time {}. Error: {}", 
                         gate, my_data->max_time, mod_conf.ErrorMessage());
      job_df.SetJobAborted();
      _clean_up();
      return;
    }

    size_t num_times = mod_conf.GetSize("gendata.signal.ormsby.times");
    if (num_times < 1l) {
      gd_logger.LogError(my_logger, "Failed to find any valid ormsby times. Error: {}", 
                         mod_conf.ErrorMessage());
      job_df.SetJobAborted();
      _clean_up();
      return;
    }

    std::vector<float> times;
    times.resize(num_times);
    mod_conf.GetArrayFloat("gendata.signal.ormsby.times", times.data());
    if (mod_conf.HasError()) {
      gd_logger.LogError(my_logger, "Failed to get ormsby times. Error: {}", 
                         mod_conf.ErrorMessage());
      job_df.SetJobAborted();
      _clean_up();
      return;
    }

    // Dump Ormsby parameters
    if (num_times > 1) 
      gd_logger.LogInfo(my_logger, "Add Ormsby wavelets with parameters:");
    else
      gd_logger.LogInfo(my_logger, "Add Ormsby wavelet with parameters:");
    gd_logger.LogInfo(my_logger, "f1={} Hz", my_data->ormsby_f1);
    gd_logger.LogInfo(my_logger, "f2={} Hz", my_data->ormsby_f2);
    gd_logger.LogInfo(my_logger, "f3={} Hz", my_data->ormsby_f3);
    gd_logger.LogInfo(my_logger, "f4={} Hz", my_data->ormsby_f4);
    gd_logger.LogInfo(my_logger, "gate={} ms", gate);
    gd_logger.LogInfo(my_logger, "times (ms):");
    for (auto i = 0; i < num_times; ++i) {
      gd_logger.LogInfo(my_logger, "  {}", times[i]);
    }

    // calculate the Ormsby wavelet
    // add ricker wavelets to trace
    add_ormsby_to_trace(my_data->trace_data, times, 
                        my_data->ormsby_f1,
                        my_data->ormsby_f2,
                        my_data->ormsby_f3,
                        my_data->ormsby_f4,
                        my_data->sinterval, gate);

  }

  if (mod_conf.Has("gendata.signal.ricker")) {
    float peak_freq = mod_conf.GetFloat("gendata.signal.ricker.pfreq");
    if (mod_conf.HasError()) {
      gd_logger.LogError(my_logger, "Failed to get ricker pfreq. Error: {}", 
                         mod_conf.ErrorMessage());
      job_df.SetJobAborted();
      _clean_up();
      return;
    }

    float gate = mod_conf.GetFloat("gendata.signal.ricker.gate");
    if (mod_conf.HasError()) {
      gd_logger.LogError(my_logger, "Failed to get ricker gate. Error: {}", 
                         mod_conf.ErrorMessage());
      job_df.SetJobAborted();
      _clean_up();
      return;
    }
    if (gate > my_data->max_time) {
      gd_logger.LogError(my_logger, "ricker gate {} > max time {}. Error: {}", 
                         gate, my_data->max_time, mod_conf.ErrorMessage());
      job_df.SetJobAborted();
      _clean_up();
      return;
    }

    size_t num_times = mod_conf.GetSize("gendata.signal.ricker.times");
    if (num_times < 1l) {
      gd_logger.LogError(my_logger, "Failed to find any valid ricker times. Error: {}", 
                         mod_conf.ErrorMessage());
      job_df.SetJobAborted();
      _clean_up();
      return;
    }

    std::vector<float> times;
    times.resize(num_times);
    mod_conf.GetArrayFloat("gendata.signal.ricker.times", times.data());
    if (mod_conf.HasError()) {
      gd_logger.LogError(my_logger, "Failed to get ricker times. Error: {}", 
                         mod_conf.ErrorMessage());
      job_df.SetJobAborted();
      _clean_up();
      return;
    }

    // Dump Ricker parameters
    if (num_times > 1) 
      gd_logger.LogInfo(my_logger, "Add Ricker wavelets with parameters:");
    else
      gd_logger.LogInfo(my_logger, "Add Ricker wavelet with parameters:");
    gd_logger.LogInfo(my_logger, "peak_freq={} Hz", peak_freq);
    gd_logger.LogInfo(my_logger, "gate={} ms", gate);
    gd_logger.LogInfo(my_logger, "times (ms):");
    for (auto i = 0; i < num_times; ++i) {
      gd_logger.LogInfo(my_logger, "  {}", times[i]);
    }
    
    // add ricker wavelets to trace
    add_ricker_to_trace(my_data->trace_data, times, peak_freq, my_data->sinterval, gate);
  }

  gd_logger.FlushLog(my_logger);

  // print attributes information
  fort::char_table attr_table;
  attr_table.set_border_style(FT_NICE_STYLE);
  attr_table << fort::header
    << "ID" << "Name" << "Format" << "Length" << "Min" << "Max" << fort::endr
    << 1 << my_data->pkey_name << "Int" << 1 << my_data->fpkey << my_data->lpkey << fort::endr
    << 2 << my_data->skey_name << "Int" << 1 << my_data->fskey << my_data->lskey << fort::endr
    << 3 << my_data->trace_name << "Float" << my_data->trace_length << -1.0 << 1.0 << fort::endr;

  attr_table.column(3).set_cell_text_align(fort::text_align::right);
  attr_table.column(4).set_cell_text_align(fort::text_align::right);
  attr_table.column(5).set_cell_text_align(fort::text_align::right);

  std::cout << std::endl;
  std::cout << "Attribute information" << std::endl;
  std::cout << "=====================" << std::endl;
  std::cout << std::endl;
  std::cout << attr_table.to_string() << std::endl;
  std::cout << std::endl;


}

void gendata_process(const char* myid)
{
  auto& gd_logger = gdlog::GdLogger::GetInstance();
  auto& job_df = df::GeoDataFlow::GetInstance();

  Gendata* my_data = static_cast<Gendata*>(job_df.GetModuleStruct(myid));
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
  
  // have we reached the end of data
  if (my_data->pkinc > 0) {
    if (my_data->current_pkey > my_data->lpkey) {
      // set the flag
      job_df.SetJobFinished();
      return;
    }

  }
  else {
    if (my_data->current_pkey < my_data->lpkey) {
      // set the flag
      job_df.SetJobFinished();
      return;
    }
  }

  int* pkey = static_cast<int*>(job_df.GetWritableBuffer(job_df.GetPrimaryKeyName()));
  if(pkey == nullptr) {
    gd_logger.LogError(my_logger, "DF returned a nullptr to the buffer of pkey is NULL");
    job_df.SetJobAborted();
    _clean_up();
    return;
  }
  
  int* skey = static_cast<int*>(job_df.GetWritableBuffer(job_df.GetSecondaryKeyName()));
  if(skey == nullptr) {
    gd_logger.LogError(my_logger, "DF returned a nullptr to the buffer of skey is NULL");
    job_df.SetJobAborted();
    _clean_up();
    return;
  }
  int num_skey = my_data->num_skey;

  for (auto i = 0; i < num_skey; ++i) {
    pkey[i] = my_data->current_pkey;
    skey[i] = my_data->fskey + i * my_data->skinc;
  }
 

  gd_logger.LogInfo(my_logger, "Process primary key {}\n", pkey[0]);

  auto vol_data_name = job_df.GetVolumeDataName();
  float* trc_buf = static_cast<float*>(job_df.GetWritableBuffer(vol_data_name));
  if(trc_buf == nullptr) {
    gd_logger.LogError(my_logger, "DF returned a nullptr to the buffer of {}", vol_data_name);
    job_df.SetJobAborted();
    _clean_up();
    return;
  }

  size_t num_skeys = job_df.GetNumberSKeys();
  size_t trc_length = job_df.GetDataVectorLength();
  for (int i=0; i<num_skeys; i++) {
    float* trc = trc_buf + i * trc_length;
    std::copy(my_data->trace_data.begin(), my_data->trace_data.end(), trc);
  }
  
  // prepare for the next call
  my_data->current_pkey = my_data->current_pkey + my_data->pkinc;
}
