// 方案2：基于函数对象的轻量级方案
#include "testexpect.h"
#include <functional>
#include <unordered_map>

// 检查函数签名
using CheckFunction = std::function<bool(Testexpect*, const std::string&, AttrData&, std::map<std::string, AttrData>&)>;

// 检查器注册表
class CheckerRegistry {
public:
    static CheckerRegistry& instance() {
        static CheckerRegistry registry;
        return registry;
    }
    
    void registerChecker(const std::string& name, CheckFunction checker) {
        checkers_[name] = checker;
    }
    
    CheckFunction getChecker(const std::string& name) {
        auto it = checkers_.find(name);
        if (it != checkers_.end()) {
            return it->second;
        }
        return nullptr;
    }
    
    void initializeDefaultCheckers() {
        // 注册现有检查器
        registerChecker("SAME", [](Testexpect* my_data, const std::string& attr_name, 
                                  AttrData& attr_data, std::map<std::string, AttrData>& variables) {
            return check_data_same(my_data, const_cast<std::string&>(attr_name), attr_data, variables);
        });
        
        registerChecker("INLINE+CROSSLINE*2.7", [](Testexpect* my_data, const std::string& attr_name, 
                                                   AttrData& attr_data, std::map<std::string, AttrData>& variables) {
            return check_data_plus_mul(my_data, const_cast<std::string&>(attr_name), attr_data, variables);
        });
        
        // 新的检查器示例
        registerChecker("NON_ZERO", [](Testexpect* my_data, const std::string& attr_name, 
                                      AttrData& attr_data, std::map<std::string, AttrData>& variables) {
            float* data = static_cast<float*>(attr_data.data);
            int length = attr_data.length * my_data->group_size;
            for(int i = 0; i < length; ++i) {
                if(data[i] == 0.0f) return false;
            }
            return true;
        });
        
        registerChecker("POSITIVE", [](Testexpect* my_data, const std::string& attr_name, 
                                      AttrData& attr_data, std::map<std::string, AttrData>& variables) {
            float* data = static_cast<float*>(attr_data.data);
            int length = attr_data.length * my_data->group_size;
            for(int i = 0; i < length; ++i) {
                if(data[i] <= 0.0f) return false;
            }
            return true;
        });
    }
    
private:
    std::unordered_map<std::string, CheckFunction> checkers_;
};

// 更新后的检查函数
bool check_data_v2(Testexpect* my_data, const std::string& attr_name, 
                   AttrData& attr_data, std::map<std::string, AttrData>& variables, 
                   const std::string& pattern_name) {
    auto checker = CheckerRegistry::instance().getChecker(pattern_name);
    if (!checker) {
        throw std::runtime_error("Unknown check pattern: " + pattern_name);
    }
    
    return checker(my_data, attr_name, attr_data, variables);
}

// 支持参数化检查器的工厂函数
class ParametricCheckerFactory {
public:
    static CheckFunction createRangeChecker(double min_val, double max_val) {
        return [min_val, max_val](Testexpect* my_data, const std::string& attr_name, 
                                 AttrData& attr_data, std::map<std::string, AttrData>& variables) {
            float* data = static_cast<float*>(attr_data.data);
            int length = attr_data.length * my_data->group_size;
            for(int i = 0; i < length; ++i) {
                if(data[i] < min_val || data[i] > max_val) return false;
            }
            return true;
        };
    }
    
    static CheckFunction createFormulaChecker(const std::string& formula) {
        // 可以实现简单的公式解析器
        return [formula](Testexpect* my_data, const std::string& attr_name, 
                        AttrData& attr_data, std::map<std::string, AttrData>& variables) {
            // 根据formula字符串执行相应的检查逻辑
            // 这里可以集成一个简单的表达式解析器
            return true; // 占位符
        };
    }
};