#pragma once
#include <string>
#include <vector>
#include <functional>
#include <typeinfo>
#include <any>
#include <memory>

using Arg = std::any;
using Args = std::vector<Arg>;

// Holds information about a data member
class MemberInfo
{
public:
    std::string name;
    std::string type_name;
    std::function<Arg(const void *)> getter;
    std::function<void(void *, const Arg &)> setter;

    MemberInfo(const std::string &n, const std::string &t,
               std::function<Arg(const void *)> g,
               std::function<void(void *, const Arg &)> s);
};

// Holds information about a method
class MethodInfo
{
public:
    std::string name;
    std::string return_type;
    std::vector<std::string> parameter_types;
    std::function<Arg(void *, const Args &)> invoker;

    MethodInfo(const std::string &n, const std::string &ret_type,
               const std::vector<std::string> &param_types,
               std::function<Arg(void *, const Args &)> inv);
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