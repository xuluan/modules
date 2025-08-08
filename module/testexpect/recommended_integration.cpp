// 推荐的集成方案：结合多种方法的优点
#include "testexpect.h"
#include "dynamic_value.h"
#include <memory>
#include <unordered_map>
#include <functional>

// 1. 检查器接口
class IDataChecker {
public:
    virtual ~IDataChecker() = default;
    virtual bool check(Testexpect* my_data, const std::string& attr_name, 
                      AttrData& attr_data, std::map<std::string, AttrData>& variables) = 0;
    virtual std::string describe() const = 0;
    virtual void configure(const DynamicValue& config) {}
};

// 2. 检查器管理器
class CheckerManager {
public:
    using CheckerFactory = std::function<std::unique_ptr<IDataChecker>(const DynamicValue&)>;
    
    static CheckerManager& instance() {
        static CheckerManager manager;
        return manager;
    }
    
    void registerCheckerType(const std::string& type_name, CheckerFactory factory) {
        factories_[type_name] = factory;
    }
    
    std::unique_ptr<IDataChecker> createChecker(const std::string& pattern_string) {
        // 首先尝试从缓存获取
        auto cached = checker_cache_.find(pattern_string);
        if (cached != checker_cache_.end()) {
            return cloneChecker(cached->second.get());
        }
        
        // 解析配置并创建新的检查器
        DynamicValue config = parsePatternString(pattern_string);
        std::string type = config.get("type", DynamicValue("")).as_string();
        
        auto factory_it = factories_.find(type);
        if (factory_it == factories_.end()) {
            throw std::runtime_error("Unknown checker type: " + type);
        }
        
        auto checker = factory_it->second(config);
        
        // 缓存检查器
        checker_cache_[pattern_string] = cloneChecker(checker.get());
        
        return checker;
    }
    
    void initializeBuiltinCheckers() {
        // 注册内置检查器
        registerCheckerType("same", [](const DynamicValue& config) {
            return std::make_unique<SameChecker>();
        });
        
        registerCheckerType("formula", [](const DynamicValue& config) {
            auto checker = std::make_unique<FormulaChecker>();
            checker->configure(config);
            return checker;
        });
        
        registerCheckerType("range", [](const DynamicValue& config) {
            auto checker = std::make_unique<RangeChecker>();
            checker->configure(config);
            return checker;
        });
        
        registerCheckerType("composite", [](const DynamicValue& config) {
            auto checker = std::make_unique<CompositeChecker>();
            checker->configure(config);
            return checker;
        });
    }
    
private:
    std::unordered_map<std::string, CheckerFactory> factories_;
    std::unordered_map<std::string, std::unique_ptr<IDataChecker>> checker_cache_;
    
    // 简化的模式字符串解析
    DynamicValue parsePatternString(const std::string& pattern) {
        DynamicValue config = DynamicValue::make_map();
        
        if (pattern == "SAME") {
            config["type"] = "same";
        } else if (pattern == "INLINE+CROSSLINE*2.7") {
            config["type"] = "formula";
            config["expression"] = pattern;
        } else if (pattern.find("RANGE[") == 0) {
            // 解析 RANGE[min,max] 格式
            config["type"] = "range";
            // TODO: 实际的解析逻辑
        } else {
            // 尝试作为YAML/JSON配置解析
            try {
                config = yaml::parse(pattern);
            } catch (...) {
                throw std::runtime_error("Invalid pattern format: " + pattern);
            }
        }
        
        return config;
    }
    
    std::unique_ptr<IDataChecker> cloneChecker(IDataChecker* original) {
        // 简化的克隆实现，实际需要更完善的深拷贝
        // 这里可以使用原型模式或序列化方式
        return nullptr; // 占位符
    }
};

// 3. 具体检查器实现
class SameChecker : public IDataChecker {
public:
    bool check(Testexpect* my_data, const std::string& attr_name, 
              AttrData& attr_data, std::map<std::string, AttrData>& variables) override {
        return check_data_same(my_data, const_cast<std::string&>(attr_name), attr_data, variables);
    }
    
