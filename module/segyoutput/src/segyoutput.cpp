#include "segyoutput.h"
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

bool check_offset(int offset, int width, int header_size)
{
    if(offset > 0 && (offset + width - 1) <= header_size) {
        return true;
    }
    return false;
}

void segyoutput_init(const char* myid, const char* buf)
{
    std::string logger_name = std::string {"segyoutput_"} + myid;
    auto& gd_logger = gdlog::GdLogger::GetInstance();
    void* my_logger = gd_logger.Init(logger_name);
    gd_logger.LogInfo(my_logger, "segyoutput_init");

    // Need to pass the pointer to DF after init function returns
    // So we use raw pointer instead of a smart pointer here
    Segyoutput* my_data = new Segyoutput {};

    my_data->logger = my_logger;

    auto& job_df = df::GeoDataFlow::GetInstance();

    // A handy function to clean up resources if errors happen
    auto _clean_up = [&] ()-> void {
        if (my_data != nullptr) {
            delete my_data;
        }
    };

    try {
        // parse job parameters
        gutl::DynamicValue config = gutl::parse(buf);

        my_data->is_success = true;

        auto& segyout_config = config["segyoutput"];

        //parse url
        my_data->url = segyout_config.at("url", "segyoutput").as_string();
        if (my_data->url.empty()) {
            throw std::runtime_error("Error: segyoutput url is empty");
        }
        gd_logger.LogInfo(my_logger, "segyoutput url: {}", my_data->url);

        // Check if parent directory exists
        std::filesystem::path output_path(my_data->url);
        std::filesystem::path parent_dir = output_path.parent_path();
        if (!parent_dir.empty() && !std::filesystem::exists(parent_dir)) {
            throw std::runtime_error("Error: segyoutput parent directory does not exist: " + parent_dir.string());
        }            
        // Get data flow information to configure SEGY writer
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
        my_data->current_pkey = my_data->fpkey;

        gd_logger.LogInfo(my_logger, "Primary axis: {} to {} ({} values, inc={})", 
                            my_data->fpkey, my_data->lpkey, my_data->num_pkey, my_data->pkinc);
        gd_logger.LogInfo(my_logger, "Secondary axis: {} to {} ({} values, inc={})", 
                            my_data->fskey, my_data->lskey, my_data->num_skey, my_data->skinc);
        gd_logger.LogInfo(my_logger, "Data axis: {} to {} ({} samples, interval={}μs)", 
                            my_data->tmin, my_data->tmax, my_data->trace_length, my_data->sinterval);

        // Parse header field offsets from configuration
        try {
            my_data->primary_offset = segyout_config.at("primary_offset", "segyoutput").as_int();
        } catch (const std::exception& e) {
            my_data->primary_offset = 189;
        }

        try {
            my_data->secondary_offset = segyout_config.at("secondary_offset", "segyoutput").as_int();
        } catch (const std::exception& e) {
            my_data->secondary_offset = 193;
        }

        try {
            my_data->sinterval_offset = segyout_config.at("sinterval_offset", "segyoutput").as_int();
        } catch (const std::exception& e) {
            my_data->sinterval_offset = 17;
        }

        try {
            my_data->trace_length_offset = segyout_config.at("trace_length_offset", "segyoutput").as_int();
        } catch (const std::exception& e) {
                my_data->trace_length_offset = 21;
        }

        try {
            my_data->data_format_code_offset = segyout_config.at("data_format_code_offset", "segyoutput").as_int();
        } catch (const std::exception& e) {
            my_data->data_format_code_offset = 25;
        }

        // Determine data format from trace attribute
        as::DataFormat trace_format;
        int trace_attr_length;
        float min_val, max_val;
        job_df.GetAttributeInfo(my_data->trace_name.c_str(), trace_format, trace_attr_length, min_val, max_val);
        
        SEGY::DataSampleFormatCode segy_format;
        switch (trace_format) {
            case as::DataFormat::FORMAT_U8:
                segy_format = SEGY::DataSampleFormatCode::Int8;
                break;
            case as::DataFormat::FORMAT_U16:
                segy_format = SEGY::DataSampleFormatCode::Int16;
                break;
            case as::DataFormat::FORMAT_U32:
                segy_format = SEGY::DataSampleFormatCode::Int32;
                break;
            case as::DataFormat::FORMAT_R32:
                segy_format = SEGY::DataSampleFormatCode::IEEEFloat;
                break;
            default:
                throw std::runtime_error("Error: unsupported data format for SEGY output");
        }
        SEGYWriteInfo write_info;

        // Configure SEGY write info
        write_info.headerEndianness = SEGY::Endianness::BigEndian; // Standard SEGY big-endian
        write_info.dataSampleFormatCode = segy_format;
        write_info.sampleCount = my_data->trace_length;
        write_info.sampleInterval = my_data->sinterval;
        
        try {
            write_info.textualHeader = segyout_config.at("textual_header", "segyoutput").as_string();
        } catch (const std::exception& e) {
            write_info.textualHeader = "";
        }

        // Calculate trace byte size based on format
        int sample_size = 0;
        switch (segy_format) {
            case SEGY::DataSampleFormatCode::Int8:
                sample_size = 1;
                break;
            case SEGY::DataSampleFormatCode::Int16:
                sample_size = 2;
                break;
            case SEGY::DataSampleFormatCode::Int32:
            case SEGY::DataSampleFormatCode::IEEEFloat:
                sample_size = 4;
                break;
            default:
                throw std::runtime_error("Error: unsupported SEGY data format");
        }
        write_info.traceByteSize = my_data->trace_length * sample_size;

        // Set coordinate system
        write_info.minInline = my_data->fpkey;
        write_info.maxInline = my_data->lpkey;
        write_info.inlineCount = my_data->num_pkey;
        write_info.minCrossline = my_data->fskey;
        write_info.maxCrossline = my_data->lskey;
        write_info.crosslineCount = my_data->num_skey;
        write_info.primaryStep = my_data->pkinc;
        write_info.secondaryStep = my_data->skinc;

        if(!check_offset(my_data->primary_offset, 4, 240)) {
            throw std::runtime_error("Error: segyoutput the offset of attribute " + my_data->pkey_name + " is invalid: " + std::to_string(my_data->primary_offset));
        }

        if(!check_offset(my_data->secondary_offset, 4, 240)) {
            throw std::runtime_error("Error: segyoutput the offset of attribute " + my_data->skey_name + " is invalid: " + std::to_string(my_data->secondary_offset));
        }


        if(!check_offset(my_data->trace_length_offset, 2, 400)) {
            throw std::runtime_error("Error: segyoutput the offset of NumSamples is invalid: " + std::to_string(my_data->trace_length_offset));
        }


        if(!check_offset(my_data->data_format_code_offset, 2, 400)) {
            throw std::runtime_error("Error: segyoutput the offset of DataFormatCode is invalid: " + std::to_string(my_data->data_format_code_offset));
        }
        

        if(!check_offset(my_data->sinterval_offset, 2, 400)) {
            throw std::runtime_error("Error: segyoutput the offset of SampleInterval is invalid: " + std::to_string(my_data->sinterval_offset));
        }

        my_data->segy_writer.addTraceField(my_data->pkey_name, my_data->primary_offset, 4, SEGY::DataSampleFormatCode::Int32);
        my_data->segy_writer.addTraceField(my_data->skey_name, my_data->secondary_offset, 4, SEGY::DataSampleFormatCode::Int32);

        my_data->segy_writer.addBinaryField("NumSamples", my_data->trace_length_offset, 2, SEGY::DataSampleFormatCode::Int16);
        my_data->segy_writer.addBinaryField("SampleInterval", my_data->sinterval_offset, 2, SEGY::DataSampleFormatCode::Int16);
        my_data->segy_writer.addBinaryField("DataFormatCode", my_data->data_format_code_offset, 2, SEGY::DataSampleFormatCode::Int16);

        // add sttributes
        auto& attrs = config["segyoutput"]["attribute"];
        if(attrs.is_array()) {
            //parse attributes
            auto& arr = config["segyoutput"]["attribute"].as_array();
            for (size_t i = 0; i < arr.size(); ++i) {
                auto& attr = arr[i];
                std::string name = attr.at("name", "attribute").as_string();
                gutl::UTL_StringToUpperCase(name);

                if(name == my_data->pkey_name || name == my_data->skey_name || name == my_data->trace_name) {
                    //todo log warning
                    continue;
                }

                std::string datatype = attr.at("datatype", "attribute").as_string();
                int offset = attr.at("offset", "attribute").as_int();
                int width;
                SEGY::DataSampleFormatCode format;
                as::DataFormat type;

                if (datatype == "int8") {
                    format = SEGY::DataSampleFormatCode::Int8;
                    type = as::DataFormat::FORMAT_U8;
                    width = 1;
                } else if (datatype == "int16") {
                    format = SEGY::DataSampleFormatCode::Int16;
                    type = as::DataFormat::FORMAT_U16;
                    width = 2;
                } else if (datatype == "int32") {
                    format = SEGY::DataSampleFormatCode::Int32;
                    type = as::DataFormat::FORMAT_U32;
                    width = 4;
                } else if (datatype == "float") {
                    format = SEGY::DataSampleFormatCode::IEEEFloat;
                    type = as::DataFormat::FORMAT_R32;
                    width = 4;
                } else {
                    throw std::runtime_error("Error: segyoutput the datatype of attribute " + name + " is invalid: " + datatype);
                }
                if(!check_offset(offset, width, 240)) {
                    throw std::runtime_error("Error: segyoutput the offset of attribute " + name + " is invalid: " + std::to_string(offset));
                }                
                my_data->segy_writer.addTraceField(name, offset, width, format);
                job_df.AddAttribute(name.c_str(), type, 1);
                job_df.SetAttributeUnit(name.c_str(), "");
            }
        }   

        // Initialize the writer and create output file
        if (!my_data->segy_writer.initialize(my_data->url, write_info)) {
            throw std::runtime_error("Error: failed to initialize SEGY writer for file: " + my_data->url + 
                                    ", Error: " + my_data->segy_writer.getLastError());
        }
        
        gd_logger.LogInfo(my_logger, "SEGY writer initialized successfully");            
        job_df.SetModuleStruct(myid, static_cast<void*>(my_data));

    } catch (const std::exception& e) {
        my_data->is_success = false;
        gd_logger.LogError(my_logger, e.what());
        job_df.SetJobAborted();
        _clean_up();
        return;
    }
}

