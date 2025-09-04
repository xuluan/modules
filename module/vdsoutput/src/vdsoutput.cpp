#include "vdsoutput.h"
#include "ArrowStore.h"
#include "GeoDataFlow.h"
#include <ModuleConfig.h>
#include <GdLogger.h>
#include <string>
#include <VdsStore.h>
#include <filesystem>
#include "fort.hpp"
#include <utl_yaml_parser.h>
#include <utl_string.h>

OpenVDS::VolumeDataFormat convert_dataformat_to_vds(as::DataFormat format)
{
    switch (format) {
        case as::DataFormat::FORMAT_U8:
            return OpenVDS::VolumeDataFormat::Format_U8;
        case as::DataFormat::FORMAT_U16:
            return OpenVDS::VolumeDataFormat::Format_U16;
        case as::DataFormat::FORMAT_R32:
            return OpenVDS::VolumeDataFormat::Format_R32;
        case as::DataFormat::FORMAT_U32:
            return OpenVDS::VolumeDataFormat::Format_U32;
        case as::DataFormat::FORMAT_R64:
            return OpenVDS::VolumeDataFormat::Format_R64;
        case as::DataFormat::FORMAT_U64:
            return OpenVDS::VolumeDataFormat::Format_U64;
        default:
            throw std::runtime_error("Unsupport as::DataFormat type:  " + std::to_string(static_cast<int>(format)));  
    }
}


