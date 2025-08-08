// 方案1：策略模式 + 工厂模式
#include "testexpect.h"
#include <memory>
#include <unordered_map>
#include <functional>

// 抽象检查策略接口
class DataChecker {
public:
    virtual ~DataChecker() = default;
    virtual bool check(Testexpect* my_data, const std::string& attr_name, 
                      AttrData& attr_data, std::map<std::string, AttrData>& variables) = 0;
    virtual std::string name() const = 0;
};

// 具体检查策略实现
class SameChecker : public DataChecker {
public:
    bool check(Testexpect* my_data, const std::string& attr_name, 
              AttrData& attr_data, std::map<std::string, AttrData>& variables) override {
        // 原有的 check_data_same 逻辑
        return check_data_same(my_data, const_cast<std::string&>(attr_name), attr_data, variables);
    }
    
    std::string name() const override { return "SAME"; }
};

class AttrPlusMulChecker : public DataChecker {
public:
    bool check(Testexpect* my_data, const std::string& attr_name, 
              AttrData& attr_data, std::map<std::string, AttrData>& variables) override {
        // 原有的 check_data_plus_mul 逻辑
        return check_data_plus_mul(my_data, const_cast<std::string&>(attr_name), attr_data, variables);
    }
    
    std::string name() const override { return "INLINE+CROSSLINE*2.7"; }
};

// 新的检查器示例
class RangeChecker : public DataChecker {
public:
    RangeChecker(double min_val, double max_val) : min_value(min_val), max_value(max_val) {}
    
    bool check(Testexpect* my_data, const std::string& attr_name, 
              AttrData& attr_data, std::map<std::string, AttrData>& variables) override {
        // 检查数据是否在指定范围内
        float* data = static_cast<float*>(attr_data.data);
        int length = attr_data.length * my_data->group_size;
        
        for(int i = 0; i < length; ++i) {
            if(data[i] < min_value || data[i] > max_value) {
                return false;
            }
        }
        return true;
    }
    
    std::string name() const override { 
        return "RANGE[" + std::to_string(min_value) + "," + std::to_string(max_value) + "]"; 
    }
    
private:
    double min_value, max_value;
};

// 检查器工厂
class CheckerFactory {
public:
    using CheckerCreator = std::function<std::unique_ptr<DataChecker>()>;
    
    static CheckerFactory& instance() {
        static CheckerFactory factory;
        return factory;
    }
    
    void registerChecker(const std::string& name, CheckerCreator creator) {
        creators_[name] = creator;
    }
    
    std::unique_ptr<DataChecker> createChecker(const std::string& name) {
        auto it = creators_.find(name);
        if (it != creators_.end()) {
            return it->second();
        }
        return nullptr;
    }
    
    void initializeDefaultCheckers() {
        registerChecker("SAME", []() { return std::make_unique<SameChecker>(); });
        registerChecker("INLINE+CROSSLINE*2.7", []() { return std::make_unique<AttrPlusMulChecker>(); });
        // 可以继续注册其他检查器
    }
    
private:
    std::unordered_map<std::string, CheckerCreator> creators_;
};

// 更新后的检查函数
bool check_data_v2(Testexpect* my_data, const std::string& attr_name, 
                   AttrData& attr_data, std::map<std::string, AttrData>& variables, 
                   const std::string& pattern_name) {
    auto checker = CheckerFactory::instance().createChecker(pattern_name);
    if (!checker) {
        throw std::runtime_error("Unknown check pattern: " + pattern_name);
    }
    
    return checker->check(my_data, attr_name, attr_data, variables);
}