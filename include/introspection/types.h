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

    // Variadic method registration - handles any number of parameters
    template <typename ReturnType, typename... Args>
    TypeRegistrar &method(const std::string &name, ReturnType (Class::*method_ptr)(Args...));

    // Variadic const method registration
    template <typename ReturnType, typename... Args>
    TypeRegistrar &method(const std::string &name, ReturnType (Class::*method_ptr)(Args...) const);
};

#include "inline/types.hxx"