void vdsoutput_init(const char* myid, const char* buf)
{
    std::string logger_name = std::string {"vdsoutput_"} + myid;
    auto& gd_logger = gdlog::GdLogger::GetInstance();
    void* my_logger = gd_logger.Init(logger_name);
    gd_logger.LogInfo(my_logger, "vdsoutput_init");

    // Need to pass the pointer to DF after init function returns
    // So we use raw pointer instead of a smart pointer here
    Vdsoutput* my_data = new Vdsoutput {};

    my_data->logger = my_logger;

    auto& job_df = df::GeoDataFlow::GetInstance();

    // A handy function to clean up resources if errors happen
    auto _clean_up = [&] ()-> void {
        if (my_data != nullptr) {
            try {
                my_data->m_vds_writer->finalize();
            } catch (const std::exception& e) {
                    gd_logger.LogError(my_logger, "VDS writer finalize failed!");
                }                
            delete my_data;
        }
    };

    try {
        // parse job parameters
        gutl::DynamicValue config = gutl::parse(buf);
        auto& vdsout_config = config["vdsoutput"];

        my_data->is_success = true;

        //parse url
        my_data->url = vdsout_config.at("url", "vdsoutput").as_string();
        if (my_data->url.empty()) {
            throw std::runtime_error("VDS output URL is empty");
        }
        gd_logger.LogInfo(my_logger, "vdsoutput url: {}", my_data->url);

        // Check if parent directory exists
        std::filesystem::path output_path(my_data->url);
        std::filesystem::path parent_dir = output_path.parent_path();
        if (!parent_dir.empty() && !std::filesystem::exists(parent_dir)) {
            throw std::runtime_error("VDS output parent directory does not exist:" + parent_dir.string());
        }   
        
        //parse config
        try {
            my_data->brick_size = vdsout_config.at("brick_size", "vdsoutput").as_int();
        } catch (const std::exception& e) {
            my_data->brick_size = 64;
        }

        try {
            my_data->lod_levels = vdsout_config.at("lod_levels", "vdsoutput").as_int();
        } catch (const std::exception& e) {
            my_data->lod_levels = 0;
        }

        try {
            std::string compression = vdsout_config.at("compression", "vdsoutput").as_string();
            if(compression == "none") {

            } else if(compression == "zip") {
                my_data->compression_method = OpenVDS::CompressionMethod::Zip;
            } else if(compression == "wavelet") {
                my_data->compression_method = OpenVDS::CompressionMethod::Wavelet;
            } else {
                my_data->compression_method = OpenVDS::CompressionMethod::None;
            }
        } catch (const std::exception& e) {
            my_data->compression_method = OpenVDS::CompressionMethod::None;
        }

        try {
            my_data->tolerance = vdsout_config.at("tolerance", "vdsoutput").as_float();
        } catch (const std::exception& e) {
            my_data->tolerance = 0.01;
        }
        
        // Get data flow information to configure VDS writer
        my_data->pkey_name = job_df.GetPrimaryKeyName();
        my_data->skey_name = job_df.GetSecondaryKeyName();
        my_data->trace_name = job_df.GetVolumeDataName();
        
        gd_logger.LogInfo(my_logger, "Primary key: {}, Secondary key: {}, Trace data: {}", 
                            my_data->pkey_name, my_data->skey_name, my_data->trace_name);

        // Get axis information from data flow
        job_df.GetPrimaryKeyAxis(my_data->fpkey, my_data->lpkey, my_data->num_pkey);
        job_df.GetSecondaryKeyAxis(my_data->fskey, my_data->lskey, my_data->num_skey);
        job_df.GetDataAxis(my_data->tmin, my_data->tmax, my_data->trace_length);
        
        // Calculate increments
        my_data->pkinc = (my_data->num_pkey > 1) ? (my_data->lpkey - my_data->fpkey) / (my_data->num_pkey - 1) : 1;
        my_data->skinc = (my_data->num_skey > 1) ? (my_data->lskey - my_data->fskey) / (my_data->num_skey - 1) : 1;
        
        // Get sample interval from data flow
        my_data->sinterval = (my_data->tmax - my_data->tmin) * 1000 /(my_data->trace_length - 1);
        my_data->current_pkey_index = 0;
        my_data->batch_start = 0;
        my_data->batch_end = 0;
        my_data->batch_num = 0;

        gd_logger.LogInfo(my_logger, "Primary axis: {} to {} ({} values, inc={})", 
                            my_data->fpkey, my_data->lpkey, my_data->num_pkey, my_data->pkinc);
        gd_logger.LogInfo(my_logger, "Secondary axis: {} to {} ({} values, inc={})", 
                            my_data->fskey, my_data->lskey, my_data->num_skey, my_data->skinc);
        gd_logger.LogInfo(my_logger, "Data axis: {} to {} ({} samples, interval={}μs)", 
                            my_data->tmin, my_data->tmax, my_data->trace_length, my_data->sinterval);


        // Determine data format from trace attribute
        as::DataFormat trace_format;
        int trace_attr_length;
        float min_val, max_val;
        job_df.GetAttributeInfo(my_data->trace_name.c_str(), trace_format, trace_attr_length, min_val, max_val);
        trace_format = as::DataFormat::FORMAT_R32;//todo

        my_data->m_vds_writer = std::make_unique<VDSWriter>(my_data->url, my_data->brick_size, my_data->lod_levels,
            my_data->compression_method, my_data->tolerance, convert_dataformat_to_vds(trace_format));


        my_data->m_vds_writer->SetPrimaryKeyAxis(my_data->fpkey, my_data->lpkey, my_data->num_pkey);
        my_data->m_vds_writer->SetSecondaryKeyAxis(my_data->fskey, my_data->lskey, my_data->num_skey);
        my_data->m_vds_writer->SetDataAxis(my_data->tmin, my_data->tmax, my_data->trace_length);

        auto& attrs = config["vdsoutput"]["attributes"];
        if(attrs.is_array()) {
            auto& arr = config["vdsoutput"]["attributes"].as_array();
            for (size_t i = 0; i < arr.size(); ++i) {
                
                auto& attr = arr[i];
                std::string name = attr.at("name", "attribute").as_string();
                gutl::UTL_StringToUpperCase(name);
                if(name == my_data->pkey_name || name == my_data->skey_name || name == my_data->trace_name){
                    continue;
                }

                bool is_found = false;

                for(int i = 0; i < job_df.GetNumAttributes(); i++) {

                    if(name == job_df.GetAttributeName(i)){
                        is_found = true;
                        break;
                    }
                }

                if(is_found) {
                    AttributeFieldInfo field;
                    as::DataFormat format;
                    field.name = name;
                    job_df.GetAttributeInfo(name.c_str(), format, field.width, min_val, max_val);
                    field.format = convert_dataformat_to_vds(format);
                    field.width *= getVDSDataSize(field.format);
                    my_data->attributes.insert(std::make_pair(name, field));
                    my_data->m_vds_writer->addAttributeField(name, field.width, field.format);
                    gd_logger.LogInfo(my_logger, "Add Channel: {} ", name);

                }
            }           

        } else { // no attributes in config, add all attributes

            for(int i = 0; i < job_df.GetNumAttributes(); i++) {

                std::string name = job_df.GetAttributeName(i);

                if(name == my_data->pkey_name || name == my_data->skey_name || name == my_data->trace_name){
                    continue;
                }
                AttributeFieldInfo field;
                as::DataFormat format;
                field.name = name;
                job_df.GetAttributeInfo(name.c_str(), format, field.width, min_val, max_val);
                field.format = convert_dataformat_to_vds(format);
                field.width *= getVDSDataSize(field.format);
                my_data->attributes.insert(std::make_pair(name, field));
                my_data->m_vds_writer->addAttributeField(name, field.width, field.format);   
                gd_logger.LogInfo(my_logger, "Add Channel: {} ", name);

            }

        }

        //create vds
        if(!my_data->m_vds_writer->createVdsStore()) {
            throw std::runtime_error("Failed to create VDS store");
        }

        //Setup sliding windows for all data types
        if (!my_data->m_vds_writer->setupSlidingWindows()) {
            throw std::runtime_error("Failed to setup sliding windows");
        }

        //Initialize chunk writers
        if (!my_data->m_vds_writer->initializeChunkWriters()) {
            throw std::runtime_error("Failed to initialize chunk writers");
        }


        gd_logger.LogInfo(my_logger, "VDS writer initialized successfully");            
        job_df.SetModuleStruct(myid, static_cast<void*>(my_data));

    } catch (const std::exception& e) {
        my_data->is_success = false;
        gd_logger.LogError(my_logger, e.what());
        job_df.SetJobAborted();
        _clean_up();
        return;
    }
}