    std::string describe() const override { return "SAME"; }
};

class FormulaChecker : public IDataChecker {
public:
    bool check(Testexpect* my_data, const std::string& attr_name, 
              AttrData& attr_data, std::map<std::string, AttrData>& variables) override {
        if (expression_ == "INLINE+CROSSLINE*2.7") {
            return check_data_plus_mul(my_data, const_cast<std::string&>(attr_name), attr_data, variables);
        }
        // 可以扩展支持更多公式
        return false;
    }
    
    void configure(const DynamicValue& config) override {
        expression_ = config.get("expression", DynamicValue("")).as_string();
    }
    
    std::string describe() const override { return "FORMULA[" + expression_ + "]"; }
    
private:
    std::string expression_;
};

class RangeChecker : public IDataChecker {
public:
    bool check(Testexpect* my_data, const std::string& attr_name, 
              AttrData& attr_data, std::map<std::string, AttrData>& variables) override {
        float* data = static_cast<float*>(attr_data.data);
        int length = attr_data.length * my_data->group_size;
        
        for(int i = 0; i < length; ++i) {
            if(data[i] < min_value_ || data[i] > max_value_) {
                return false;
            }
        }
        return true;
    }
    
    void configure(const DynamicValue& config) override {
        min_value_ = config.get("min", DynamicValue(-1e6)).as_double();
        max_value_ = config.get("max", DynamicValue(1e6)).as_double();
    }
    
    std::string describe() const override {
        return "RANGE[" + std::to_string(min_value_) + "," + std::to_string(max_value_) + "]";
    }
    
private:
    double min_value_ = -1e6;
    double max_value_ = 1e6;
};

class CompositeChecker : public IDataChecker {
public:
    bool check(Testexpect* my_data, const std::string& attr_name, 
              AttrData& attr_data, std::map<std::string, AttrData>& variables) override {
        if (checkers_.empty()) return true;
        
        if (logic_op_ == "AND") {
            for (const auto& checker : checkers_) {
                if (!checker->check(my_data, attr_name, attr_data, variables)) {
                    return false;
                }
            }
            return true;
        } else if (logic_op_ == "OR") {
            for (const auto& checker : checkers_) {
                if (checker->check(my_data, attr_name, attr_data, variables)) {
                    return true;
                }
            }
            return false;
        }
        return false;
    }
    
    void configure(const DynamicValue& config) override {
        logic_op_ = config.get("operator", DynamicValue("AND")).as_string();
        
        const auto& checkers_config = config.get("checkers", DynamicValue::make_array()).as_array();
        for (const auto& checker_config : checkers_config) {
            auto checker = CheckerManager::instance().createChecker(yaml::dump(checker_config));
            if (checker) {
                checkers_.push_back(std::move(checker));
            }
        }
    }
    
    std::string describe() const override {
        std::string result = "(";
        for (size_t i = 0; i < checkers_.size(); ++i) {
            if (i > 0) result += " " + logic_op_ + " ";
            result += checkers_[i]->describe();
        }
        result += ")";
        return result;
    }
    
private:
    std::string logic_op_ = "AND";
    std::vector<std::unique_ptr<IDataChecker>> checkers_;
};

// 4. 更新的主检查函数
bool check_data_new(Testexpect* my_data, const std::string& attr_name, 
                    AttrData& attr_data, std::map<std::string, AttrData>& variables, 
                    const std::string& pattern_string) {
    try {
        auto checker = CheckerManager::instance().createChecker(pattern_string);
        return checker->check(my_data, attr_name, attr_data, variables);
    } catch (const std::exception& e) {
        throw std::runtime_error("Check failed for pattern '" + pattern_string + "': " + e.what());
    }
}

// 5. 在testexpect.h中更新CheckPattern
// 可以完全移除enum CheckPattern，改用字符串配置