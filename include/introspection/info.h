#pragma once
#include <string>
#include <vector>
#include <functional>
#include <typeinfo>
#include <any>
#include <memory>

// Holds information about a data member
class MemberInfo
{
public:
    std::string name;
    std::string type_name;
    std::function<std::any(const void *)> getter;
    std::function<void(void *, const std::any &)> setter;

    MemberInfo(const std::string &n, const std::string &t,
               std::function<std::any(const void *)> g,
               std::function<void(void *, const std::any &)> s);
};

// Holds information about a method
class MethodInfo
{
public:
    std::string name;
    std::string return_type;
    std::vector<std::string> parameter_types;
    std::function<std::any(void *, const std::vector<std::any> &)> invoker;

    MethodInfo(const std::string &n, const std::string &ret_type,
               const std::vector<std::string> &param_types,
               std::function<std::any(void *, const std::vector<std::any> &)> inv);
};

// Holds complete type information
class TypeInfo
{
public:
    std::string class_name;
    std::unordered_map<std::string, std::unique_ptr<MemberInfo>> members;
    std::unordered_map<std::string, std::unique_ptr<MethodInfo>> methods;

    explicit TypeInfo(const std::string &name) : class_name(name) {}

    void addMember(std::unique_ptr<MemberInfo> member);
    void addMethod(std::unique_ptr<MethodInfo> method);
    const MemberInfo *getMember(const std::string &name) const;
    const MethodInfo *getMethod(const std::string &name) const;
    std::vector<std::string> getMemberNames() const;
    std::vector<std::string> getMethodNames() const;
};

#include "inline/info.hxx"