void vdsoutput_process(const char* myid)
{
    auto& gd_logger = gdlog::GdLogger::GetInstance();
    auto& job_df = df::GeoDataFlow::GetInstance();

    Vdsoutput* my_data = static_cast<Vdsoutput*>(job_df.GetModuleStruct(myid));
    void* my_logger = my_data->logger;

    // A handy function to clean up resources if errors happen, 
    // or job has finished
    auto _clean_up = [&] ()-> void {
        if (my_data != nullptr) {
            delete my_data;
        }
    };

    if (job_df.JobFinished()) {
        try {
            my_data->is_success = my_data->m_vds_writer->finalize() && my_data->is_success;
        } catch (const std::exception& e) {
                gd_logger.LogError(my_logger, "Error: VDS writer finalize failed!");
                my_data->is_success = false;
        } 

        if (my_data->is_success == false) {
            gd_logger.LogError(my_logger, "VDS output failed!");
        } else {
            gd_logger.LogInfo(my_logger, "Output VDS dataset: {}", my_data->url);
        }
        _clean_up();
        return;
    }

    try {        
        int grp_size = job_df.GetGroupSize();

        std::string data_name = job_df.GetVolumeDataName();
        my_data->batch_end++;
        my_data->batch_num++;
        //write data: primary, secondary, trace and attributes
        for(int i = 0; i < job_df.GetNumAttributes(); i++) {

            std::string attr_name = job_df.GetAttributeName(i);
            char *data = static_cast<char*>(job_df.GetWritableBuffer(attr_name.c_str()));
            if(data == nullptr) {
                throw std::runtime_error("DF returned a nullptr to the buffer of attribute: " + attr_name);
            }
            if(attr_name == my_data->pkey_name || attr_name == my_data->skey_name) {
                continue;
            }

            if(attr_name == my_data->trace_name ){
                attr_name = "Amplitude";

            }

            if(!my_data->m_vds_writer->fill(attr_name, data)) {
                throw std::runtime_error("Failed to fill sliding window for channel: "+ attr_name + " at primary index : " + std::to_string(my_data->current_pkey_index));

            }
            if(my_data->batch_num == my_data->brick_size*2 || my_data->batch_end == my_data->num_pkey){
                if(!my_data->m_vds_writer->processBatch(attr_name, my_data->batch_start, my_data->batch_end)) {
                    throw std::runtime_error("Failed to process batch for channel: "+ attr_name + " at primary index : " + std::to_string(my_data->current_pkey_index));
                }
            }
            if(my_data->batch_num == my_data->brick_size*2){

                if(!my_data->m_vds_writer->slide(attr_name)) {
                    throw std::runtime_error("Failed to slide window for channel: "+ attr_name + " at primary index : " + std::to_string(my_data->current_pkey_index));

                }
            }
        }

        //file.close();      
    } catch (const std::exception& e) {
        my_data->is_success = false;
        gd_logger.LogError(my_logger, "Exception in vdsoutput_process: {}", e.what());
        job_df.SetJobAborted();
        _clean_up();
        return;
    }

    if(my_data->batch_num == my_data->brick_size*2) {
        my_data->batch_start += my_data->brick_size;
        my_data->batch_num -= my_data->brick_size;
    }
    // prepare for the next call
    my_data->current_pkey_index++;
}