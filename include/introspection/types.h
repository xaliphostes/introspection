#pragma once
#include <introspection/info.h>

// Template helpers for type name extraction
template <typename T>
std::string getTypeName();

// Template-based registration system
template <typename Class>
class TypeRegistrar
{
private:
    TypeInfo &info;

public:
    explicit TypeRegistrar(TypeInfo &type_info) : info(type_info) {}

    // Member registration
    template <typename MemberType>
    TypeRegistrar &member(const std::string &name, MemberType Class::*member_ptr);

    // Method registration for methods with no parameters
    template <typename ReturnType>
    TypeRegistrar &method(const std::string &name, ReturnType (Class::*method_ptr)());

    // Method registration for const methods with no parameters
    template <typename ReturnType>
    TypeRegistrar &method(const std::string &name, ReturnType (Class::*method_ptr)() const);

    // Method registration for methods with 1 parameter
    template <typename ReturnType, typename Param1>
    TypeRegistrar &method(const std::string &name, ReturnType (Class::*method_ptr)(Param1));

    // Method registration for methods with 2 parameters
    template <typename ReturnType, typename Param1, typename Param2>
    TypeRegistrar &method(const std::string &name, ReturnType (Class::*method_ptr)(Param1, Param2));

    // Method registration for methods with 3 parameters
    template <typename ReturnType, typename Param1, typename Param2, typename Param3>
    TypeRegistrar &method(const std::string &name, ReturnType (Class::*method_ptr)(Param1, Param2, Param3));
};

#include "inline/types.hxx"