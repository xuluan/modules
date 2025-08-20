#include "input.h"
#include "ArrowStore.h"
#include "GeoDataFlow.h"
#include <ModuleConfig.h>
#include <GdLogger.h>
#include <string>
#include <VdsStore.h>
#include <filesystem>
#include "fort.hpp"

void input_init(const char* myid, const char* buf)
{
  std::string logger_name = std::string {"input_"} + myid;
  auto& gd_logger = gdlog::GdLogger::GetInstance();
  void* my_logger = gd_logger.Init(logger_name);
  gd_logger.LogInfo(my_logger, "input_init");

  // Need to pass the pointer to DF after init function returns
  // So we use raw pointer instead of a smart pointer here
  Input* my_data = new Input {};

  my_data->logger = my_logger;

  auto& job_df = df::GeoDataFlow::GetInstance();

  // A handy function to clean up resources if errors happen
  auto _clean_up = [&] ()-> void {
    if (my_data != nullptr) {
      delete my_data;
    }
  };

  // parse job parameters
  mc::ModuleConfig mod_conf {};  
  mod_conf.Parse(buf);
  if(mod_conf.HasError()) {
    gd_logger.LogError(my_logger, "Failed to parse the job setup. Error: {}", mod_conf.ErrorMessage().c_str());
    job_df.SetJobAborted();
    _clean_up();
    return;
  }

  mod_conf.GetText("input.url", my_data->data_url);
  if(mod_conf.HasError()) {
    gd_logger.LogError(my_logger, "Failed to get input data url. Error: {}", mod_conf.ErrorMessage().c_str());
    job_df.SetJobAborted();
    _clean_up();
    return;
  }

  if (my_data->data_url.empty()) {
    gd_logger.LogError(my_logger, "The input data url should not be empty");
    job_df.SetJobAborted();
    _clean_up();
    return;
  }

  if (!std::filesystem::exists(my_data->data_url)) {
    gd_logger.LogError(my_logger, "The input data file {} does not exist", my_data->data_url);
    job_df.SetJobAborted();
    _clean_up();
    return;
  }

  // read input dataset
  ovds::VdsStore* pvs = new ovds::VdsStore(my_data->data_url);
  if (pvs->HasError()) {
    gd_logger.LogError(my_logger, "Failed to open the input dataset {}. Error: {}", 
                       my_data->data_url, pvs->ErrorMessage());
    job_df.SetJobAborted();
    _clean_up();
    return;
  }
  pvs->ReadAxesInfo();
  if (pvs->HasError()) {
    gd_logger.LogError(my_logger, "Failed to read dimension information of the input dataset. Error: {}", 
                       pvs->ErrorMessage());
    job_df.SetJobAborted();
    _clean_up();
    return;
  }
  
  int num_axes = pvs->GetNumberDimensions();
  if (num_axes < 3 || num_axes > 6) {
    gd_logger.LogError(my_logger, "Invalid number of dimensions. num_axes: {}", num_axes); 
    job_df.SetJobAborted();
    _clean_up();
    return;
    
  }

  // set up key dimensions
  std::string slice_pos;
  mod_conf.GetText("input.sliceposition", slice_pos);
  if(mod_conf.HasError()) {
    gd_logger.LogError(my_logger, "Failed to get sliceposition. Error: {}", mod_conf.ErrorMessage().c_str());
    gd_logger.LogWarning(my_logger, "In this case, get slices on the primary key");
  }

  if (slice_pos == "on_data_samples") {
    my_data->pkey_dim = 0;
    gd_logger.LogInfo(my_logger, "Reading slices on data samples (ie, timeslices)");
  }
  else if (slice_pos == "on_secondary_key") {
    my_data->pkey_dim = 1;
    gd_logger.LogInfo(my_logger, "Reading slices on secondary key");
  }
  else {
    my_data->pkey_dim = 2;
    gd_logger.LogInfo(my_logger, "Reading slices on primary key");
  }

  int pkey_dim = 2;
  int skey_dim = 1;
  int data_dim = 0;
  if (my_data->pkey_dim == 1) {
    pkey_dim = 1;
    skey_dim = 2;
    data_dim = 0;
  }
  
  if (my_data->pkey_dim == 0) {
    pkey_dim = 0;
    skey_dim = 2;
    data_dim = 1;
  }

  // primary key
  std::string pkey_name{}, pkey_unit{};
  int num_pkeys;
  float pkey_min, pkey_max;
  pvs->GetAxisInfo(pkey_dim, pkey_name, pkey_unit, num_pkeys, 
                   pkey_min,
                   pkey_max);
  job_df.AddAttribute(pkey_name.c_str(), as::DataFormat::FORMAT_U32, 1);
  job_df.SetAttributeUnit(pkey_name.c_str(), pkey_unit.c_str());
  job_df.SetPrimaryKeyName(pkey_name.c_str());
  // Set up primary key axis
  job_df.SetPrimaryKeyAxis(static_cast<int>(pkey_min), static_cast<int>(pkey_max), num_pkeys);
  int pkey_inc = static_cast<int>((pkey_max - pkey_min) / (num_pkeys - 1.0f) + 0.5f);
  gd_logger.LogDebug(my_logger, "pkey_inc: {}, using round: {}", 
                    pkey_inc,
                    round((pkey_max - pkey_min) / (num_pkeys - 1.f)));
  my_data->current_pkey_index = 0;
  my_data->pkeys.clear();
  for (auto i = 0; i < num_pkeys; ++i) {
    my_data->pkeys.push_back(pkey_min + pkey_inc*i + 0.5f);
  }

  
  // secondary key
  std::string skey_name{}, skey_unit{};
  int num_skeys;
  float skey_min, skey_max;
  pvs->GetAxisInfo(skey_dim, skey_name, skey_unit, num_skeys, 
                   skey_min,
                   skey_max);
  job_df.AddAttribute(skey_name.c_str(), as::DataFormat::FORMAT_U32, 1);
  job_df.SetAttributeUnit(skey_name.c_str(), skey_unit.c_str());
  job_df.SetSecondaryKeyName(skey_name.c_str());
  // set up secondary key axis
  job_df.SetSecondaryKeyAxis(static_cast<int>(skey_min), static_cast<int>(skey_max), num_skeys);
  int skey_inc = static_cast<int>((skey_max - skey_min) / (num_skeys - 1.0f) + 0.5f);
  gd_logger.LogDebug(my_logger, "skey_inc: {}, using round: {}", 
                    skey_inc,
                    round((skey_max - skey_min) / (num_skeys - 1.f)));
  my_data->skeys.clear();
  for (auto i = 0; i < num_pkeys; ++i) {
    my_data->skeys.push_back(skey_min + skey_inc * i + 0.5f);
  }

  std::map<std::string, std::string> fmt_string;
  std::map<std::string, int> attr_length;


  // The other attributes
  int first_attr = 0;
  if (my_data->pkey_dim == 0) {
    // if reading time slices
    first_attr = 1;
  }

  for (auto i = first_attr; i < pvs->GetNumberAttributes(); ++i) {
    std::string attr_name = pvs->GetAttributeName(i);
    int length;
    ovds::DataFormat attr_format;
    ovds::AttributeType attr_type;
    pvs->GetAttributeInfo(attr_name, attr_format, length, attr_type);
    attr_length.emplace(std::make_pair(attr_name, length));
    switch (attr_format) {
      case ovds::DataFormat::FORMAT_U8:
        job_df.AddAttribute(attr_name.c_str(), as::DataFormat::FORMAT_U8, length);
        fmt_string.emplace(std::make_pair(attr_name, "Int8"));
        break;
      case ovds::DataFormat::FORMAT_U16:
        job_df.AddAttribute(attr_name.c_str(), as::DataFormat::FORMAT_U16, length);
        fmt_string.emplace(std::make_pair(attr_name, "Int16"));
        break;
      case ovds::DataFormat::FORMAT_U32:
        job_df.AddAttribute(attr_name.c_str(), as::DataFormat::FORMAT_U32, length);
        fmt_string.emplace(std::make_pair(attr_name, "Int32"));
        break;
      case ovds::DataFormat::FORMAT_U64:
        job_df.AddAttribute(attr_name.c_str(), as::DataFormat::FORMAT_U64, length);
        fmt_string.emplace(std::make_pair(attr_name, "Int64"));
        break;
      case ovds::DataFormat::FORMAT_R32:
        job_df.AddAttribute(attr_name.c_str(), as::DataFormat::FORMAT_R32, length);
        fmt_string.emplace(std::make_pair(attr_name, "Float"));
        break;
      case ovds::DataFormat::FORMAT_R64:
        job_df.AddAttribute(attr_name.c_str(), as::DataFormat::FORMAT_R64, length);
        fmt_string.emplace(std::make_pair(attr_name, "Double"));
        break;
      default:
        gd_logger.LogWarning(my_logger, "Unknown data type of attribute {}. Skip it", attr_name);
    }

    std::string attr_unit = pvs->GetAttributeUnit(i);
    job_df.SetAttributeUnit(attr_name.c_str(), attr_unit.c_str());
    
    float val_min, val_max;
    pvs->GetAttributeValueRange(attr_name, val_min, val_max);
    job_df.SetAttributeValueRange(attr_name.c_str(), val_min, val_max);
  }
  
  // set up data axis
  std::string trace_name{}, trace_unit{};
  int trace_length;
  float time_min, time_max;
  pvs->GetAxisInfo(data_dim, trace_name, trace_unit, trace_length, 
                   time_min,
                   time_max);
  float trc_val_min, trc_val_max;
  pvs->GetAttributeValueRange(trace_name.c_str(), trc_val_min, trc_val_max);

  job_df.SetVolumeDataName(trace_name.c_str());
  job_df.SetDataAxisUnit(trace_unit.c_str());
  
  fmt_string[pkey_name] = "Int32";
  attr_length[pkey_name] = 1;
  fmt_string[skey_name] = "Int32";
  attr_length[skey_name] = 1;
  fmt_string[trace_name] = "Float";
  attr_length[trace_name] = trace_length;

  // Set up data axis
  job_df.SetDataAxis(time_min, time_max, trace_length);
  
  // set the group size, to allocate buffers
  job_df.SetGroupSize(num_skeys);

  my_data->vsid = pvs;

  job_df.SetModuleStruct(myid, static_cast<void*>(my_data));
  
  gd_logger.FlushLog(my_logger);

  // print attributes information
  fort::char_table attr_table;
  attr_table.set_border_style(FT_NICE_STYLE);
  attr_table << fort::header
    << "ID" << "Name" << "Format" << "Length" << "Min" << "Max" << fort::endr
    << 1 << pkey_name << fmt_string[pkey_name] << 1 << pkey_min << pkey_max << fort::endr
    << 2 << skey_name << fmt_string[skey_name] << 1 << skey_min << skey_max << fort::endr
    << 3 << trace_name << fmt_string[trace_name] << trace_length << trc_val_min << trc_val_max << fort::endr;
  for (auto i = 1; i < pvs->GetNumberAttributes(); ++i) {
    std::string attr_name = pvs->GetAttributeName(i);
    float val_min, val_max;
    pvs->GetAttributeValueRange(attr_name, val_min, val_max);
    attr_table << 3 + i 
      << attr_name << fmt_string[attr_name] << attr_length[attr_name] << val_min << val_max << fort::endr;
  }

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

void input_process(const char* myid)
{
  auto& gd_logger = gdlog::GdLogger::GetInstance();
  auto& job_df = df::GeoDataFlow::GetInstance();

  Input* my_data = static_cast<Input*>(job_df.GetModuleStruct(myid));
  void* my_logger = my_data->logger;
  ovds::VdsStore* pvs = my_data->vsid;

  // A handy function to clean up resources if errors happen, 
  // or job has finished
  auto _clean_up = [&] ()-> void {
    if (my_data != nullptr) {
      delete my_data;
    }
  };

  if (job_df.JobFinished()) {
    pvs->Close();
    _clean_up();
    return;
  }
  
  // have we reached the end of data
  if (my_data->current_pkey_index >= my_data->pkeys.size()) {
    // set the flag
    job_df.SetJobFinished();
    return;
  }

  int grp_size = job_df.GetGroupSize();

  int* pkey;
  pkey = static_cast<int*>(job_df.GetWritableBuffer(job_df.GetPrimaryKeyName()));
  if(pkey == nullptr) {
    gd_logger.LogError(my_logger, "DF returned a nullptr to the buffer of pkey is NULL");
    job_df.SetJobAborted();
    _clean_up();
    return;
  }

  std::fill(pkey, pkey + grp_size, my_data->pkeys[my_data->current_pkey_index]);
  gd_logger.LogInfo(my_logger, "Process primary key {}\n", pkey[0]);


  int* skey;
  skey = static_cast<int*>(job_df.GetWritableBuffer(job_df.GetSecondaryKeyName()));
  if(skey == nullptr) {
    gd_logger.LogError(my_logger, "DF returned a nullptr to the buffer of skey is NULL");
    job_df.SetJobAborted();
    _clean_up();
    return;
  }

  std::copy(my_data->skeys.begin(), 
            my_data->skeys.end(), 
            skey);

  for (auto i = 0; i < pvs->GetNumberAttributes(); ++i) {
    std::string attr_name = pvs->GetAttributeName(i);
    int channel_id = pvs->GetAttributeChannelId(attr_name);
    void* buf = job_df.GetWritableBuffer(attr_name.c_str());
    if(buf == nullptr) {
      gd_logger.LogError(my_logger, "DF returned a nullptr to the buffer of {}", attr_name);
      job_df.SetJobAborted();
      _clean_up();
      return;
    }
    
    int buf_bytesize = grp_size * job_df.GetAttributeByteSize(attr_name.c_str());
    gd_logger.LogDebug(my_logger, "attribute: {}, buf_bytesize: {}, channel_id: {}",
                      attr_name, buf_bytesize, channel_id);

    pvs->ReadAttributeSlice(buf, buf_bytesize, channel_id, 
                            my_data->pkey_dim, my_data->current_pkey_index);
  }

  // prepare for the next call
  my_data->current_pkey_index++;
}