void segyoutput_process(const char* myid)
{
    auto& gd_logger = gdlog::GdLogger::GetInstance();
    auto& job_df = df::GeoDataFlow::GetInstance();

    Segyoutput* my_data = static_cast<Segyoutput*>(job_df.GetModuleStruct(myid));
    void* my_logger = my_data->logger;

    // A handy function to clean up resources if errors happen, 
    // or job has finished
    auto _clean_up = [&] ()-> void {
        if (my_data != nullptr) {
            delete my_data;
        }
    };

    if (job_df.JobFinished()) {

        my_data->segy_writer.finished();

        if(my_data->is_success) {
            gd_logger.LogInfo(my_logger, "Output SEGY dataset: {}", my_data->url);
        } else {
            gd_logger.LogError(my_logger, "SEGY output failed!");
        }

        _clean_up();
        return;
    }

    try {        
        int grp_size = job_df.GetGroupSize();

        std::string data_name = job_df.GetVolumeDataName();

        //write data: primary, secondary, trace and attributes
        for(int i = 0; i < job_df.GetNumAttributes(); i++) {

            std::string attr_name = job_df.GetAttributeName(i);

            char *data = static_cast<char*>(job_df.GetWritableBuffer(attr_name.c_str()));
            int bytesize = my_data->segy_writer.getTraceByteSize();
            if(data == nullptr) {
                throw std::runtime_error("DF returned a nullptr to the buffer of attribute: " + attr_name);
            }

            SEGY::HeaderField field = my_data->segy_writer.getTraceField(attr_name);

            if (field.defined()) {
                bytesize =  field.fieldWidth;
            }
            printf("attr %s, %d %d\n", attr_name.c_str(), bytesize, field.byteLocation);

            for(int j = my_data->fskey; j <= my_data->lskey; j+=my_data->skinc) {
                if(attr_name == data_name) {
                    if(!my_data->segy_writer.writeTraceData(my_data->current_pkey, j, data)) {
                        throw std::runtime_error("Error: write trace, primary: " + std::to_string(i)
                            + ", secondary: "+ std::to_string(j) + ", error:"  + my_data->segy_writer.getErrMsg());
                    }
                } else {
                    if(!my_data->segy_writer.writeTraceHeader(my_data->current_pkey, j, data, field.byteLocation, field.fieldWidth)) {
                        throw std::runtime_error("Error: write trace, primary: " + std::to_string(i) 
                            + ", secondary: "+ std::to_string(j) + ", error:"  + my_data->segy_writer.getErrMsg());
                    }
                }
                data += bytesize;
            }
        }
        //file.close();      
    } catch (const std::exception& e) {
        my_data->is_success = false;
        gd_logger.LogError(my_logger, "Exception in segyoutput_process: {}", e.what());
        job_df.SetJobAborted();
        _clean_up();
        return;
    }
    // prepare for the next call
    my_data->current_pkey = my_data->current_pkey + my_data->pkinc;
}